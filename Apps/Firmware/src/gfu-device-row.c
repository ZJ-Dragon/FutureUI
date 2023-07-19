/*
 * Copyright (C) 2012 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include "config.h"

#include "gfu-device-row.h"

typedef struct {
	FwupdDevice *device;
	GtkWidget *image;
	GtkWidget *name;
	guint pending_refresh_id;
} GfuDeviceRowPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GfuDeviceRow, gfu_device_row, GTK_TYPE_LIST_BOX_ROW)

static void
gfu_device_row_refresh(GfuDeviceRow *self)
{
	const gchar *tmp;
	GPtrArray *icons;
	GtkIconTheme *icon_theme = gtk_icon_theme_get_for_display(gdk_display_get_default());
	g_autofree gchar *icon_name = NULL;
	g_autoptr(GString) str = g_string_new(NULL);

	GfuDeviceRowPrivate *priv = gfu_device_row_get_instance_private(self);
	if (priv->device == NULL)
		return;

	/* row labels - name */
	tmp = fwupd_device_get_vendor(priv->device);
	if (tmp != NULL)
		g_string_append(str, tmp);
	tmp = fwupd_device_get_name(priv->device);
	if (tmp != NULL) {
		if (str->len > 0)
			g_string_append(str, " ");
		g_string_append(str, tmp);
	}

	gtk_label_set_label(GTK_LABEL(priv->name), str->str);
	gtk_widget_set_margin_start(priv->image,
				    6 + (fwupd_device_get_parent(priv->device) != NULL ? 16 : 0));

	/* set icon, with fallbacks */
	icons = fwupd_device_get_icons(priv->device);
	for (guint i = 0; i < icons->len; i++) {
		const gchar *icon_tmp = g_ptr_array_index(icons, i);
		g_autoptr(GString) icon_symbolic = g_string_new(icon_tmp);

		if (!g_str_has_suffix(icon_symbolic->str, "-symbolic"))
			g_string_append(icon_symbolic, "-symbolic");
		if (gtk_icon_theme_has_icon(icon_theme, icon_symbolic->str)) {
			icon_name = g_string_free(g_steal_pointer(&icon_symbolic), FALSE);
			break;
		}
		g_warning("did not find icon %s", icon_tmp);
	}

	/* ultimate fallback */
	if (icon_name == NULL)
		icon_name = g_strdup("computer-chip-symbolic");

	gtk_image_set_from_icon_name(GTK_IMAGE(priv->image), icon_name);
}

FwupdDevice *
gfu_device_row_get_device(GfuDeviceRow *self)
{
	GfuDeviceRowPrivate *priv = gfu_device_row_get_instance_private(self);
	g_return_val_if_fail(GFU_IS_DEVICE_ROW(self), NULL);
	return priv->device;
}

static gboolean
gfu_device_row_refresh_idle_cb(gpointer user_data)
{
	GfuDeviceRow *self = GFU_DEVICE_ROW(user_data);
	GfuDeviceRowPrivate *priv = gfu_device_row_get_instance_private(self);
	priv->pending_refresh_id = 0;
	gfu_device_row_refresh(self);
	return FALSE;
}

static void
gfu_device_row_notify_props_changed_cb(FwupdDevice *device, GParamSpec *pspec, GfuDeviceRow *self)
{
	GfuDeviceRowPrivate *priv = gfu_device_row_get_instance_private(self);
	if (priv->pending_refresh_id > 0)
		return;
	priv->pending_refresh_id = g_idle_add(gfu_device_row_refresh_idle_cb, self);
}

static void
gfu_device_row_set_device(GfuDeviceRow *self, FwupdDevice *device)
{
	GfuDeviceRowPrivate *priv = gfu_device_row_get_instance_private(self);

	priv->device = g_object_ref(device);

	g_signal_connect_object(priv->device,
				"notify::state",
				G_CALLBACK(gfu_device_row_notify_props_changed_cb),
				self,
				0);
	gfu_device_row_refresh(self);
}

static void
gfu_device_row_dispose(GObject *object)
{
	GfuDeviceRow *self = GFU_DEVICE_ROW(object);
	GfuDeviceRowPrivate *priv = gfu_device_row_get_instance_private(self);

	if (priv->device != NULL)
		g_signal_handlers_disconnect_by_func(priv->device,
						     gfu_device_row_notify_props_changed_cb,
						     self);

	g_clear_object(&priv->device);
	if (priv->pending_refresh_id != 0) {
		g_source_remove(priv->pending_refresh_id);
		priv->pending_refresh_id = 0;
	}

	G_OBJECT_CLASS(gfu_device_row_parent_class)->dispose(object);
}

static void
gfu_device_row_class_init(GfuDeviceRowClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	object_class->dispose = gfu_device_row_dispose;
	gtk_widget_class_set_template_from_resource(widget_class,
						    "/org/gnome/Firmware/gfu-device-row.ui");
	gtk_widget_class_bind_template_child_private(widget_class, GfuDeviceRow, image);
	gtk_widget_class_bind_template_child_private(widget_class, GfuDeviceRow, name);
}

static void
gfu_device_row_init(GfuDeviceRow *self)
{
	gtk_widget_init_template(GTK_WIDGET(self));
}

GtkWidget *
gfu_device_row_new(FwupdDevice *device)
{
	GtkWidget *self;
	g_return_val_if_fail(FWUPD_IS_DEVICE(device), NULL);
	self = g_object_new(GFU_TYPE_DEVICE_ROW, NULL);
	gfu_device_row_set_device(GFU_DEVICE_ROW(self), device);
	return self;
}
