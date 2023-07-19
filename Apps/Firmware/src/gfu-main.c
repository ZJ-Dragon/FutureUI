/*
 * Copyright (C) 2019 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include "config.h"

#include <adwaita.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <locale.h>
#include <stdlib.h>

#include "gfu-common.h"
#include "gfu-device-row.h"
#include "gfu-release-row.h"

typedef struct {
	AdwApplication *application;
	GtkBuilder *builder;
	GCancellable *cancellable;
	FwupdClient *client;
	FwupdDevice *device;
	FwupdRelease *release;
	FwupdInstallFlags flags;
	guint refresh_id;
	GPtrArray *actions;
	GPtrArray *post_requests;
} GfuMain;

static void
gfu_main_release_row_button_clicked_cb(GfuReleaseRow *row, GfuMain *self);
static void
gfu_main_refresh_releases(GfuMain *self);

/* GTK helper functions */

static void
gfu_main_list_box_remove_all(GtkListBox *container)
{
	GtkWidget *child;
	while ((child = gtk_widget_get_last_child(GTK_WIDGET(container))) != NULL)
		gtk_list_box_remove(container, child);
}

static void
gfu_main_error_dialog(GfuMain *self, const gchar *title, const gchar *message)
{
	GtkWindow *window;
	GtkWidget *dialog;

	window = GTK_WINDOW(gtk_builder_get_object(self->builder, "dialog_main"));
	dialog = gtk_message_dialog_new(window,
					GTK_DIALOG_MODAL,
					GTK_MESSAGE_ERROR,
					GTK_BUTTONS_CLOSE,
					"%s",
					title);
	if (message != NULL)
		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", message);
	g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
	gtk_window_present(GTK_WINDOW(dialog));
}

static void
gfu_main_error_fatal(GfuMain *self, const gchar *text)
{
	GtkWidget *w = GTK_WIDGET(gtk_builder_get_object(self->builder, "stack_main"));
	GtkWidget *leaflet_devices =
	    GTK_WIDGET(gtk_builder_get_object(self->builder, "leaflet_devices"));
	gtk_widget_set_visible(GTK_WIDGET(leaflet_devices), FALSE);
	gtk_stack_set_visible_child_name(GTK_STACK(w), "error");
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "label_error"));
	adw_status_page_set_description(ADW_STATUS_PAGE(w), text);
}

static void
gfu_main_activate_cb(GApplication *application, GfuMain *self)
{
	GtkWindow *window = GTK_WINDOW(gtk_builder_get_object(self->builder, "dialog_main"));
	gtk_window_present(window);
}

static void
gfu_main_set_label(GfuMain *self, const gchar *label_id, const gchar *text)
{
	GtkWidget *w;
	g_autofree gchar *label_id_title = g_strdup_printf("%s_title", label_id);

	/* hide empty box */
	if (text == NULL || text[0] == '\0') {
		w = GTK_WIDGET(gtk_builder_get_object(self->builder, label_id));
		gtk_widget_set_visible(w, FALSE);
		w = GTK_WIDGET(gtk_builder_get_object(self->builder, label_id_title));
		gtk_widget_set_visible(w, FALSE);
		return;
	}

	/* update and display */
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, label_id));
	gtk_label_set_label(GTK_LABEL(w), text);
	gtk_widget_set_visible(w, TRUE);
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, label_id_title));
	gtk_widget_set_visible(w, TRUE);
}

static void
gfu_main_set_label_title(GfuMain *self, const gchar *label_id, const gchar *text)
{
	/* update only the title of a label */
	GtkWidget *w;
	g_autofree gchar *label_id_title = g_strdup_printf("%s_title", label_id);

	/* hide empty box */
	if (text == NULL) {
		w = GTK_WIDGET(gtk_builder_get_object(self->builder, label_id));
		gtk_widget_set_visible(w, FALSE);
		w = GTK_WIDGET(gtk_builder_get_object(self->builder, label_id_title));
		gtk_widget_set_visible(w, FALSE);
		return;
	}

	/* update and display */
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, label_id_title));
	gtk_label_set_label(GTK_LABEL(w), text);
	gtk_widget_set_visible(w, TRUE);
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, label_id));
	gtk_widget_set_visible(w, TRUE);
}

static GtkWidget *
gfu_main_guid_instance_id_new(const gchar *instance_id, const gchar *guid)
{
	GtkWidget *row = adw_action_row_new();
	g_autofree gchar *guid_monospace = g_strdup_printf("<tt>%s</tt>", guid);

	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), guid_monospace);
	adw_preferences_row_set_use_markup(ADW_PREFERENCES_ROW(row), TRUE);
	adw_preferences_row_set_title_selectable(ADW_PREFERENCES_ROW(row), TRUE);

	if (instance_id != NULL) {
		g_autofree gchar *instance_id_safe = g_markup_escape_text(instance_id, -1);
		adw_action_row_set_subtitle(ADW_ACTION_ROW(row), instance_id_safe);
	}

	return row;
}

static void
gfu_main_update_guids(GfuMain *self, GPtrArray *guids, GPtrArray *instance_ids)
{
	GtkWidget *w;
	g_autoptr(GHashTable) guid_hash = NULL;

	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "row_guids"));
	gtk_widget_set_visible(w, guids->len > 0);

	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "listbox_guids"));
	gfu_main_list_box_remove_all(GTK_LIST_BOX(w));

	/* instance IDs */
	guid_hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	for (guint i = 0; i < instance_ids->len; i++) {
		const gchar *instance_id = g_ptr_array_index(instance_ids, i);
		g_autofree gchar *guid = fwupd_guid_hash_string(instance_id);
		GtkWidget *row = gfu_main_guid_instance_id_new(instance_id, guid);
		g_hash_table_add(guid_hash, g_steal_pointer(&guid));
		gtk_list_box_insert(GTK_LIST_BOX(w), row, -1);
	}

	/* GUID fallback */
	for (guint i = 0; i < guids->len; i++) {
		const gchar *guid = g_ptr_array_index(guids, i);
		GtkWidget *row;
		if (g_hash_table_contains(guid_hash, guid))
			continue;
		row = gfu_main_guid_instance_id_new(NULL, guid);
		gtk_list_box_insert(GTK_LIST_BOX(w), row, -1);
	}
}

static void
gfu_main_set_device_flags(GfuMain *self, guint64 flags)
{
	GtkWidget *icon, *label;
	GtkGrid *w;
	gint count = 0;
	g_autoptr(GString) flag = g_string_new(NULL);

	/* clear the grid */
	w = GTK_GRID(gtk_builder_get_object(self->builder, "grid_device_flags"));
	gtk_grid_remove_column(w, 0);
	gtk_grid_remove_column(w, 0);
	gtk_grid_insert_column(w, 0);
	gtk_grid_insert_column(w, 0);

	/* iterate through flags */
	for (guint j = 0; j < 64; j++) {
		const gchar *flag_tmp;
		/* if flag is not set, skip */
		if ((flags & ((guint64)1 << j)) == 0)
			continue;
		/* else, add a row for this flag */
		g_string_set_size(flag, 0);
		flag_tmp = gfu_common_device_flags_to_string((guint64)1 << j);
		if (flag_tmp == NULL)
			continue;
		g_string_assign(flag, flag_tmp);
		gtk_grid_insert_row(w, count);

		icon =
		    gtk_image_new_from_icon_name(gfu_common_device_icon_from_flag((guint64)1 << j));
		gtk_widget_set_visible(icon, TRUE);
		gtk_grid_attach(w, icon, 0, count, 1, 1);

		label = gtk_label_new(flag->str);
		gtk_label_set_xalign(GTK_LABEL(label), 0);
		gtk_widget_set_visible(label, TRUE);
		gtk_label_set_wrap(GTK_LABEL(label), TRUE);
		gtk_grid_attach(w, label, 1, count, 1, 1);
		count++;
	}
	if (count == 0) {
		/* no flags were added */
		gtk_widget_set_visible(GTK_WIDGET(w), FALSE);
		gtk_widget_set_visible(
		    GTK_WIDGET(gtk_builder_get_object(self->builder, "label_device_flags_title")),
		    FALSE);
	} else {
		gtk_widget_set_visible(GTK_WIDGET(w), TRUE);
		gtk_widget_set_visible(
		    GTK_WIDGET(gtk_builder_get_object(self->builder, "label_device_flags_title")),
		    TRUE);
	}
}

static void
gfu_main_device_auto_verify_cb(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	GfuMain *self = (GfuMain *)user_data;
	g_autoptr(GError) error = NULL;

	if (!fwupd_client_verify_finish(self->client, res, &error)) {
		g_warning("failed to verify firmware:  %s", error->message);
		/* TRANSLATORS: the attestation failed */
		gfu_main_set_label(self, "label_device_checksums", _("Not OK"));
		return;
	}

	/* TRANSLATORS: the checksum verified */
	gfu_main_set_label(self, "label_device_checksums", _("OK"));
}

