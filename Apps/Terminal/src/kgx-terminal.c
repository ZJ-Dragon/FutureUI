/* kgx-terminal.c
 *
 * Copyright 2019 Zander Brown
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:kgx-terminal
 * @title: KgxTerminal
 * @short_description: The terminal
 *
 * The main terminal widget with various features added such as a context
 * menu (via #GtkPopover) and link detection
 */

#include "kgx-config.h"

#include <glib/gi18n.h>
#include <adwaita.h>
#include <vte/vte.h>
#define PCRE2_CODE_UNIT_WIDTH 0
#include <pcre2.h>

#include "rgba.h"
#include "xdg-fm1.h"

#include "kgx-terminal.h"
#include "kgx-settings.h"
#include "kgx-marshals.h"

/*       Regex adapted from TerminalWidget.vala in Pantheon Terminal       */

#define USERCHARS "-[:alnum:]"
#define USERCHARS_CLASS "[" USERCHARS "]"
#define PASSCHARS_CLASS "[-[:alnum:]\\Q,?;.:/!%$^*&~\"#'\\E]"
#define HOSTCHARS_CLASS "[-[:alnum:]]"
#define HOST HOSTCHARS_CLASS "+(\\." HOSTCHARS_CLASS "+)*"
#define PORT "(?:\\:[[:digit:]]{1,5})?"
#define PATHCHARS_CLASS "[-[:alnum:]\\Q_$.+!*,;:@&=?/~#%\\E]"
#define PATHTERM_CLASS "[^\\Q]'.}>) \t\r\n,\"\\E]"
#define SCHEME "(?:news:|telnet:|nntp:|file:\\/|https?:|ftps?:|sftp:|webcal:\n" \
               "|irc:|sftp:|ldaps?:|nfs:|smb:|rsync:|"                          \
               "ssh:|rlogin:|telnet:|git:\n"                                    \
               "|git\\+ssh:|bzr:|bzr\\+ssh:|svn:|svn\\"                         \
               "+ssh:|hg:|mailto:|magnet:)"
#define USERPASS USERCHARS_CLASS "+(?:" PASSCHARS_CLASS "+)?"
#define URLPATH "(?:(/" PATHCHARS_CLASS "+(?:[(]" PATHCHARS_CLASS "*[)])*" PATHCHARS_CLASS "*)*" PATHTERM_CLASS ")?"

#define KGX_TERMINAL_N_LINK_REGEX 5

static const char *links[KGX_TERMINAL_N_LINK_REGEX] = {
  SCHEME "//(?:" USERPASS "\\@)?" HOST PORT URLPATH,
  "(?:www|ftp)" HOSTCHARS_CLASS "*\\." HOST PORT URLPATH,
  "(?:callto:|h323:|sip:)" USERCHARS_CLASS "[" USERCHARS ".]*(?:" PORT "/[a-z0-9]+)?\\@" HOST,
  "(?:mailto:)?" USERCHARS_CLASS "[" USERCHARS ".]*\\@" HOSTCHARS_CLASS "+\\." HOST,
  "(?:news:|man:|info:)[-[:alnum:]\\Q^_{|}~!\"#$%&'()*+,./;:=?`\\E]+"
};

/*       Regex adapted from TerminalWidget.vala in Pantheon Terminal       */

/**
 * KgxTerminal:
 * @theme: the palette to use, see #KgxTerminal:theme
 * @actions: action map for the context menu
 * @current_url: the address under the cursor
 * @match_id: regex ids for finding hyperlinks
 *
 * Stability: Private
 */
struct _KgxTerminal {
  VteTerminal parent_instance;

  KgxSettings   *settings;
  GSignalGroup  *settings_signals;
  GBindingGroup *settings_binds;
  GtkWidget  *popup_menu;

  /* Hyperlinks */
  char       *current_url;
  int         match_id[KGX_TERMINAL_N_LINK_REGEX];

  gboolean    popup_is_touch;
};


G_DEFINE_TYPE (KgxTerminal, kgx_terminal, VTE_TYPE_TERMINAL)

