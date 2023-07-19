/* nautilus-ui-utilities.c - helper functions for GtkUIManager stuff
 *
 *  Copyright (C) 2004 Red Hat, Inc.
 *
 *  The Gnome Library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  The Gnome Library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with the Gnome Library; see the file COPYING.LIB.  If not,
 *  see <http://www.gnu.org/licenses/>.
 *
 *  Authors: Alexander Larsson <alexl@redhat.com>
 */

#include <config.h>

#include "nautilus-ui-utilities.h"
#include "nautilus-icon-info.h"
#include "nautilus-application.h"

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <string.h>
#include <glib/gi18n.h>

/**
 * nautilus_gmenu_set_from_model:
 * @target_menu: the #GMenu to be filled
 * @source_model: (nullable): a #GMenuModel to copy items from
 *
 * This will replace the content of @target_menu with a copy of all items from
 * @source_model.
 *
 * If the @source_model is empty (i.e., its item count is 0), or if it is %NULL,
 * then the @target_menu is left empty.
 */
void
nautilus_gmenu_set_from_model (GMenu      *target_menu,
                               GMenuModel *source_model)
{
    g_return_if_fail (G_IS_MENU (target_menu));
    g_return_if_fail (source_model == NULL || G_IS_MENU_MODEL (source_model));

    /* First, empty the menu... */
    g_menu_remove_all (target_menu);

    /* ...then, repopulate it (maybe). */
    if (source_model != NULL)
    {
        gint n_items;

        n_items = g_menu_model_get_n_items (source_model);
        for (gint i = 0; i < n_items; i++)
        {
            g_autoptr (GMenuItem) item = NULL;
            item = g_menu_item_new_from_model (source_model, i);
            g_menu_append_item (target_menu, item);
        }
    }
}

/**
 * nautilus_g_menu_model_find_by_string:
 * @model: the #GMenuModel with items to search
 * @attribute: the menu item attribute to compare with
 * @string: the string to match the value of @attribute
 *
 * This will search for an item in the model which has got the @attribute and
 * whose value is equal to @string.
 *
 * It is assumed that @attribute has the a GVariant format string "s".
 *
 * Returns: The index of the first match in the model, or -1 if no item matches.
 */
gint
nautilus_g_menu_model_find_by_string (GMenuModel  *model,
                                      const gchar *attribute,
                                      const gchar *string)
{
    gint item_index = -1;
    gint n_items;

    n_items = g_menu_model_get_n_items (model);
    for (gint i = 0; i < n_items; i++)
    {
        g_autofree gchar *value = NULL;
        if (g_menu_model_get_item_attribute (model, i, attribute, "s", &value) &&
            g_strcmp0 (value, string) == 0)
        {
            item_index = i;
            break;
        }
    }
    return item_index;
}

/**
 * nautilus_g_menu_replace_string_in_item:
 * @menu: the #GMenu to modify
 * @i: the position of the item to change
 * @attribute: the menu item attribute to change
 * @string: the string to change the value of @attribute to
 *
 * This will replace the item at @position with a new item which is identical
 * except that it has @attribute set to @string.
 *
 * This is useful e.g. when want to change the menu model of a #GtkPopover and
 * you have a pointer to its menu model but not to the popover itself, so you
 * can't just set a new model. With this method, the GtkPopover is notified of
 * changes in its model and updates its contents accordingly.
 *
 * It is assumed that @attribute has the a GVariant format string "s".
 */
void
nautilus_g_menu_replace_string_in_item (GMenu       *menu,
                                        gint         i,
                                        const gchar *attribute,
                                        const gchar *string)
{
    g_autoptr (GMenuItem) item = NULL;

    g_return_if_fail (i != -1);
    item = g_menu_item_new_from_model (G_MENU_MODEL (menu), i);
    g_return_if_fail (item != NULL);

    if (string != NULL)
    {
        g_menu_item_set_attribute (item, attribute, "s", string);
    }
    else
    {
        g_menu_item_set_attribute (item, attribute, NULL);
    }

    g_menu_remove (menu, i);
    g_menu_insert_item (menu, i, item);
}