#if FWUPD_CHECK_VERSION(1, 8, 1)
static gchar *
gfu_main_device_problem_to_string(GfuMain *self, FwupdDeviceProblem problem)
{
	if (problem == FWUPD_DEVICE_PROBLEM_SYSTEM_POWER_TOO_LOW) {
		if (fwupd_client_get_battery_level(self->client) == FWUPD_BATTERY_LEVEL_INVALID ||
		    fwupd_client_get_battery_threshold(self->client) ==
			FWUPD_BATTERY_LEVEL_INVALID) {
			/* TRANSLATORS: as in laptop battery power */
			return g_strdup(_("System power is too low to perform the update"));
		}
		return g_strdup_printf(
		    /* TRANSLATORS: as in laptop battery power */
		    _("System power is too low to perform the update (%u%%, requires %u%%)"),
		    fwupd_client_get_battery_level(self->client),
		    fwupd_client_get_battery_threshold(self->client));
	}
	if (problem == FWUPD_DEVICE_PROBLEM_UNREACHABLE) {
		/* TRANSLATORS: for example, a Bluetooth mouse that is in powersave mode */
		return g_strdup(_("Device is unreachable, or out of wireless range"));
	}
	if (problem == FWUPD_DEVICE_PROBLEM_POWER_TOO_LOW) {
		if (fwupd_device_get_battery_level(self->device) == FWUPD_BATTERY_LEVEL_INVALID ||
		    fwupd_device_get_battery_threshold(self->device) ==
			FWUPD_BATTERY_LEVEL_INVALID) {
			/* TRANSLATORS: for example the batteries *inside* the Bluetooth mouse */
			return g_strdup(_("Device battery power is too low"));
		}
		/* TRANSLATORS: for example the batteries *inside* the Bluetooth mouse */
		return g_strdup_printf(_("Device battery power is too low (%u%%, requires %u%%)"),
				       fwupd_device_get_battery_level(self->device),
				       fwupd_device_get_battery_threshold(self->device));
	}
	if (problem == FWUPD_DEVICE_PROBLEM_UPDATE_PENDING) {
		/* TRANSLATORS: usually this is when we're waiting for a reboot */
		return g_strdup(_("Device is waiting for the update to be applied"));
	}
	if (problem == FWUPD_DEVICE_PROBLEM_REQUIRE_AC_POWER) {
		/* TRANSLATORS: as in, wired mains power for a laptop */
		return g_strdup(_("Device requires AC power to be connected"));
	}
	if (problem == FWUPD_DEVICE_PROBLEM_LID_IS_CLOSED) {
		/* TRANSLATORS: lid means "laptop top cover" */
		return g_strdup(_("Device cannot be used while the lid is closed"));
	}
#if FWUPD_CHECK_VERSION(1, 9, 1)
	if (problem == FWUPD_DEVICE_PROBLEM_IN_USE) {
		/* TRANSLATORS: device cannot be interrupted, for instance taking a phone call */
		return g_strdup(_("Device is in use"));
	}
#endif
	return NULL;
}
#endif

static gchar *
gfu_main_device_problems_to_string(GfuMain *self)
{
#if FWUPD_CHECK_VERSION(1, 8, 1)
	g_autoptr(GString) str = g_string_new(NULL);

	/* sanity check */
	if (self->device == NULL)
		return NULL;
	if (fwupd_device_get_problems(self->device) == FWUPD_DEVICE_PROBLEM_NONE)
		return NULL;

	/* translate each problem */
	for (guint i = 0; i < 64; i++) {
		FwupdDeviceProblem problem = 1ull << i;
		g_autofree gchar *tmp = NULL;
		if (!fwupd_device_has_problem(self->device, problem))
			continue;
		tmp = gfu_main_device_problem_to_string(self, problem);
		if (tmp == NULL)
			continue;
		g_string_append_printf(str, "%s\n", tmp);
	}

	/* success */
	if (str->len == 0)
		return NULL;
	g_string_truncate(str, str->len - 1);
	return g_string_free(g_steal_pointer(&str), FALSE);
#else
	return NULL;
#endif
}

static gboolean
gfu_main_refresh_device_cb(gpointer user_data)
{
	GfuMain *self = (GfuMain *)user_data;
	GtkWidget *w;
	GtkWidget *leaflet_devices =
	    GTK_WIDGET(gtk_builder_get_object(self->builder, "leaflet_devices"));
	GPtrArray *vendor_ids = NULL;
	g_autofree gchar *problems_str = NULL;
	g_autoptr(GString) attr = g_string_new(NULL);
	g_autoptr(GString) version = NULL;

	/* set stack */
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "stack_main"));
	if (self->device == NULL) {
		gtk_stack_set_visible_child_name(GTK_STACK(w), "loading");
		gtk_widget_set_visible(GTK_WIDGET(leaflet_devices), FALSE);
		self->refresh_id = 0;
		return G_SOURCE_REMOVE;
	}

	gtk_stack_set_visible_child_name(GTK_STACK(w), "main");
	gtk_widget_set_visible(GTK_WIDGET(leaflet_devices), TRUE);

	/* set device information */
	version = g_string_new(fwupd_device_get_version(self->device));

	if (fwupd_device_get_version_build_date(self->device) != 0) {
		guint64 value = fwupd_device_get_version_build_date(self->device);
		g_autoptr(GDateTime) date = g_date_time_new_from_unix_utc((gint64)value);
		g_autofree gchar *datestr = g_date_time_format(date, "%F");
		g_string_append_printf(version, " [%s]", datestr);
	}

	gfu_main_set_label(self, "label_device_version", version->str);
	gfu_main_set_label(self,
			   "label_device_version_lowest",
			   fwupd_device_get_version_lowest(self->device));
	gfu_main_set_label(self,
			   "label_device_version_bootloader",
			   fwupd_device_get_version_bootloader(self->device));
	problems_str = gfu_main_device_problems_to_string(self);
	gfu_main_set_label(self, "label_device_problems", problems_str);
	if (problems_str != NULL) {
		gfu_main_set_label(self, "label_device_update_error", NULL);
	} else {
		gfu_main_set_label(self,
				   "label_device_update_error",
				   fwupd_device_get_update_error(self->device));
	}
	gfu_main_set_label(self, "label_device_serial", fwupd_device_get_serial(self->device));
	gfu_main_set_label(self, "label_device_vendor", fwupd_device_get_vendor(self->device));
	gfu_main_set_label(self, "label_device_branch", fwupd_device_get_branch(self->device));

	vendor_ids = fwupd_device_get_vendor_ids(self->device);
	if (vendor_ids->len > 0) {
		g_autofree gchar *strv = gfu_common_strjoin_array(", ", vendor_ids);
		gfu_main_set_label(self, "label_device_vendor_ids", strv);
		w = GTK_WIDGET(
		    gtk_builder_get_object(self->builder, "label_device_vendor_ids_title"));
		adw_preferences_row_set_title(ADW_PREFERENCES_ROW(w),
					      /* TRANSLATORS: the hw IDs the device defines,
					       * e.g. USB:0x1234 */
					      ngettext("Vendor ID", "Vendor IDs", vendor_ids->len));
	} else {
		gfu_main_set_label(self, "label_device_vendor_ids", NULL);
	}

	g_string_append_printf(attr, "%u", fwupd_device_get_flashes_left(self->device));
	gfu_main_set_label(self,
			   "label_device_flashes_left",
			   g_strcmp0(attr->str, "0") ? attr->str : NULL);

	gfu_main_set_label(
	    self,
	    "label_device_install_duration",
	    gfu_common_seconds_to_string(fwupd_device_get_install_duration(self->device)));

	gfu_main_update_guids(self,
			      fwupd_device_get_guids(self->device),
			      fwupd_device_get_instance_ids(self->device));
	gfu_main_set_device_flags(self, fwupd_device_get_flags(self->device));

	/* device can be verified immediately without hardware access */
	if (fwupd_device_has_flag(self->device, FWUPD_DEVICE_FLAG_CAN_VERIFY) &&
	    !fwupd_device_has_flag(self->device, FWUPD_DEVICE_FLAG_CAN_VERIFY_IMAGE)) {
		fwupd_client_verify_async(self->client,
					  fwupd_device_get_id(self->device),
					  self->cancellable,
					  gfu_main_device_auto_verify_cb,
					  self);
	} else if (fwupd_device_has_flag(self->device, FWUPD_DEVICE_FLAG_CAN_VERIFY_IMAGE)) {
		/* TRANSLATORS: the checksum state was unknown */
		gfu_main_set_label(self, "label_device_checksums", _("Unknown"));
	} else {
		gfu_main_set_label(self, "label_device_checksums", NULL);
	}

	/* unlock section */
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "label_device_unlock_title"));
	gtk_widget_set_visible(w, fwupd_device_has_flag(self->device, FWUPD_DEVICE_FLAG_LOCKED));

	/* verify button */
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "button_verify"));
	gtk_widget_set_visible(
	    w,
	    fwupd_device_has_flag(self->device, FWUPD_DEVICE_FLAG_CAN_VERIFY_IMAGE));

	/* verify update button */
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "button_verify_update"));
	gtk_widget_set_visible(
	    w,
	    fwupd_device_get_checksums(self->device)->len > 0 &&
		fwupd_device_has_flag(self->device, FWUPD_DEVICE_FLAG_CAN_VERIFY));

	/* get new releases */
	gfu_main_refresh_releases(self);
	self->refresh_id = 0;
	return G_SOURCE_REMOVE;
}

static void
gfu_main_refresh_device(GfuMain *self)
{
	if (self->refresh_id != 0)
		g_source_remove(self->refresh_id);
	self->refresh_id = g_timeout_add(100, gfu_main_refresh_device_cb, self);
}

static void
gfu_main_device_added_cb(FwupdClient *client, FwupdDevice *device, GfuMain *self)
{
	GtkWidget *w;
	GtkWidget *l;

	/* ignore if device can't be updated */
	if (!fwupd_device_has_flag(device, FWUPD_DEVICE_FLAG_UPDATABLE) &&
	    !fwupd_device_has_flag(device, FWUPD_DEVICE_FLAG_UPDATABLE_HIDDEN))
		return;

	/* create and add new row for device */
	l = gfu_device_row_new(device);
	gtk_widget_set_visible(l, TRUE);
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "listbox_devices"));
	gtk_list_box_insert(GTK_LIST_BOX(w), l, -1);
}