enum {
  PROP_0,
  PROP_SETTINGS,
  PROP_PATH,
  LAST_PROP
};

static GParamSpec *pspecs[LAST_PROP] = { NULL, };

enum {
  SIZE_CHANGED,
  N_SIGNALS
};
static guint signals[N_SIGNALS];


static void
kgx_terminal_dispose (GObject *object)
{
  KgxTerminal *self = KGX_TERMINAL (object);

  g_clear_pointer (&self->current_url, g_free);
  g_clear_pointer (&self->popup_menu, gtk_widget_unparent);

  g_clear_object (&self->settings);
  g_clear_object (&self->settings_signals);
  g_clear_object (&self->settings_binds);

  G_OBJECT_CLASS (kgx_terminal_parent_class)->dispose (object);
}


static void
update_terminal_colours (KgxTerminal *self)
{
  KgxTheme current_theme;
  KgxTheme resolved_theme;
  GdkRGBA fg;
  GdkRGBA bg;

  // Workings of GDK_RGBA prevent this being static
  GdkRGBA palette[16] = {
    GDK_RGBA ("241f31"), // Black
    GDK_RGBA ("c01c28"), // Red
    GDK_RGBA ("2ec27e"), // Green
    GDK_RGBA ("f5c211"), // Yellow
    GDK_RGBA ("1e78e4"), // Blue
    GDK_RGBA ("9841bb"), // Magenta
    GDK_RGBA ("0ab9dc"), // Cyan
    GDK_RGBA ("c0bfbc"), // White
    GDK_RGBA ("5e5c64"), // Bright Black
    GDK_RGBA ("ed333b"), // Bright Red
    GDK_RGBA ("57e389"), // Bright Green
    GDK_RGBA ("f8e45c"), // Bright Yellow
    GDK_RGBA ("51a1ff"), // Bright Blue
    GDK_RGBA ("c061cb"), // Bright Magenta
    GDK_RGBA ("4fd2fd"), // Bright Cyan
    GDK_RGBA ("f6f5f4"), // Bright White
  };

  if (!self->settings) {
    return;
  }

  g_object_get (self->settings, "theme", &current_theme, NULL);

  if (current_theme == KGX_THEME_AUTO) {
    AdwStyleManager *manager = adw_style_manager_get_default ();

    if (adw_style_manager_get_dark (manager)) {
      resolved_theme = KGX_THEME_NIGHT;
    } else {
      resolved_theme = KGX_THEME_DAY;
    }
  } else {
    resolved_theme = current_theme;
  }

  switch (resolved_theme) {
    case KGX_THEME_HACKER:
      fg = (GdkRGBA) { 0.1, 1.0, 0.1, 1.0};
      bg = (GdkRGBA) { 0.05, 0.05, 0.05, 1.0 };
      break;
    case KGX_THEME_DAY:
      fg = (GdkRGBA) { 0.0, 0.0, 0.0, 0.0 };
      bg = (GdkRGBA) { 1.0, 1.0, 1.0, 1.0 };
      break;
    case KGX_THEME_NIGHT:
    case KGX_THEME_AUTO:
    default:
      fg = (GdkRGBA) { 1.0, 1.0, 1.0, 1.0};
      bg = (GdkRGBA) { 0.12, 0.12, 0.12, 1.0 };
      break;
  }

  vte_terminal_set_colors (VTE_TERMINAL (self), &fg, &bg, palette, 16);
}