static GdkPixbuf *filmholes_left = NULL;
static GdkPixbuf *filmholes_right = NULL;

static gboolean
ensure_filmholes (void)
{
    if (filmholes_left == NULL)
    {
        filmholes_left = gdk_pixbuf_new_from_resource ("/org/gnome/nautilus/icons/filmholes.png", NULL);
    }
    if (filmholes_right == NULL &&
        filmholes_left != NULL)
    {
        filmholes_right = gdk_pixbuf_flip (filmholes_left, TRUE);
    }

    return (filmholes_left && filmholes_right);
}

void
nautilus_ui_frame_video (GtkSnapshot *snapshot,
                         gdouble      width,
                         gdouble      height)
{
    g_autoptr (GdkTexture) left_texture = NULL;
    g_autoptr (GdkTexture) right_texture = NULL;
    int holes_width, holes_height;

    if (!ensure_filmholes ())
    {
        return;
    }

    holes_width = gdk_pixbuf_get_width (filmholes_left);
    holes_height = gdk_pixbuf_get_height (filmholes_left);

    /* Left */
    gtk_snapshot_push_repeat (snapshot,
                              &GRAPHENE_RECT_INIT (0, 0, holes_width, height),
                              NULL);
    left_texture = gdk_texture_new_for_pixbuf (filmholes_left);
    gtk_snapshot_append_texture (snapshot,
                                 left_texture,
                                 &GRAPHENE_RECT_INIT (0, 0, holes_width, holes_height));
    gtk_snapshot_pop (snapshot);

    /* Right */
    gtk_snapshot_push_repeat (snapshot,
                              &GRAPHENE_RECT_INIT (width - holes_width, 0, holes_width, height),
                              NULL);
    right_texture = gdk_texture_new_for_pixbuf (filmholes_right);
    gtk_snapshot_append_texture (snapshot,
                                 right_texture,
                                 &GRAPHENE_RECT_INIT (width - holes_width, 0, holes_width, holes_height));
    gtk_snapshot_pop (snapshot);
}

gboolean
nautilus_date_time_is_between_dates (GDateTime *date,
                                     GDateTime *initial_date,
                                     GDateTime *end_date)
{
    gboolean in_between;

    /* Silently ignore errors */
    if (date == NULL || g_date_time_to_unix (date) == 0)
    {
        return FALSE;
    }

    /* For the end date, we want to make end_date inclusive,
     * for that the difference between the start of the day and the in_between
     * has to be more than -1 day
     */
    in_between = g_date_time_difference (date, initial_date) > 0 &&
                 g_date_time_difference (end_date, date) / G_TIME_SPAN_DAY > -1;

    return in_between;
}

static const gchar *
get_text_for_days_ago (gint     days,
                       gboolean prefix_with_since)
{
    if (days < 7)
    {
        /* days */
        return prefix_with_since ?
               ngettext ("Since %d day ago", "Since %d days ago", days) :
               ngettext ("%d day ago", "%d days ago", days);
    }
    if (days < 30)
    {
        /* weeks */
        return prefix_with_since ?
               ngettext ("Since last week", "Since %d weeks ago", days / 7) :
               ngettext ("Last week", "%d weeks ago", days / 7);
    }
    if (days < 365)
    {
        /* months */
        return prefix_with_since ?
               ngettext ("Since last month", "Since %d months ago", days / 30) :
               ngettext ("Last month", "%d months ago", days / 30);
    }

    /* years */
    return prefix_with_since ?
           ngettext ("Since last year", "Since %d years ago", days / 365) :
           ngettext ("Last year", "%d years ago", days / 365);
}