static void
gfu_main_device_removed_cb(FwupdClient *client, FwupdDevice *device, GfuMain *self)
{
	GtkWidget *w;

	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "listbox_devices"));
	for (GtkWidget *c = gtk_widget_get_first_child(w); c != NULL;
	     c = gtk_widget_get_next_sibling(c)) {
		GfuDeviceRow *row = GFU_DEVICE_ROW(c);
		if (fwupd_device_compare(device, gfu_device_row_get_device(row)) == 0) {
			gtk_list_box_remove(GTK_LIST_BOX(w), c);
			break;
		}
	}
}

static void
gfu_main_reboot_shutdown_response_cb(GtkDialog *dialog, int response_id, GfuMain *self)
{
	if (response_id == GTK_RESPONSE_YES) {
		g_autoptr(GError) error = NULL;
		if (!gfu_common_system_shutdown(&error)) {
			g_debug("Failed to shutdown device: %s\n", error->message);
			gfu_main_error_dialog(self,
					      /* TRANSLATORS: error in shutting down the system */
					      _("Failed to shutdown device"),
					      _("A manual shutdown is required."));
		}
	}
	gtk_window_destroy(GTK_WINDOW(dialog));
}

static void
gfu_main_reboot_restart_response_cb(GtkDialog *dialog, int response_id, GfuMain *self)
{
	if (response_id == GTK_RESPONSE_YES) {
		g_autoptr(GError) error = NULL;
		if (!gfu_common_system_reboot(&error)) {
			g_debug("Failed to reboot device: %s\n", error->message);
			gfu_main_error_dialog(self,
					      /* TRANSLATORS: error in rebooting down the system */
					      _("Failed to reboot device"),
					      _("A manual reboot is required."));
		}
	}
	gtk_window_destroy(GTK_WINDOW(dialog));
}

static void
gfu_main_update_remotes_cb(GObject *source, GAsyncResult *res, gpointer user_data)
{
	GfuMain *self = (GfuMain *)user_data;
	GtkWidget *w;
	gboolean disabled_lvfs_remote = FALSE;
	gboolean enabled_any_download_remote = FALSE;
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) remotes = NULL;

	remotes = fwupd_client_get_remotes_finish(FWUPD_CLIENT(source), res, &error);
	if (remotes == NULL) {
		/* TRANSLATORS: fwupd refused us data */
		gfu_main_error_dialog(self, _("Failed to load list of remotes"), error->message);
		return;
	}
	for (guint i = 0; i < remotes->len; i++) {
		FwupdRemote *remote = g_ptr_array_index(remotes, i);
		g_debug("%s is %s",
			fwupd_remote_get_id(remote),
			fwupd_remote_get_enabled(remote) ? "enabled" : "disabled");

		/* if another repository is turned on providing metadata */
		if (fwupd_remote_get_enabled(remote)) {
			if (fwupd_remote_get_kind(remote) == FWUPD_REMOTE_KIND_DOWNLOAD)
				enabled_any_download_remote = TRUE;

			/* lvfs is present and disabled */
		} else {
			if (g_strcmp0(fwupd_remote_get_id(remote), "lvfs") == 0)
				disabled_lvfs_remote = TRUE;
		}
	}

	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "infobar_enable_lvfs"));
	if (disabled_lvfs_remote && !enabled_any_download_remote) {
		gtk_widget_set_visible(w, TRUE);
		gtk_info_bar_set_revealed(GTK_INFO_BAR(w), TRUE);
	} else {
		gtk_widget_set_visible(w, FALSE);
	}
}

static gint
gfu_main_sort_device_list_box_cb(GtkListBoxRow *row1, GtkListBoxRow *row2, gpointer user_data)
{
	FwupdDevice *dev1 = gfu_device_row_get_device(GFU_DEVICE_ROW(row1));
	FwupdDevice *dev2 = gfu_device_row_get_device(GFU_DEVICE_ROW(row2));
	FwupdDevice *roo1 = fwupd_device_get_root(dev1);
	FwupdDevice *roo2 = fwupd_device_get_root(dev2);
	g_autofree gchar *id1 = NULL;
	g_autofree gchar *id2 = NULL;

	if (dev1 == roo1) {
		id1 = g_strdup_printf("%s:", fwupd_device_get_id(dev1));
	} else {
		id1 =
		    g_strdup_printf("%s:%s", fwupd_device_get_id(roo1), fwupd_device_get_id(dev1));
	}
	if (dev2 == roo2) {
		id2 = g_strdup_printf("%s:", fwupd_device_get_id(dev2));
	} else {
		id2 =
		    g_strdup_printf("%s:%s", fwupd_device_get_id(roo2), fwupd_device_get_id(dev2));
	}
	return g_strcmp0(id1, id2);
}

static gboolean
gfu_main_update_devices_activate_child_cb(gpointer user_data)
{
	GfuMain *self = (GfuMain *)user_data;
	GtkWidget *w;
	AdwLeaflet *leaflet = ADW_LEAFLET(gtk_builder_get_object(self->builder, "leaflet"));

	/* get first device in the list */
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "listbox_devices"));
	w = gtk_widget_get_first_child(w);
	if (w == NULL)
		return G_SOURCE_REMOVE;

	/* activate the first device */
	if (!adw_leaflet_get_folded(leaflet))
		gtk_widget_activate(w);

	/* never repeat */
	return G_SOURCE_REMOVE;
}

static void
gfu_main_update_devices_cb(GObject *source, GAsyncResult *res, gpointer user_data)
{
	GtkWidget *w;
	GfuMain *self = (GfuMain *)user_data;
	AdwLeaflet *leaflet = ADW_LEAFLET(gtk_builder_get_object(self->builder, "leaflet"));
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) devices = NULL;

	/* stop the spinner now */
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "spinner_loading"));
	gtk_spinner_stop(GTK_SPINNER(w));

	devices = fwupd_client_get_devices_finish(FWUPD_CLIENT(source), res, &error);
	if (devices == NULL) {
		/* TRANSLATORS: fwupd refused us data */
		gfu_main_error_dialog(self, _("Failed to load device list"), error->message);
		return;
	}

	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "device_metadata"));
	gtk_widget_set_visible(GTK_WIDGET(w), devices->len > 0);

	/* create a row in the listbox for each updatable device */
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "listbox_devices"));
	for (guint i = 0; i < devices->len; i++) {
		FwupdDevice *device = g_ptr_array_index(devices, i);
		GtkWidget *l;

		/* skip devices that can't be updated */
		if (!fwupd_device_has_flag(device, FWUPD_DEVICE_FLAG_UPDATABLE) &&
		    !fwupd_device_has_flag(device, FWUPD_DEVICE_FLAG_UPDATABLE_HIDDEN) &&
		    !fwupd_device_has_flag(device, FWUPD_DEVICE_FLAG_LOCKED)) {
			g_debug("ignoring non-updatable device: %s", fwupd_device_get_name(device));
			continue;
		}
		l = gfu_device_row_new(device);
		gtk_widget_set_visible(l, TRUE);
		gtk_list_box_insert(GTK_LIST_BOX(w), l, -1);
	}

	/* get GfuDeviceRow */
	w = gtk_widget_get_first_child(w);
	if (w == NULL) {
		/* TRANSLATORS: no devices supporting firmware updates were found */
		gfu_main_error_fatal(self, _("No supported devices were found…"));
		return;
	}

	/* show devices */
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "stack_main"));
	gtk_stack_set_visible_child_name(GTK_STACK(w), "main");
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "leaflet_devices"));
	gtk_widget_set_visible(GTK_WIDGET(w), TRUE);
	adw_leaflet_set_visible_child(leaflet, w);

	/* activate the first device once the leaflet has been folded or not */
	g_idle_add(gfu_main_update_devices_activate_child_cb, self);
}

static void
gfu_main_update_releases(GfuMain *self, GPtrArray *releases)
{
	GtkWidget *w;

	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "group_releases"));
	gtk_widget_set_visible(w, releases != NULL);
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "group_releases_none"));
	gtk_widget_set_visible(w, releases == NULL);

	/* nothing to do */
	if (releases == NULL)
		return;

	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "listbox_releases"));
	for (guint i = 0; i < releases->len; i++) {
		FwupdRelease *release = g_ptr_array_index(releases, i);
		GtkWidget *l = gfu_release_row_new(self->device, release);
		g_debug("adding release %s", fwupd_release_get_version(release));
		gtk_list_box_insert(GTK_LIST_BOX(w), l, -1);
		g_signal_connect(l,
				 "button-clicked",
				 G_CALLBACK(gfu_main_release_row_button_clicked_cb),
				 self);
	}
}

static void
gfu_main_get_releases_cb(GObject *source, GAsyncResult *res, gpointer user_data)
{
	GfuMain *self = (GfuMain *)user_data;
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) releases = NULL;

	releases = fwupd_client_get_releases_finish(FWUPD_CLIENT(source), res, &error);
	if (releases == NULL)
		g_debug("ignoring: %s", error->message);
	gfu_main_update_releases(self, releases);
}