static void
kgx_terminal_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  KgxTerminal *self = KGX_TERMINAL (object);

  switch (property_id) {
    case PROP_SETTINGS:
      g_set_object (&self->settings, g_value_get_object (value));
      update_terminal_colours (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}


static void
kgx_terminal_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  KgxTerminal *self = KGX_TERMINAL (object);
  const char *uri;
  g_autoptr (GFile) path = NULL;

  switch (property_id) {
    case PROP_SETTINGS:
      g_value_set_object (value, self->settings);
      break;
    case PROP_PATH:
      if ((uri = vte_terminal_get_current_file_uri (VTE_TERMINAL (self)))) {
        path = g_file_new_for_uri (uri);
      } else if ((uri = vte_terminal_get_current_directory_uri (VTE_TERMINAL (self)))) {
        path = g_file_new_for_uri (uri);
      }
      g_value_set_object (value, path);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}


static gboolean
have_url_under_pointer (KgxTerminal *self,
                        double       x,
                        double       y)
{
  g_autofree char *hyperlink = NULL;
  g_autofree char *match = NULL;
  int match_id;

  g_clear_pointer (&self->current_url, g_free);

  hyperlink = vte_terminal_check_hyperlink_at (VTE_TERMINAL (self), x, y);

  if (G_UNLIKELY (hyperlink)) {
    self->current_url = g_steal_pointer (&hyperlink);
  } else {
    match = vte_terminal_check_match_at (VTE_TERMINAL (self), x, y, &match_id);

    for (int i = 0; i < KGX_TERMINAL_N_LINK_REGEX; i++) {
      if (self->match_id[i] == match_id) {
        self->current_url = g_steal_pointer (&match);
        break;
      }
    }
  }

  return self->current_url != NULL;
}


static void
update_menu_position (KgxTerminal *self)
{
  if (!self->popup_menu)
    return;

  gtk_popover_set_position (GTK_POPOVER (self->popup_menu),
                            self->popup_is_touch ? GTK_POS_TOP : GTK_POS_BOTTOM);

  gtk_popover_set_has_arrow (GTK_POPOVER (self->popup_menu), self->popup_is_touch);

  if (self->popup_is_touch) {
    gtk_widget_set_halign (self->popup_menu, GTK_ALIGN_FILL);
  } else if (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL) {
    gtk_widget_set_halign (self->popup_menu, GTK_ALIGN_END);
  } else {
    gtk_widget_set_halign (self->popup_menu, GTK_ALIGN_START);
  }
}


static void
context_menu (KgxTerminal *self,
              double       x,
              double       y,
              gboolean     touch)
{
  GtkApplication *app;
  gboolean value;

  value = have_url_under_pointer (self, x, y);

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "term.open-link", value);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "term.copy-link", value);

  app = GTK_APPLICATION (g_application_get_default ());

  if (!self->popup_menu) {
    GMenu *model = gtk_application_get_menu_by_id (app, "context-menu");

    self->popup_menu = gtk_popover_menu_new_from_model (G_MENU_MODEL (model));

    gtk_widget_set_parent (self->popup_menu, GTK_WIDGET (self));
  }

  self->popup_is_touch = touch;

  update_menu_position (self);

  if (x > -1 && y > -1) {
    GdkRectangle rect = { x, y, 1, 1 };
    gtk_popover_set_pointing_to (GTK_POPOVER (self->popup_menu), &rect);
  } else {
    gtk_popover_set_pointing_to (GTK_POPOVER (self->popup_menu), NULL);
  }

  gtk_popover_popup (GTK_POPOVER (self->popup_menu));
}


static void
menu_popup_activated (KgxTerminal *self)
{
  context_menu (self, 1, 1, FALSE);
}


static void
open_link_cb (GtkUriLauncher *launcher,
              GAsyncResult   *result)
{
  g_autoptr (GError) error = NULL;

  if (!gtk_uri_launcher_launch_finish (launcher, result, &error))
    g_warning ("Couldn't open uri: %s\n", error->message);
}


static void
open_link (KgxTerminal *self)
{
  GtkUriLauncher *launcher;

  launcher = gtk_uri_launcher_new (self->current_url);
  gtk_uri_launcher_launch (launcher,
                           GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self))),
                           NULL,
                           (GAsyncReadyCallback) open_link_cb,
                           NULL);
}


static void
open_link_activated (KgxTerminal *self)
{
  open_link (self);
}


static void
copy_link_activated (KgxTerminal *self)
{
  GdkClipboard *cb = gtk_widget_get_clipboard (GTK_WIDGET (self));

  gdk_clipboard_set_text (cb, self->current_url);
}