gchar *
get_text_for_date_range (GPtrArray *date_range,
                         gboolean   prefix_with_since)
{
    gint days;
    gint normalized;
    GDateTime *initial_date;
    GDateTime *end_date;
    gchar *formatted_date;
    gchar *label;

    if (!date_range)
    {
        return NULL;
    }

    initial_date = g_ptr_array_index (date_range, 0);
    end_date = g_ptr_array_index (date_range, 1);
    days = g_date_time_difference (end_date, initial_date) / G_TIME_SPAN_DAY;
    formatted_date = g_date_time_format (initial_date, "%x");

    if (days < 1)
    {
        label = g_strdup (formatted_date);
    }
    else
    {
        if (days < 7)
        {
            /* days */
            normalized = days;
        }
        else if (days < 30)
        {
            /* weeks */
            normalized = days / 7;
        }
        else if (days < 365)
        {
            /* months */
            normalized = days / 30;
        }
        else
        {
            /* years */
            normalized = days / 365;
        }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
        label = g_strdup_printf (get_text_for_days_ago (days,
                                                        prefix_with_since),
                                 normalized);
#pragma GCC diagnostic pop
    }

    g_free (formatted_date);

    return label;
}

AdwMessageDialog *
show_dialog (const gchar    *primary_text,
             const gchar    *secondary_text,
             GtkWindow      *parent,
             GtkMessageType  type)
{
    GtkWidget *dialog;

    g_return_val_if_fail (parent != NULL, NULL);

    dialog = adw_message_dialog_new (parent, primary_text, secondary_text);
    adw_message_dialog_add_response (ADW_MESSAGE_DIALOG (dialog), "ok", _("_OK"));
    adw_message_dialog_set_default_response (ADW_MESSAGE_DIALOG (dialog), "ok");

    gtk_window_present (GTK_WINDOW (dialog));

    return ADW_MESSAGE_DIALOG (dialog);
}

static void
notify_unmount_done (GMountOperation *op,
                     const gchar     *message,
                     gpointer         user_data)
{
    NautilusApplication *application;
    gchar *notification_id;

    application = nautilus_application_get_default ();
    notification_id = g_strdup_printf ("nautilus-mount-operation-%p", op);
    nautilus_application_withdraw_notification (application, notification_id);

    if (message != NULL)
    {
        GNotification *unplug;
        GIcon *icon;
        gchar **strings;

        strings = g_strsplit (message, "\n", 0);
        icon = g_mount_get_symbolic_icon (G_MOUNT (user_data));
        unplug = g_notification_new (strings[0]);
        g_notification_set_body (unplug, strings[1]);
        g_notification_set_icon (unplug, icon);

        nautilus_application_send_notification (application, notification_id, unplug);
        g_object_unref (unplug);
        g_object_unref (icon);
        g_strfreev (strings);
    }

    g_free (notification_id);
}

static void
notify_unmount_show (GMountOperation *op,
                     const gchar     *message,
                     gpointer         user_data)
{
    NautilusApplication *application;
    GNotification *unmount;
    gchar *notification_id;
    GIcon *icon;
    gchar **strings;

    application = nautilus_application_get_default ();
    strings = g_strsplit (message, "\n", 0);
    icon = g_mount_get_icon (G_MOUNT (user_data));

    unmount = g_notification_new (strings[0]);
    g_notification_set_body (unmount, strings[1]);
    g_notification_set_icon (unmount, icon);
    g_notification_set_priority (unmount, G_NOTIFICATION_PRIORITY_URGENT);

    notification_id = g_strdup_printf ("nautilus-mount-operation-%p", op);
    nautilus_application_send_notification (application, notification_id, unmount);
    g_object_unref (unmount);
    g_object_unref (icon);
    g_strfreev (strings);
    g_free (notification_id);
}

void
show_unmount_progress_cb (GMountOperation *op,
                          const gchar     *message,
                          gint64           time_left,
                          gint64           bytes_left,
                          gpointer         user_data)
{
    if (bytes_left == 0)
    {
        notify_unmount_done (op, message, user_data);
    }
    else
    {
        notify_unmount_show (op, message, user_data);
    }
}

void
show_unmount_progress_aborted_cb (GMountOperation *op,
                                  gpointer         user_data)
{
    notify_unmount_done (op, NULL, user_data);
}