static void
gfu_main_update_progress_bar(GfuMain *self)
{
	GtkWidget *w = GTK_WIDGET(gtk_builder_get_object(self->builder, "progress_bar"));
	GtkRevealer *revealer = GTK_REVEALER(gtk_builder_get_object(self->builder, "revealer"));
	guint percentage = fwupd_client_get_percentage(self->client);
	FwupdStatus status = fwupd_client_get_status(self->client);

	/* nothing to show */
	if (status == FWUPD_STATUS_IDLE || status == FWUPD_STATUS_UNKNOWN) {
		gtk_revealer_set_reveal_child(revealer, FALSE);
		return;
	}

	/* update progress */
	if (percentage == 0) {
		gtk_progress_bar_pulse(GTK_PROGRESS_BAR(w));
	} else {
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(w), ((gdouble)percentage) / 100.f);
	}

	/* set label */
	g_debug("status: %s [%u%%]", fwupd_status_to_string(status), percentage);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(w), gfu_status_to_string(status));
	gtk_revealer_set_reveal_child(revealer, TRUE);
}

static void
gfu_main_client_status_changed_cb(FwupdClient *client, GParamSpec *pspec, GfuMain *self)
{
	gfu_main_update_progress_bar(self);
}

static void
gfu_main_client_percentage_changed_cb(FwupdClient *client, GParamSpec *pspec, GfuMain *self)
{
	gfu_main_update_progress_bar(self);
}

static void
gfu_main_refresh_remote_cb(GObject *source, GAsyncResult *res, gpointer user_data)
{
	GfuMain *self = (GfuMain *)user_data;
	g_autoptr(GError) error = NULL;

	if (!fwupd_client_update_metadata_bytes_finish(FWUPD_CLIENT(source), res, &error)) {
		/* TRANSLATORS: the list of available firmware failed to be updated */
		gfu_main_error_dialog(self, _("Failed to refresh metadata"), error->message);
		return;
	}
}

static void
gfu_main_get_lvfs_remote_cb(GObject *source, GAsyncResult *res, gpointer user_data)
{
	GfuMain *self = (GfuMain *)user_data;
	g_autoptr(FwupdRemote) remote = NULL;
	g_autoptr(GError) error = NULL;

	remote = fwupd_client_get_remote_by_id_finish(FWUPD_CLIENT(source), res, &error);
	if (remote == NULL) {
		/* TRANSLATORS: the LVFS remote was not found on the system */
		gfu_main_error_dialog(self, _("Failed to find LVFS"), error->message);
		return;
	}
	fwupd_client_refresh_remote_async(self->client,
					  remote,
					  self->cancellable,
					  gfu_main_refresh_remote_cb,
					  self);
}

static void
gfu_main_modify_remote_cb(GObject *source, GAsyncResult *res, gpointer user_data)
{
	GfuMain *self = (GfuMain *)user_data;
	g_autoptr(GError) error = NULL;

	if (!fwupd_client_modify_remote_finish(FWUPD_CLIENT(source), res, &error)) {
		/* TRANSLATORS: the LVFS remote could not be enabled */
		gfu_main_error_dialog(self, _("Failed to enable LVFS"), error->message);
		return;
	}

	/* refresh the newly-enabled remote */
	fwupd_client_get_remote_by_id_async(self->client,
					    "lvfs",
					    self->cancellable,
					    gfu_main_get_lvfs_remote_cb,
					    self);
}

static void
gfu_main_enable_lvfs_cb(GtkWidget *widget, GfuMain *self)
{
	GtkWidget *w;

	/* hide notification */
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "infobar_enable_lvfs"));
	gtk_info_bar_set_revealed(GTK_INFO_BAR(w), FALSE);

	/* enable remote */
	fwupd_client_modify_remote_async(self->client,
					 "lvfs",
					 "Enabled",
					 "true",
					 self->cancellable,
					 gfu_main_modify_remote_cb,
					 self);
}

static void
gfu_main_get_remotes_cb(GObject *source, GAsyncResult *res, gpointer user_data)
{
	GfuMain *self = (GfuMain *)user_data;
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) remotes = NULL;

	remotes = fwupd_client_get_remotes_finish(FWUPD_CLIENT(source), res, &error);
	if (remotes == NULL) {
		/* TRANSLATORS: fwupd refused us data */
		gfu_main_error_dialog(self, _("Failed to get remotes list"), error->message);
		return;
	}
	for (guint i = 0; i < remotes->len; i++) {
		FwupdRemote *remote = g_ptr_array_index(remotes, i);
		if (!fwupd_remote_get_enabled(remote))
			continue;
		if (fwupd_remote_get_kind(remote) != FWUPD_REMOTE_KIND_DOWNLOAD)
			continue;
		/* should this be a single async action? */
		fwupd_client_refresh_remote_async(self->client,
						  remote,
						  self->cancellable,
						  gfu_main_refresh_remote_cb,
						  self);
	}
}

static void
gfu_main_activate_refresh_metadata(GSimpleAction *simple, GVariant *parameter, gpointer user_data)
{
	GfuMain *self = (GfuMain *)user_data;
	fwupd_client_get_remotes_async(self->client,
				       self->cancellable,
				       gfu_main_get_remotes_cb,
				       self);
}

static void
gfu_main_show_request_real(GfuMain *self, FwupdRequest *request, GdkPixbuf *pixbuf)
{
	GtkWindow *window;
	GtkWidget *dialog;
	const gchar *title;

	if (fwupd_request_get_kind(request) == FWUPD_REQUEST_KIND_POST) {
		/* TRANSLATORS: the user needs to do something, e.g. remove the device */
		title = _("To complete the update further action is required");
	} else {
		/* TRANSLATORS: the user needs to do something, e.g. remove the device */
		title = _("Action is required");
	}

	window = GTK_WINDOW(gtk_builder_get_object(self->builder, "dialog_main"));
	dialog = gtk_message_dialog_new(window,
					GTK_DIALOG_MODAL,
					GTK_MESSAGE_ERROR,
					GTK_BUTTONS_OK,
					"%s",
					title);
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
						 "%s",
						 fwupd_request_get_message(request));

	/* image is optional */
	if (pixbuf != NULL) {
		GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);
		GtkWidget *box = gtk_message_dialog_get_message_area(GTK_MESSAGE_DIALOG(dialog));
		gtk_widget_set_size_request(image,
					    gdk_pixbuf_get_width(pixbuf),
					    gdk_pixbuf_get_height(pixbuf));
		gtk_box_append(GTK_BOX(box), image);
	}

	g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
	gtk_window_present(GTK_WINDOW(dialog));
}

typedef struct {
	GfuMain *self;
	FwupdRequest *request;
} GfuMainAsyncReleaseHelper;

static void
gfu_main_async_helper_free(GfuMainAsyncReleaseHelper *helper)
{
	g_object_unref(helper->request);
	g_free(helper);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GfuMainAsyncReleaseHelper, gfu_main_async_helper_free);

static void
gfu_main_show_request_download_cb(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	g_autoptr(GfuMainAsyncReleaseHelper) helper = (GfuMainAsyncReleaseHelper *)user_data;
	g_autoptr(GBytes) blob = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GInputStream) stream = NULL;
	g_autoptr(GdkPixbuf) pixbuf = NULL;

	blob = fwupd_client_download_bytes_finish(FWUPD_CLIENT(source_object), res, &error);
	if (blob == NULL) {
		g_warning("failed to download image: %s", error->message);
	} else {
		stream = g_memory_input_stream_new_from_bytes(blob);
		pixbuf = gdk_pixbuf_new_from_stream(stream, helper->self->cancellable, &error);
		if (pixbuf == NULL) {
			g_warning("failed to load image: %s", error->message);
		} else {
			g_debug("loaded pixbuf %ux%u",
				(guint)gdk_pixbuf_get_width(pixbuf),
				(guint)gdk_pixbuf_get_height(pixbuf));
		}
	}
	gfu_main_show_request_real(helper->self, helper->request, pixbuf);
}

static void
gfu_main_show_request(GfuMain *self, FwupdRequest *request)
{
	GfuMainAsyncReleaseHelper *helper;

	/* no image */
	if (fwupd_request_get_image(request) == NULL) {
		gfu_main_show_request_real(self, request, NULL);
		return;
	}

	/* image required */
	helper = g_new0(GfuMainAsyncReleaseHelper, 1);
	helper->self = self;
	helper->request = g_object_ref(request);
	fwupd_client_download_bytes_async(self->client,
					  fwupd_request_get_image(request),
					  FWUPD_CLIENT_DOWNLOAD_FLAG_NONE,
					  self->cancellable,
					  gfu_main_show_request_download_cb,
					  helper);
}