static void
copy_activated (KgxTerminal *self)
{
  GdkClipboard *clipboard = gtk_widget_get_clipboard (GTK_WIDGET (self));
  g_autofree char *text = vte_terminal_get_text_selected (VTE_TERMINAL (self),
                                                          VTE_FORMAT_TEXT);
  gdk_clipboard_set_text (clipboard, text);
}


static void
got_text (GdkClipboard *cb,
          GAsyncResult *result,
          KgxTerminal  *self)
{
  g_autofree char *text = NULL;
  g_autoptr (GError) error = NULL;

  /* Get the resulting text of the read operation */
  text = gdk_clipboard_read_text_finish (cb, result, &error);

  if (error) {
    g_critical ("Couldn't paste text: %s\n", error->message);
    return;
  }

  kgx_terminal_accept_paste (self, text);
}


static void
paste_activated (KgxTerminal *self)
{
  GdkClipboard *cb = gtk_widget_get_clipboard (GTK_WIDGET (self));

  gdk_clipboard_read_text_async (cb, NULL, (GAsyncReadyCallback) got_text, self);
}


static void
select_all_activated (KgxTerminal *self)
{
  vte_terminal_select_all (VTE_TERMINAL (self));
}


typedef struct {
  GStrv     uris;
  gboolean  show_folders;
} ShowData;


static void
clear_show_data (gpointer data)
{
  ShowData *self = data;

  g_clear_pointer (&self->uris, g_strfreev);
  g_free (self);
}


G_DEFINE_AUTOPTR_CLEANUP_FUNC (ShowData, clear_show_data)


static void
complete_call (GObject *source, GAsyncResult *res, gpointer data)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (ShowData) show = data;

  if (show->show_folders) {
    xdg_file_manager1_call_show_folders_finish (XDG_FILE_MANAGER1 (source),
                                                res,
                                                &error);
  } else {
    xdg_file_manager1_call_show_items_finish (XDG_FILE_MANAGER1 (source),
                                              res,
                                              &error);
  }

  if (error) {
    g_warning ("term.show-in-files: D-Bus call failed %s", error->message);
  }
}


static void
got_proxy (GObject *source, GAsyncResult *res, gpointer data)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (XdgFileManager1) fm = NULL;
  g_autoptr (ShowData) show = data;
  g_auto (GStrv) uris = g_steal_pointer (&show->uris);

  fm = xdg_file_manager1_proxy_new_finish (res, &error);

  if (error) {
    g_warning ("term.show-in-files: D-Bus connect failed %s", error->message);
    return;
  }

  if (show->show_folders) {
    xdg_file_manager1_call_show_folders (fm,
                                         (const char *const *) uris,
                                         "",
                                         NULL,
                                         complete_call,
                                         g_steal_pointer (&show));
  } else {
    xdg_file_manager1_call_show_items (fm,
                                       (const char *const *) uris,
                                       "",
                                       NULL,
                                       complete_call,
                                       g_steal_pointer (&show));
  }
}


static void
show_in_files_activated (KgxTerminal *self)
{
  g_autoptr (ShowData) data = g_new0 (ShowData, 1);
  g_autoptr (GStrvBuilder) builder = g_strv_builder_new ();
  const char *uri = NULL;

  data->show_folders = FALSE;
  uri = vte_terminal_get_current_file_uri (VTE_TERMINAL (self));

  if (uri == NULL) {
    data->show_folders = TRUE;
    uri = vte_terminal_get_current_directory_uri (VTE_TERMINAL (self));
  }

  if (uri == NULL) {
    g_warning ("term.show-in-files: no file");
    return;
  }

  g_strv_builder_add (builder, uri);
  data->uris = g_strv_builder_end (builder);

  xdg_file_manager1_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                       G_DBUS_PROXY_FLAGS_NONE,
                                       "org.freedesktop.FileManager1",
                                       "/org/freedesktop/FileManager1",
                                       NULL,
                                       got_proxy,
                                       g_steal_pointer (&data));
}