static void
gfu_main_install_release_cb(GObject *source, GAsyncResult *res, gpointer user_data)
{
	GfuMain *self = (GfuMain *)user_data;
	GtkWidget *dialog;
	GtkWindow *window;
	g_autoptr(GError) error = NULL;

	if (!fwupd_client_install_bytes_finish(FWUPD_CLIENT(source), res, &error)) {
		/* TRANSLATORS: the firmware install failed for an unspecified reason */
		gfu_main_error_dialog(self, _("Failed to install firmware"), error->message);
		return;
	}

	/* show any manual action required */
	for (guint i = 0; i < self->post_requests->len; i++) {
		FwupdRequest *request = g_ptr_array_index(self->post_requests, i);
		gfu_main_show_request(self, request);
	}
	g_ptr_array_set_size(self->post_requests, 0);

	/* update the list of releases */
	gfu_main_refresh_device(self);

	/* if successful, prompt for reboot */
	window = GTK_WINDOW(gtk_builder_get_object(self->builder, "dialog_main"));
	if (fwupd_device_has_flag(self->device, FWUPD_DEVICE_FLAG_NEEDS_SHUTDOWN)) {
		dialog = gtk_message_dialog_new(
		    window,
		    GTK_DIALOG_MODAL,
		    GTK_MESSAGE_QUESTION,
		    GTK_BUTTONS_YES_NO,
		    "%s",
		    /* TRANSLATORS: prompting a shutdown to the user */
		    _("The update requires the system to shutdown to complete."));
		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
							 "%s",
							 _("Shutdown now?"));
		g_signal_connect(dialog,
				 "response",
				 G_CALLBACK(gfu_main_reboot_shutdown_response_cb),
				 self);
		gtk_window_present(GTK_WINDOW(dialog));
	} else if (fwupd_device_has_flag(self->device, FWUPD_DEVICE_FLAG_NEEDS_REBOOT)) {
		dialog = gtk_message_dialog_new(window,
						GTK_DIALOG_MODAL,
						GTK_MESSAGE_QUESTION,
						GTK_BUTTONS_YES_NO,
						"%s",
						/* TRANSLATORS: prompting a reboot to the user */
						_("The update requires a reboot to complete."));
		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
							 "%s",
							 _("Restart now?"));
		g_signal_connect(dialog,
				 "response",
				 G_CALLBACK(gfu_main_reboot_restart_response_cb),
				 self);
		gtk_window_present(GTK_WINDOW(dialog));
	} else if (fwupd_release_get_update_message(self->release) == NULL) {
		dialog = gtk_message_dialog_new(window,
						GTK_DIALOG_MODAL,
						GTK_MESSAGE_INFO,
						GTK_BUTTONS_OK,
						"%s",
						/* TRANSLATORS: inform the user that the
						   installation was successful onto the device */
						_("Installation successful"));
		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
							 /* TRANSLATORS: dialog text, %1 is a
							  * version number, %2 is a device name */
							 _("Installed firmware version %s on %s."),
							 fwupd_release_get_version(self->release),
							 fwupd_device_get_name(self->device));
		g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
		gtk_window_present(GTK_WINDOW(dialog));
	}
}

static void
gfu_main_release_install(GfuMain *self)
{
	/* begin installing, show loading animation */
	fwupd_client_install_release2_async(self->client,
					    self->device,
					    self->release,
					    self->flags | FWUPD_INSTALL_FLAG_ALLOW_BRANCH_SWITCH,
					    FWUPD_CLIENT_DOWNLOAD_FLAG_NONE,
					    self->cancellable,
					    gfu_main_install_release_cb,
					    self);
}

static GtkWidget *
gfu_main_release_show_fde_warning(GfuMain *self)
{
	GtkWidget *window;
	GtkWidget *dialog;

	window = GTK_WIDGET(gtk_builder_get_object(self->builder, "dialog_main"));
	dialog = gtk_message_dialog_new(GTK_WINDOW(window),
					GTK_DIALOG_MODAL | GTK_DIALOG_USE_HEADER_BAR,
					GTK_MESSAGE_QUESTION,
					GTK_BUTTONS_NONE,
					"%s",
					/* TRANSLATORS: e.g. bitlocker */
					_("Full disk encryption detected"));
	gtk_dialog_add_button(GTK_DIALOG(dialog), _("OK"), GTK_RESPONSE_OK);
	gtk_dialog_add_button(GTK_DIALOG(dialog), _("Cancel"), GTK_RESPONSE_CANCEL);

	gtk_message_dialog_format_secondary_text(
	    GTK_MESSAGE_DIALOG(dialog),
	    "%s %s",
	    /* TRANSLATORS: the platform secret is stored in the PCRx registers on the TPM */
	    _("Some of the platform secrets may be invalidated when updating this firmware."),
	    /* TRANSLATORS: 'recovery key' here refers to a code, rather than a metal thing */
	    _("Please ensure you have the volume recovery key before continuing."));

	/* success */
	return dialog;
}

static GtkWidget *
gfu_main_release_show_branch_warning(GfuMain *self)
{
	GtkWidget *dialog;
	GtkWidget *window;
	g_autoptr(GString) body = g_string_new(NULL);
	g_autoptr(GString) head = g_string_new(NULL);

	if (fwupd_device_get_vendor(self->device) != NULL &&
	    fwupd_release_get_vendor(self->release) != NULL) {
		g_string_append_printf(head,
				       /* TRANSLATORS: title, %1 is the firmware vendor, %2 is the
					  device vendor name */
				       _("The firmware from %s is not supplied by %s"),
				       fwupd_release_get_vendor(self->release),
				       fwupd_device_get_vendor(self->device));
	} else {
		/* TRANSLATORS: branch switch title */
		g_string_append(head, _("The firmware is not supplied by the vendor"));
	}

	window = GTK_WIDGET(gtk_builder_get_object(self->builder, "dialog_main"));
	dialog = gtk_message_dialog_new(GTK_WINDOW(window),
					GTK_DIALOG_MODAL | GTK_DIALOG_USE_HEADER_BAR,
					GTK_MESSAGE_QUESTION,
					GTK_BUTTONS_NONE,
					"%s",
					head->str);

	/* TRANSLATORS: buttons */
	gtk_dialog_add_button(GTK_DIALOG(dialog), _("Cancel"), GTK_RESPONSE_CANCEL);
	/* TRANSLATORS: button text to switch to another stream of firmware */
	gtk_dialog_add_button(GTK_DIALOG(dialog), _("Switch Branch"), GTK_RESPONSE_OK);

	if (fwupd_device_get_vendor(self->device) != NULL) {
		g_string_append_printf(body,
				       /* TRANSLATORS: %1 is the device vendor name */
				       _("Your hardware may be damaged using this firmware, "
					 "and installing this release may void any warranty "
					 "with %s."),
				       fwupd_device_get_vendor(self->device));
	} else {
		g_string_append(body,
				/* TRANSLATORS: the vendor is unknown */
				_("Your hardware may be damaged using this firmware, "
				  "and installing this release may void any warranty "
				  "with the vendor."));
	}
	g_string_append(body, "\n\n");
	g_string_append(body,
			/* TRANSLATORS: should the branch be changed */
			_("You must understand the consequences of changing the firmware branch."));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", body->str);

	/* success */
	return dialog;
}

static GtkWidget *
gfu_main_release_show_confirmation(GfuMain *self)
{
	GtkWidget *window;
	GtkWidget *dialog;
	const gchar *title_string = NULL;
	gboolean upgrade = fwupd_release_has_flag(self->release, FWUPD_RELEASE_FLAG_IS_UPGRADE);
	gboolean downgrade = fwupd_release_has_flag(self->release, FWUPD_RELEASE_FLAG_IS_DOWNGRADE);
	gboolean reinstall = !downgrade && !upgrade;

	/* make sure user wants to install file to device */
	if (reinstall) {
		/* TRANSLATORS: %1 is device name, %2 is the version */
		title_string = _("Reinstall %s firmware version %s");
	} else if (upgrade) {
		/* TRANSLATORS: %1 is device name, %2 is the version */
		title_string = _("Upgrade %s firmware version %s");
	} else if (downgrade) {
		/* TRANSLATORS: %1 is device name, %2 is the version */
		title_string = _("Downgrade %s firmware version %s");
	} else {
		/* TRANSLATORS: %1 is device name, %2 is the version */
		title_string = _("Install %s firmware version %s");
	}
	window = GTK_WIDGET(gtk_builder_get_object(self->builder, "dialog_main"));
	dialog = gtk_message_dialog_new(GTK_WINDOW(window),
					GTK_DIALOG_MODAL | GTK_DIALOG_USE_HEADER_BAR,
					GTK_MESSAGE_QUESTION,
					GTK_BUTTONS_NONE,
					title_string,
					fwupd_device_get_name(self->device),
					fwupd_release_get_version(self->release));
	/* TRANSLATORS: button text */
	gtk_dialog_add_button(GTK_DIALOG(dialog), _("Cancel"), GTK_RESPONSE_CANCEL);

	self->flags = FWUPD_INSTALL_FLAG_NONE;
	if (reinstall) {
		/* TRANSLATORS: button text: install the same version again */
		gtk_dialog_add_button(GTK_DIALOG(dialog), _("Reinstall"), GTK_RESPONSE_OK);
		self->flags |= FWUPD_INSTALL_FLAG_ALLOW_REINSTALL;
		/* upgrade or downgrade */
	} else {
		if (upgrade) {
			/* TRANSLATORS: button text, move from old -> new */
			gtk_dialog_add_button(GTK_DIALOG(dialog), _("Upgrade"), GTK_RESPONSE_OK);
		} else if (downgrade) {
			self->flags |= FWUPD_INSTALL_FLAG_ALLOW_OLDER;
			/* TRANSLATORS: button text, move from new -> old */
			gtk_dialog_add_button(GTK_DIALOG(dialog), _("Downgrade"), GTK_RESPONSE_OK);
		}
	}
	if (fwupd_device_has_flag(self->device, FWUPD_DEVICE_FLAG_USABLE_DURING_UPDATE)) {
		gtk_message_dialog_format_secondary_text(
		    GTK_MESSAGE_DIALOG(dialog),
		    "%s",
		    /* TRANSLATORS: the device can be used normallly */
		    _("The device will remain usable for the duration of the update"));
	} else {
		gtk_message_dialog_format_secondary_text(
		    GTK_MESSAGE_DIALOG(dialog),
		    "%s",
		    /* TRANSLATORS: the device will be non-fuctional for a while */
		    _("The device will be unusable while the update is installing"));
	}

	/* success */
	return dialog;
}

static void
gfu_main_release_install_response(GfuMain *self);

static void
gfu_main_release_install_response_cb(GtkDialog *dialog, int response_id, GfuMain *self)
{
	gtk_window_destroy(GTK_WINDOW(dialog));

	if (response_id != GTK_RESPONSE_OK) {
		g_ptr_array_set_size(self->actions, 0);
		return;
	}

	g_ptr_array_remove(self->actions, dialog);
	gfu_main_release_install_response(self);
}

static void
gfu_main_release_install_response(GfuMain *self)
{
	GtkWidget *dialog;

	if (self->actions->len == 0) {
		gfu_main_release_install(self);
		return;
	}
	dialog = GTK_WIDGET(g_ptr_array_index(self->actions, 0));
	g_signal_connect(dialog,
			 "response",
			 G_CALLBACK(gfu_main_release_install_response_cb),
			 self);
	gtk_window_present(GTK_WINDOW(dialog));
}

static void
gfu_main_release_row_button_clicked_cb(GfuReleaseRow *row, GfuMain *self)
{
	FwupdRelease *release = gfu_release_row_get_release(GFU_RELEASE_ROW(row));

	g_set_object(&self->release, release);

	g_ptr_array_add(self->actions, gfu_main_release_show_confirmation(self));
	if (g_strcmp0(fwupd_device_get_branch(self->device),
		      fwupd_release_get_branch(self->release)) != 0) {
		g_ptr_array_add(self->actions, gfu_main_release_show_branch_warning(self));
	}
	if (fwupd_device_has_flag(self->device, FWUPD_DEVICE_FLAG_AFFECTS_FDE))
		g_ptr_array_add(self->actions, gfu_main_release_show_fde_warning(self));
	gfu_main_release_install_response(self);
}

static void
gfu_main_button_back_cb(GtkWidget *widget, GfuMain *self)
{
	AdwLeaflet *leaflet = ADW_LEAFLET(gtk_builder_get_object(self->builder, "leaflet"));
	adw_leaflet_navigate(leaflet, ADW_NAVIGATION_DIRECTION_BACK);
}

static void
gfu_main_device_verify_cb(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	GfuMain *self = (GfuMain *)user_data;
	GtkWidget *window = GTK_WIDGET(gtk_builder_get_object(self->builder, "dialog_main"));
	GtkWidget *dialog;
	g_autoptr(GError) error = NULL;

	if (!fwupd_client_verify_finish(self->client, res, &error)) {
		/* TRANSLATORS: verify means checking the actual checksum of the firmware */
		gfu_main_error_dialog(self, _("Failed to verify firmware"), error->message);
		return;
	}

	dialog = gtk_message_dialog_new(GTK_WINDOW(window),
					GTK_DIALOG_MODAL,
					GTK_MESSAGE_INFO,
					GTK_BUTTONS_OK,
					"%s",
					/* TRANSLATORS: inform the user that the
					   firmware verification was successful */
					_("Verification succeeded"));
	gtk_message_dialog_format_secondary_text(
	    GTK_MESSAGE_DIALOG(dialog),
	    /* TRANSLATORS: firmware is cryptographically identical */
	    _("%s firmware checksums matched"),
	    fwupd_device_get_name(self->device));
	g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
	gtk_window_present(GTK_WINDOW(dialog));
	gfu_main_refresh_device(self);
}

static void
gfu_main_device_verify_response_cb(GtkDialog *dialog_parent, int response_id, GfuMain *self)
{
	if (response_id == GTK_RESPONSE_YES) {
		fwupd_client_verify_async(self->client,
					  fwupd_device_get_id(self->device),
					  self->cancellable,
					  gfu_main_device_verify_cb,
					  self);
	}
	gtk_window_destroy(GTK_WINDOW(dialog_parent));
}

static void
gfu_main_device_verify_clicked_cb(GtkWidget *widget, GfuMain *self)
{
	GtkWidget *dialog;
	GtkWidget *window = GTK_WIDGET(gtk_builder_get_object(self->builder, "dialog_main"));

	dialog = gtk_message_dialog_new(GTK_WINDOW(window),
					GTK_DIALOG_MODAL,
					GTK_MESSAGE_QUESTION,
					GTK_BUTTONS_YES_NO,
					"%s",
					/* TRANSLATORS: dialog title */
					_("Verify firmware checksums?"));
	gtk_message_dialog_format_secondary_text(
	    GTK_MESSAGE_DIALOG(dialog),
	    "%s",
	    /* TRANSLATORS: device will "go away" and then "come back" */
	    _("The device may be unusable during this action"));
	g_signal_connect(dialog, "response", G_CALLBACK(gfu_main_device_verify_response_cb), self);
	gtk_window_present(GTK_WINDOW(dialog));
}

static void
gfu_main_verify_update_cb(GObject *source, GAsyncResult *res, gpointer user_data)
{
	GfuMain *self = (GfuMain *)user_data;
	g_autoptr(GError) error = NULL;

	if (!fwupd_client_verify_update_finish(FWUPD_CLIENT(source), res, &error)) {
		/* TRANSLATORS: verify means checking the actual checksum of the firmware */
		gfu_main_error_dialog(self, _("Failed to update checksums"), error->message);
		return;
	}
	gfu_main_refresh_device(self);
}

static void
gfu_main_device_verify_update_response_cb(GtkDialog *dialog, int response_id, GfuMain *self)
{
	if (response_id == GTK_RESPONSE_YES) {
		fwupd_client_verify_update_async(self->client,
						 fwupd_device_get_id(self->device),
						 self->cancellable,
						 gfu_main_verify_update_cb,
						 self);
	}
	gtk_window_destroy(GTK_WINDOW(dialog));
}

static void
gfu_main_device_verify_update_cb(GtkWidget *widget, GfuMain *self)
{
	GtkWidget *dialog;
	GtkWidget *window = GTK_WIDGET(gtk_builder_get_object(self->builder, "dialog_main"));

	dialog = gtk_message_dialog_new(GTK_WINDOW(window),
					GTK_DIALOG_MODAL,
					GTK_MESSAGE_QUESTION,
					GTK_BUTTONS_YES_NO,
					"%s",
					/* TRANSLATORS: dialog title */
					_("Update cryptographic hash"));
	gtk_message_dialog_format_secondary_text(
	    GTK_MESSAGE_DIALOG(dialog),
	    "%s",
	    /* TRANSLATORS: save what we have now as "valid" */
	    _("Record current device cryptographic hashes as verified?"));
	g_signal_connect(dialog,
			 "response",
			 G_CALLBACK(gfu_main_device_verify_update_response_cb),
			 self);
	gtk_window_present(GTK_WINDOW(dialog));
}

static void
gfu_main_about_activated_cb(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GfuMain *self = (GfuMain *)user_data;
	GList *windows;
	GtkWindow *parent = NULL;
	const gchar *authors[] = {"Richard Hughes", "Andrew Schwenn", NULL};
	const gchar *copyright = "Copyright \xc2\xa9 2019 Richard Hughes";

	windows = gtk_application_get_windows(GTK_APPLICATION(self->application));
	if (windows)
		parent = windows->data;

	gtk_show_about_dialog(parent,
			      "title",
			      /* TRANSLATORS: the title of the about window */
			      _("About GNOME Firmware"),
			      "program-name",
			      /* TRANSLATORS: the application name for the about UI */
			      _("GNOME Firmware"),
			      "authors",
			      authors,
			      "comments",
			      /* TRANSLATORS: one-line description for the app */
			      _("Install firmware on devices"),
			      "copyright",
			      copyright,
			      "license-type",
			      GTK_LICENSE_GPL_2_0,
			      "logo-icon-name",
			      "org.gnome.Firmware",
			      "translator-credits",
			      /* TRANSLATORS: you can put your name here :) */
			      _("translator-credits"),
			      "version",
			      VERSION,
			      NULL);
}

static void
gfu_main_quit_activated_cb(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	GfuMain *self = (GfuMain *)user_data;
	g_application_quit(G_APPLICATION(self->application));
}

static GActionEntry actions[] = {{"about", gfu_main_about_activated_cb, NULL, NULL, NULL},
				 {"refresh", gfu_main_activate_refresh_metadata, NULL, NULL, NULL},
				 {"quit", gfu_main_quit_activated_cb, NULL, NULL, NULL}};