static void
kgx_terminal_size_allocate (GtkWidget *widget,
                            int        width,
                            int        height,
                            int        baseline)
{
  int          rows;
  int          cols;
  KgxTerminal *self = KGX_TERMINAL (widget);
  VteTerminal *term = VTE_TERMINAL (self);

  if (self->popup_menu)
    gtk_popover_present (GTK_POPOVER (self->popup_menu));

  GTK_WIDGET_CLASS (kgx_terminal_parent_class)->size_allocate (widget, width, height, baseline);

  rows = vte_terminal_get_row_count (term);
  cols = vte_terminal_get_column_count (term);

  g_signal_emit (self, signals[SIZE_CHANGED], 0, rows, cols);
}


static void
kgx_terminal_direction_changed (GtkWidget        *widget,
                                GtkTextDirection  previous_direction)
{
  KgxTerminal *self = KGX_TERMINAL (widget);

  GTK_WIDGET_CLASS (kgx_terminal_parent_class)->direction_changed (widget, previous_direction);

  update_menu_position (self);
}


static void
kgx_terminal_class_init (KgxTerminalClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = kgx_terminal_dispose;
  object_class->set_property = kgx_terminal_set_property;
  object_class->get_property = kgx_terminal_get_property;

  widget_class->size_allocate = kgx_terminal_size_allocate;
  widget_class->direction_changed = kgx_terminal_direction_changed;

  pspecs[PROP_SETTINGS] =
    g_param_spec_object ("settings", NULL, NULL,
                         KGX_TYPE_SETTINGS,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  /**
   * KgxTerminal:path:
   *
   *
   * Stability: Private
   */
  pspecs[PROP_PATH] =
    g_param_spec_object ("path", "Path", "Current path",
                         G_TYPE_FILE,
                         G_PARAM_READABLE);

  g_object_class_install_properties (object_class, LAST_PROP, pspecs);

  signals[SIZE_CHANGED] = g_signal_new ("size-changed",
                                        G_TYPE_FROM_CLASS (klass),
                                        G_SIGNAL_RUN_LAST,
                                        0, NULL, NULL,
                                        kgx_marshals_VOID__UINT_UINT,
                                        G_TYPE_NONE,
                                        2,
                                        G_TYPE_UINT,
                                        G_TYPE_UINT);

  gtk_widget_class_install_action (widget_class,
                                   "menu.popup",
                                   NULL,
                                   (GtkWidgetActionActivateFunc) menu_popup_activated);
  gtk_widget_class_install_action (widget_class,
                                   "term.open-link",
                                   NULL,
                                   (GtkWidgetActionActivateFunc) open_link_activated);
  gtk_widget_class_install_action (widget_class,
                                   "term.copy-link",
                                   NULL,
                                   (GtkWidgetActionActivateFunc) copy_link_activated);
  gtk_widget_class_install_action (widget_class,
                                   "term.copy",
                                   NULL,
                                   (GtkWidgetActionActivateFunc) copy_activated);
  gtk_widget_class_install_action (widget_class,
                                   "term.paste",
                                   NULL,
                                   (GtkWidgetActionActivateFunc) paste_activated);
  gtk_widget_class_install_action (widget_class,
                                   "term.select-all",
                                   NULL,
                                   (GtkWidgetActionActivateFunc) select_all_activated);
  gtk_widget_class_install_action (widget_class,
                                   "term.show-in-files",
                                   NULL,
                                   (GtkWidgetActionActivateFunc) show_in_files_activated);

  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_F10, GDK_SHIFT_MASK,
                                       "menu.popup",
                                       NULL);
  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_Menu, 0,
                                       "menu.popup",
                                       NULL);
}


typedef struct {
  KgxTerminal *dest;
  char        *text;
} PasteData;


static void
clear_paste_data (gpointer data)
{
  PasteData *self = data;

  g_clear_weak_pointer (&self->dest);
  g_free (self->text);
  g_free (self);
}


G_DEFINE_AUTOPTR_CLEANUP_FUNC (PasteData, clear_paste_data)

static void
paste_response (PasteData *data)
{
  g_autoptr (PasteData) paste = data;

  vte_terminal_paste_text (VTE_TERMINAL (paste->dest), paste->text);
}


static void
pressed (GtkGestureClick *gesture,
         int              n_presses,
         double           x,
         double           y,
         KgxTerminal     *self)
{
  GdkEvent *event;
  GdkModifierType state;
  guint button;

  if (n_presses > 1) {
    gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
    return;
  }

  event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (gesture));

  if (gdk_event_triggers_context_menu (event)) {
    context_menu (self, x, y, FALSE);
    gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

    return;
  }

  state = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (gesture));
  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

  if (have_url_under_pointer (self, x, y) &&
      (button == GDK_BUTTON_PRIMARY || button == GDK_BUTTON_MIDDLE) &&
      state & GDK_CONTROL_MASK) {

    open_link (self);
    gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

    return;
  }

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
}


static void
long_pressed (GtkGestureLongPress *gesture,
              double               x,
              double               y,
              KgxTerminal         *self)
{
  context_menu (self, x, y, TRUE);
  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}


static void
selection_changed (KgxTerminal *self)
{
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "term.copy",
                                 vte_terminal_get_has_selection (VTE_TERMINAL (self)));
}


static void
location_changed (KgxTerminal *self)
{
  gboolean value;

  value = vte_terminal_get_current_file_uri (VTE_TERMINAL (self)) ||
            vte_terminal_get_current_directory_uri (VTE_TERMINAL (self));

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "term.show-in-files", value);

  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_PATH]);
}


static void
dark_changed (KgxTerminal *self)
{
  KgxTheme theme;

  g_object_get (self->settings, "theme", &theme, NULL);

  if (theme == KGX_THEME_AUTO) {
    update_terminal_colours (self);
  }
}

static gboolean
on_scroll (GtkEventControllerScroll *scroll,
           double                   dx,
           double                   dy,
           KgxTerminal              *self)
{
  GApplication *app = g_application_get_default ();
  GdkModifierType mods = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (scroll));
  GAction *action = NULL;

  if ((mods & GDK_CONTROL_MASK) == 0 || dy == 0) {
    return FALSE;
  }

  if (dy > 0) {
    action = g_action_map_lookup_action (G_ACTION_MAP (app), "zoom-out");
  } else {
    action = g_action_map_lookup_action (G_ACTION_MAP (app), "zoom-in");
  }

  g_action_activate (action, NULL);
  return TRUE;
}