static void
gfu_main_refresh_release(GfuMain *self)
{
	GtkWidget *w;
	GPtrArray *cats = NULL;
	GPtrArray *checks = NULL;
	GPtrArray *issues = NULL;
	g_autofree gchar *desc = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GString) attr = g_string_new(NULL);

	if (self->release == NULL)
		return;

	cats = fwupd_release_get_categories(self->release);
	if (cats->len == 0) {
		gfu_main_set_label(self, "label_release_categories", NULL);
	} else {
		for (guint i = 0; i < cats->len; i++) {
			g_string_append_printf(attr,
					       "%s\n",
					       (const gchar *)g_ptr_array_index(cats, i));
		}
		if (attr->len > 0)
			g_string_truncate(attr, attr->len - 1);
		gfu_main_set_label(self, "label_release_categories", attr->str);
		w = GTK_WIDGET(
		    gtk_builder_get_object(self->builder, "label_release_categories_title"));
		adw_preferences_row_set_title(ADW_PREFERENCES_ROW(w),
					      ngettext("Category", "Categories", cats->len));
	}

	g_string_set_size(attr, 0);
	checks = fwupd_release_get_checksums(self->release);
	if (checks->len == 0) {
		gfu_main_set_label(self, "label_release_checksum", NULL);
	} else {
		for (guint i = 0; i < checks->len; i++) {
			g_autofree gchar *tmp =
			    gfu_common_checksum_format(g_ptr_array_index(checks, i));
			g_string_append_printf(attr, "%s\n", tmp);
		}
		if (attr->len > 0)
			g_string_truncate(attr, attr->len - 1);
		gfu_main_set_label(self, "label_release_checksum", attr->str);
		w = GTK_WIDGET(
		    gtk_builder_get_object(self->builder, "label_release_checksum_title"));
		adw_preferences_row_set_title(ADW_PREFERENCES_ROW(w),
					      ngettext("Checksum", "Checksums", checks->len));
	}

	issues = fwupd_release_get_issues(self->release);
	if (issues->len == 0) {
		gfu_main_set_label(self, "label_release_issues", NULL);
	} else {
		g_autoptr(GString) str = g_string_new(NULL);
		for (guint i = 0; i < issues->len; i++) {
			const gchar *tmp = g_ptr_array_index(issues, i);
			g_string_append_printf(str, "%s\n", tmp);
		}
		if (str->len > 0)
			g_string_truncate(str, str->len - 1);
		gfu_main_set_label(self, "label_release_issues", str->str);
		gfu_main_set_label_title(self,
					 "label_release_issues",
					 /* TRANSLATORS: list of security issues, e.g. CVEs */
					 ngettext("Fixed Issue", "Fixed Issues", checks->len));
	}

	gfu_main_set_label(self,
			   "label_release_filename",
			   fwupd_release_get_filename(self->release));
	gfu_main_set_label(self,
			   "label_release_protocol",
			   fwupd_release_get_protocol(self->release));
	gfu_main_set_label(self,
			   "label_release_appstream_id",
			   fwupd_release_get_appstream_id(self->release));
	gfu_main_set_label(self,
			   "label_release_remote_id",
			   fwupd_release_get_remote_id(self->release));
	gfu_main_set_label(self, "label_release_vendor", fwupd_release_get_vendor(self->release));
	gfu_main_set_label(self, "label_release_summary", fwupd_release_get_summary(self->release));

	g_string_set_size(attr, 0);
	desc = gfu_common_xml_to_text(fwupd_release_get_description(self->release), &error);
	if (desc == NULL) {
		g_debug("failed to get release description for version %s: %s",
			fwupd_release_get_version(self->release),
			error->message);
		gfu_main_set_label(self, "label_release_description", NULL);
	} else {
		g_string_append(attr, desc);
		if (attr->len > 0)
			g_string_truncate(attr, attr->len - 1);
		gfu_main_set_label(self, "label_release_description", attr->str);
	}

	gfu_main_set_label(self,
			   "label_release_size",
			   g_format_size(fwupd_release_get_size(self->release)));

	if (g_strcmp0(fwupd_release_get_license(self->release), "LicenseRef-proprietary") == 0) {
		/* TRANSLATORS: as in non-free */
		gfu_main_set_label(self, "label_release_license", _("Proprietary"));
	} else {
		gfu_main_set_label(self,
				   "label_release_license",
				   fwupd_release_get_license(self->release));
	}
	gfu_main_set_label(
	    self,
	    "label_release_flags",
	    gfu_common_release_flags_to_strings(fwupd_release_get_flags(self->release)));

	gfu_main_set_label(
	    self,
	    "label_release_install_duration",
	    gfu_common_seconds_to_string(fwupd_release_get_install_duration(self->release)));

	gfu_main_set_label(self,
			   "label_release_update_message",
			   fwupd_release_get_update_message(self->release));
}

static void
gfu_main_release_row_selected_cb(GtkListBox *box, GtkListBoxRow *row, GfuMain *self)
{
	FwupdRelease *release = gfu_release_row_get_release(GFU_RELEASE_ROW(row));
	GtkWidget *w;
	GtkWidget *leaflet = GTK_WIDGET(gtk_builder_get_object(self->builder, "leaflet"));

	g_set_object(&self->release, release);
	gfu_main_refresh_release(self);

	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "release_window_title"));
	adw_window_title_set_title(ADW_WINDOW_TITLE(w), fwupd_release_get_version(release));

	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "window_release"));
	gtk_window_set_default_size(GTK_WINDOW(w),
				    gtk_widget_get_width(leaflet) * 85 / 100,
				    gtk_widget_get_height(leaflet) * 60 / 100);
	gtk_window_present(GTK_WINDOW(w));
}

static void
gfu_main_refresh_releases(GfuMain *self)
{
	GtkWidget *w;

	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "group_releases"));
	gtk_widget_set_visible(w, FALSE);
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "listbox_releases"));
	gfu_main_list_box_remove_all(GTK_LIST_BOX(w));

	/* async call for releases */
	if (self->device != NULL &&
	    (fwupd_device_has_flag(self->device, FWUPD_DEVICE_FLAG_UPDATABLE) ||
	     fwupd_device_has_flag(self->device, FWUPD_DEVICE_FLAG_UPDATABLE_HIDDEN))) {
		fwupd_client_get_releases_async(self->client,
						fwupd_device_get_id(self->device),
						self->cancellable,
						(GAsyncReadyCallback)gfu_main_get_releases_cb,
						self);
	} else {
		gfu_main_update_releases(self, NULL);
	}
}

static void
gfu_main_device_row_selected_cb(GtkListBox *box, GtkListBoxRow *row, GfuMain *self)
{
	AdwLeaflet *leaflet = ADW_LEAFLET(gtk_builder_get_object(self->builder, "leaflet"));
	FwupdDevice *device;

	if (row == NULL)
		return;

	adw_leaflet_navigate(leaflet, ADW_NAVIGATION_DIRECTION_FORWARD);

	device = gfu_device_row_get_device(GFU_DEVICE_ROW(row));
	g_set_object(&self->device, device);
	gfu_main_refresh_device(self);
}

static void
gfu_main_set_feature_flags_cb(GObject *source, GAsyncResult *res, gpointer user_data)
{
	GfuMain *self = (GfuMain *)user_data;
	g_autoptr(GError) error = NULL;
	if (!fwupd_client_set_feature_flags_finish(FWUPD_CLIENT(source), res, &error)) {
		g_warning("%s", error->message);
		if (g_error_matches(error, FWUPD_ERROR, FWUPD_ERROR_NOT_SUPPORTED)) {
			gfu_main_error_fatal(self,
					     /* TRANSLATORS: maybe try Linux? */
					     _("The fwupd service is not available for your OS."));
			return;
		}
	}
	fwupd_client_get_devices_async(self->client,
				       self->cancellable,
				       gfu_main_update_devices_cb,
				       self);
	fwupd_client_get_remotes_async(self->client,
				       self->cancellable,
				       gfu_main_update_remotes_cb,
				       self);
}

static void
gfu_main_unlock_cb(GObject *source, GAsyncResult *res, gpointer user_data)
{
	GfuMain *self = (GfuMain *)user_data;
	GtkWindow *window;
	GtkWidget *dialog;
	g_autoptr(GError) error = NULL;

	if (!fwupd_client_verify_update_finish(FWUPD_CLIENT(source), res, &error)) {
		/* TRANSLATORS: unlock means to make the device functional in another mode */
		gfu_main_error_dialog(self, _("Failed to unlock device"), error->message);
		return;
	}

	/* if successful, prompt for reboot */
	window = GTK_WINDOW(gtk_builder_get_object(self->builder, "dialog_main"));
	if (fwupd_device_has_flag(self->device, FWUPD_DEVICE_FLAG_NEEDS_SHUTDOWN)) {
		dialog = gtk_message_dialog_new(
		    window,
		    GTK_DIALOG_MODAL,
		    GTK_MESSAGE_QUESTION,
		    GTK_BUTTONS_YES_NO,
		    "%s",
		    /* TRANSLATORS: prompting a shutdown to the user */
		    _("Unlocking a device requires the system to shutdown to complete."));
		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
							 "%s",
							 _("Shutdown now?"));
		g_signal_connect(dialog,
				 "response",
				 G_CALLBACK(gfu_main_reboot_shutdown_response_cb),
				 self);
		gtk_window_present(GTK_WINDOW(dialog));
	} else if (fwupd_device_has_flag(self->device, FWUPD_DEVICE_FLAG_NEEDS_REBOOT)) {
		dialog =
		    gtk_message_dialog_new(window,
					   GTK_DIALOG_MODAL,
					   GTK_MESSAGE_QUESTION,
					   GTK_BUTTONS_YES_NO,
					   "%s",
					   /* TRANSLATORS: prompting a reboot to the user */
					   _("Unlocking a device requires a reboot to complete."));
		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
							 "%s",
							 /* TRANSLATORS: button text */
							 _("Restart now?"));
		g_signal_connect(dialog,
				 "response",
				 G_CALLBACK(gfu_main_reboot_restart_response_cb),
				 self);
		gtk_window_present(GTK_WINDOW(dialog));
	}
}

static void
gfu_main_device_unlock_cb(GtkWidget *widget, gboolean state, GfuMain *self)
{
	if (state)
		return;
	fwupd_client_unlock_async(self->client,
				  fwupd_device_get_id(self->device),
				  self->cancellable,
				  gfu_main_unlock_cb,
				  self);
}

static void
gfu_main_infobar_close_cb(GtkInfoBar *infobar, gpointer user_data)
{
	gtk_info_bar_set_revealed(infobar, FALSE);
}