static void
kgx_terminal_init (KgxTerminal *self)
{
  GtkGesture *gesture;
  GtkEventController *controller;
  GtkShortcut *shortcut;

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  g_signal_connect (gesture, "pressed", G_CALLBACK (pressed), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));

  gesture = gtk_gesture_long_press_new ();
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), TRUE);
  g_signal_connect (gesture, "pressed", G_CALLBACK (long_pressed), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));

  controller = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_VERTICAL | GTK_EVENT_CONTROLLER_SCROLL_DISCRETE);
  gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
  g_signal_connect (controller,
                    "scroll",
                    G_CALLBACK (on_scroll),
                    self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  controller = gtk_shortcut_controller_new ();
  gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  shortcut = gtk_shortcut_new (gtk_keyval_trigger_new (GDK_KEY_C, GDK_CONTROL_MASK | GDK_SHIFT_MASK),
                               gtk_named_action_new ("term.copy"));
  gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller), shortcut);

  shortcut = gtk_shortcut_new (gtk_keyval_trigger_new (GDK_KEY_V, GDK_CONTROL_MASK | GDK_SHIFT_MASK),
                               gtk_named_action_new ("term.paste"));
  gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller), shortcut);

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "term.open-link", FALSE);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "term.copy-link", FALSE);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "term.copy", FALSE);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "term.show-in-files", FALSE);

  vte_terminal_set_mouse_autohide (VTE_TERMINAL (self), TRUE);
  vte_terminal_search_set_wrap_around (VTE_TERMINAL (self), TRUE);
  vte_terminal_set_allow_hyperlink (VTE_TERMINAL (self), TRUE);
  vte_terminal_set_enable_fallback_scrolling (VTE_TERMINAL (self), FALSE);
  vte_terminal_set_scroll_unit_is_pixels (VTE_TERMINAL (self), TRUE);

  g_signal_connect (self, "selection-changed",
                    G_CALLBACK (selection_changed), NULL);
  g_signal_connect (self, "current-directory-uri-changed",
                    G_CALLBACK (location_changed), NULL);
  g_signal_connect (self, "current-file-uri-changed",
                    G_CALLBACK (location_changed), NULL);

  for (int i = 0; i < KGX_TERMINAL_N_LINK_REGEX; i++) {
    g_autoptr (VteRegex) regex = NULL;
    g_autoptr (GError) error = NULL;

    regex = vte_regex_new_for_match (links[i], -1, PCRE2_MULTILINE, &error);

    if (error) {
      g_warning ("link regex failed: %s", error->message);
      continue;
    }

    self->match_id[i] = vte_terminal_match_add_regex (VTE_TERMINAL (self),
                                                      regex,
                                                      0);

    vte_terminal_match_set_cursor_name (VTE_TERMINAL (self),
                                        self->match_id[i],
                                        "pointer");
  }

  g_signal_connect_object (adw_style_manager_get_default (),
                           "notify::dark", G_CALLBACK (dark_changed),
                           self, G_CONNECT_SWAPPED);

  self->settings_signals = g_signal_group_new (KGX_TYPE_SETTINGS);
  g_object_bind_property (self, "settings",
                          self->settings_signals, "target",
                          G_BINDING_DEFAULT);
  g_signal_group_connect_swapped (self->settings_signals,
                                  "notify::theme", G_CALLBACK (update_terminal_colours),
                                  self);

  self->settings_binds = g_binding_group_new ();
  g_object_bind_property (self, "settings",
                          self->settings_binds, "source",
                          G_BINDING_DEFAULT);
  g_binding_group_bind (self->settings_binds, "font",
                        self, "font-desc",
                        G_BINDING_SYNC_CREATE);
  g_binding_group_bind (self->settings_binds, "font-scale",
                        self, "font-scale",
                        G_BINDING_SYNC_CREATE);
  g_binding_group_bind (self->settings_binds, "scrollback-lines",
                        self, "scrollback-lines",
                        G_BINDING_SYNC_CREATE);
}


void
kgx_terminal_accept_paste (KgxTerminal *self,
                           const char  *text)
{
  g_autofree char *striped = g_strchug (g_strdup (text));
  g_autoptr (PasteData) paste = g_new0 (PasteData, 1);
  gsize len;

  if (!text || !text[0]) {
    return;
  }

  len = strlen (text);

  g_set_weak_pointer (&paste->dest, self);
  paste->text = g_strdup (text);

  if (g_strstr_len (striped, len, "sudo") != NULL &&
      g_strstr_len (striped, len, "\n") != NULL) {
    GtkWidget *dlg = adw_message_dialog_new (GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self))),
                                             _("You are pasting a command that runs as an administrator"),
                                             NULL);
    adw_message_dialog_format_body (ADW_MESSAGE_DIALOG (dlg),
                                    // TRANSLATORS: %s is the command being pasted
                                    _("Make sure you know what the command does:\n%s"),
                                    text);
    adw_message_dialog_add_responses (ADW_MESSAGE_DIALOG (dlg),
                                      "cancel", _("_Cancel"),
                                      "paste", _("_Paste"),
                                      NULL);
    adw_message_dialog_set_response_appearance (ADW_MESSAGE_DIALOG (dlg),
                                                "paste",
                                                ADW_RESPONSE_DESTRUCTIVE);

    g_signal_connect_swapped (dlg,
                              "response::paste",
                              G_CALLBACK (paste_response),
                              g_steal_pointer (&paste));

    gtk_window_present (GTK_WINDOW (dlg));
  } else {
    paste_response (g_steal_pointer (&paste));
  }
}