static void
gfu_main_infobar_response_cb(GtkInfoBar *infobar, gint response_id, gpointer user_data)
{
	if (response_id == GTK_RESPONSE_CLOSE)
		gtk_info_bar_set_revealed(infobar, FALSE);
}

static void
gfu_main_leaflet_notify_folded_cb(AdwLeaflet *leaflet, GParamSpec *pspec, GfuMain *self)
{
	GtkWidget *w;
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "listbox_devices"));
	gtk_list_box_set_selection_mode(GTK_LIST_BOX(w),
					adw_leaflet_get_folded(leaflet) ? GTK_SELECTION_NONE
									: GTK_SELECTION_SINGLE);
}

static void
gfu_main_client_connect_cb(GObject *source, GAsyncResult *res, gpointer user_data)
{
	GfuMain *self = (GfuMain *)user_data;
	g_autoptr(GError) error = NULL;

	/* get result */
	if (!fwupd_client_connect_finish(FWUPD_CLIENT(source), res, &error)) {
		g_warning("%s", error->message);
		/* TRANSLATORS: maybe try Linux? */
		gfu_main_error_fatal(self, _("The fwupd service is not available for your OS."));
		return;
	}

	/* set user agent */
	fwupd_client_set_user_agent_for_package(self->client, GETTEXT_PACKAGE, VERSION);

	/* get data */
	fwupd_client_set_feature_flags_async(
	    self->client,
	    FWUPD_FEATURE_FLAG_FDE_WARNING | FWUPD_FEATURE_FLAG_SWITCH_BRANCH |
#if FWUPD_CHECK_VERSION(1, 8, 1)
		FWUPD_FEATURE_FLAG_SHOW_PROBLEMS |
#endif
		FWUPD_FEATURE_FLAG_REQUESTS | FWUPD_FEATURE_FLAG_UPDATE_ACTION,
	    self->cancellable,
	    gfu_main_set_feature_flags_cb,
	    self);
}

static void
gfu_main_startup_cb(GApplication *application, GfuMain *self)
{
	GtkWidget *w;
	GtkWidget *main_window;

	/* add application menu items */
	g_action_map_add_action_entries(G_ACTION_MAP(application),
					actions,
					G_N_ELEMENTS(actions),
					self);

	/* get UI */
	self->builder = gtk_builder_new_from_resource("/org/gnome/Firmware/gfu-main.ui");

	main_window = GTK_WIDGET(gtk_builder_get_object(self->builder, "dialog_main"));
	gtk_application_add_window(GTK_APPLICATION(self->application), GTK_WINDOW(main_window));
	gtk_window_set_default_size(GTK_WINDOW(main_window), 1024, 600);

	/* hide window first so that the dialogue resizes itself without redrawing */
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "stack_main"));
	gtk_stack_set_visible_child_name(GTK_STACK(w), "loading");

	/* buttons */
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "switch_device_unlock"));
	g_signal_connect(w, "state-set", G_CALLBACK(gfu_main_device_unlock_cb), self);
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "button_verify"));
	g_signal_connect(w, "clicked", G_CALLBACK(gfu_main_device_verify_clicked_cb), self);
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "button_verify_update"));
	g_signal_connect(w, "clicked", G_CALLBACK(gfu_main_device_verify_update_cb), self);
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "button_back"));
	g_signal_connect(w, "clicked", G_CALLBACK(gfu_main_button_back_cb), self);
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "button_infobar_enable_lvfs"));
	g_signal_connect(w, "clicked", G_CALLBACK(gfu_main_enable_lvfs_cb), self);
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "listbox_devices"));
	g_signal_connect(w, "row-activated", G_CALLBACK(gfu_main_device_row_selected_cb), self);
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "listbox_releases"));
	g_signal_connect(w, "row-activated", G_CALLBACK(gfu_main_release_row_selected_cb), self);
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "infobar_enable_lvfs"));
	g_signal_connect(w, "close", G_CALLBACK(gfu_main_infobar_close_cb), self);
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "infobar_enable_lvfs"));
	g_signal_connect(w, "response", G_CALLBACK(gfu_main_infobar_response_cb), self);
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "leaflet"));
	g_signal_connect(w, "notify::folded", G_CALLBACK(gfu_main_leaflet_notify_folded_cb), self);

	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "window_release"));
	g_signal_connect(ADW_WINDOW(w), "close-request", G_CALLBACK(gtk_widget_hide), self);

	/* sort by parent */
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "listbox_devices"));
	gtk_list_box_set_sort_func(GTK_LIST_BOX(w), gfu_main_sort_device_list_box_cb, self, NULL);

	/* start spinner */
	w = GTK_WIDGET(gtk_builder_get_object(self->builder, "spinner_loading"));
	gtk_spinner_start(GTK_SPINNER(w));

	/* show main UI */
	gtk_widget_show(main_window);
	gfu_main_update_progress_bar(self);

	/* connect to fwupd */
	fwupd_client_connect_async(self->client,
				   self->cancellable,
				   gfu_main_client_connect_cb,
				   self);
}

static void
gfu_main_client_device_changed_cb(FwupdClient *client, FwupdDevice *device, GfuMain *self)
{
	/* same as last time, so just refresh */
	if (self->device != NULL && fwupd_device_compare(self->device, device) == 0) {
		g_set_object(&self->device, device);
		gfu_main_refresh_device(self);
		return;
	}
}

static void
gfu_main_device_request_cb(FwupdClient *client, FwupdRequest *request, GfuMain *self)
{
	/* nothing sensible to show */
	if (fwupd_request_get_message(request) == NULL)
		return;

	/* show this now */
	if (fwupd_request_get_kind(request) == FWUPD_REQUEST_KIND_IMMEDIATE) {
		gfu_main_show_request(self, request);
		return;
	}

	/* save for later */
	if (fwupd_request_get_kind(request) == FWUPD_REQUEST_KIND_POST)
		g_ptr_array_add(self->post_requests, g_object_ref(request));
}

static void
gfu_main_free(GfuMain *self)
{
	if (self->refresh_id != 0)
		g_source_remove(self->refresh_id);
	if (self->builder != NULL)
		g_object_unref(self->builder);
	if (self->cancellable != NULL)
		g_object_unref(self->cancellable);
	if (self->device != NULL)
		g_object_unref(self->device);
	if (self->release != NULL)
		g_object_unref(self->release);
	if (self->client != NULL)
		g_object_unref(self->client);
	if (self->application != NULL)
		g_object_unref(self->application);
	if (self->actions != NULL)
		g_ptr_array_unref(self->actions);
	if (self->post_requests != NULL)
		g_ptr_array_unref(self->post_requests);
	g_free(self);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GfuMain, gfu_main_free)

int
main(int argc, char **argv)
{
	gboolean verbose = FALSE;
	g_autoptr(GError) error = NULL;
	g_autoptr(GfuMain) self = g_new0(GfuMain, 1);
	g_autoptr(GOptionContext) context = NULL;
	const GOptionEntry options[] = {{"verbose",
					 'v',
					 0,
					 G_OPTION_ARG_NONE,
					 &verbose,
					 /* TRANSLATORS: command line option */
					 N_("Show extra debugging information"),
					 NULL},
					{NULL}};

	setlocale(LC_ALL, "");

	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	/* TRANSLATORS: command description */
	context = g_option_context_new(_("GNOME Firmware"));
	g_option_context_add_main_entries(context, options, NULL);
	if (!g_option_context_parse(context, &argc, &argv, &error)) {
		/* TRANSLATORS: the user has sausages for fingers */
		g_print("%s: %s\n", _("Failed to parse command line options"), error->message);
		return EXIT_FAILURE;
	}

	self->cancellable = g_cancellable_new();
	self->client = fwupd_client_new();
	g_signal_connect(self->client,
			 "notify::percentage",
			 G_CALLBACK(gfu_main_client_percentage_changed_cb),
			 self);
	g_signal_connect(self->client,
			 "notify::status",
			 G_CALLBACK(gfu_main_client_status_changed_cb),
			 self);
	g_signal_connect(self->client,
			 "device-changed",
			 G_CALLBACK(gfu_main_client_device_changed_cb),
			 self);
	g_signal_connect(self->client, "device-added", G_CALLBACK(gfu_main_device_added_cb), self);
	g_signal_connect(self->client,
			 "device-removed",
			 G_CALLBACK(gfu_main_device_removed_cb),
			 self);
	/* required for interactive devices */
	g_signal_connect(FWUPD_CLIENT(self->client),
			 "device-request",
			 G_CALLBACK(gfu_main_device_request_cb),
			 self);

	self->actions = g_ptr_array_new_with_free_func((GDestroyNotify)gtk_window_destroy);
	self->post_requests = g_ptr_array_new_with_free_func((GDestroyNotify)g_object_unref);

	/* ensure single instance */
#if GLIB_CHECK_VERSION(2, 74, 0)
	self->application = adw_application_new("org.gnome.Firmware", G_APPLICATION_DEFAULT_FLAGS);
#else
	self->application = adw_application_new("org.gnome.Firmware", G_APPLICATION_FLAGS_NONE);
#endif
	g_signal_connect(self->application, "startup", G_CALLBACK(gfu_main_startup_cb), self);
	g_signal_connect(self->application, "activate", G_CALLBACK(gfu_main_activate_cb), self);
	/* set verbose? */
	if (verbose)
		g_setenv("G_MESSAGES_DEBUG", "all", FALSE);

	/* wait */
	return g_application_run(G_APPLICATION(self->application), argc, argv);
}
