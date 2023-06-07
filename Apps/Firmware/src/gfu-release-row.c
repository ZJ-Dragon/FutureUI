/*
 * Copyright (C) 2012 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include "config.h"

#include <glib/gi18n.h>

#include "gfu-release-row.h"

typedef struct {
	FwupdDevice *device;
	FwupdRelease *release;
	GtkWidget *image;
	GtkWidget *name;
	GtkWidget *version;
	GtkWidget *button;
	guint pending_refresh_id;
} GfuReleaseRowPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GfuReleaseRow, gfu_release_row, GTK_TYPE_LIST_BOX_ROW)

enum { SIGNAL_BUTTON_CLICKED, SIGNAL_LAST };

static guint signals[SIGNAL_LAST] = {0};

static void
gfu_release_row_refresh(GfuReleaseRow *self)
{
	const gchar *tmp;

	GfuReleaseRowPrivate *priv = gfu_release_row_get_instance_private(self);
	if (priv->release == NULL)
		return;

	switch (fwupd_release_get_urgency(priv->release)) {
	case FWUPD_RELEASE_URGENCY_HIGH:
	case FWUPD_RELEASE_URGENCY_CRITICAL:
		gtk_widget_set_visible(priv->image, TRUE);
		gtk_image_set_from_icon_name(GTK_IMAGE(priv->image),
					     "software-update-urgent-symbolic");
		break;
	default:
		gtk_widget_set_visible(priv->image, FALSE);
		break;
	}

	tmp = fwupd_release_get_name(priv->release);
	gtk_label_set_label(GTK_LABEL(priv->name), tmp);
	tmp = fwupd_release_get_version(priv->release);
	gtk_label_set_label(GTK_LABEL(priv->version), tmp);

	if (fwupd_release_has_flag(priv->release, FWUPD_RELEASE_FLAG_IS_UPGRADE)) {
		/* TRANSLATORS: upgrading the firmware */
		gtk_button_set_label(GTK_BUTTON(priv->button), _("Upgrade"));
	} else if (fwupd_release_has_flag(priv->release, FWUPD_RELEASE_FLAG_IS_DOWNGRADE)) {
		/* TRANSLATORS: downgrading the firmware */
		gtk_button_set_label(GTK_BUTTON(priv->button), _("Downgrade"));
	} else {
		/* TRANSLATORS: installing the same firmware that is
		 * currently installed */
		gtk_button_set_label(GTK_BUTTON(priv->button), _("Reinstall"));
	}

	/* only mark active if updatable; there might be a problem */
	if (fwupd_device_has_flag(priv->device, FWUPD_DEVICE_FLAG_UPDATABLE)) {
		gtk_widget_set_tooltip_text(priv->button, NULL);
		gtk_widget_set_sensitive(priv->button, TRUE);
	} else {
		gtk_widget_set_tooltip_text(
		    priv->button,
		    /* TRANSLATORS: problems are things the user has to fix, e.g. low battery */
		    _("Cannot perform action as the device has a problem"));
		gtk_widget_set_sensitive(priv->button, FALSE);
	}
}

FwupdRelease *
gfu_release_row_get_release(GfuReleaseRow *self)
{
	GfuReleaseRowPrivate *priv = gfu_release_row_get_instance_private(self);
	g_return_val_if_fail(GFU_IS_RELEASE_ROW(self), NULL);
	return priv->release;
}

static gboolean
gfu_release_row_refresh_idle_cb(gpointer user_data)
{
	GfuReleaseRow *self = GFU_RELEASE_ROW(user_data);
	GfuReleaseRowPrivate *priv = gfu_release_row_get_instance_private(self);
	priv->pending_refresh_id = 0;
	gfu_release_row_refresh(self);
	return FALSE;
}

static void
gfu_release_row_notify_props_changed_cb(FwupdRelease *release,
					GParamSpec *pspec,
					GfuReleaseRow *self)
{
	GfuReleaseRowPrivate *priv = gfu_release_row_get_instance_private(self);
	if (priv->pending_refresh_id > 0)
		return;
	priv->pending_refresh_id = g_idle_add(gfu_release_row_refresh_idle_cb, self);
}

static void
gfu_release_row_set_release(GfuReleaseRow *self, FwupdRelease *release)
{
	GfuReleaseRowPrivate *priv = gfu_release_row_get_instance_private(self);

	priv->release = g_object_ref(release);

	g_signal_connect_object(priv->release,
				"notify::state",
				G_CALLBACK(gfu_release_row_notify_props_changed_cb),
				self,
				0);
	gfu_release_row_refresh(self);
}

static void
gfu_release_row_set_device(GfuReleaseRow *self, FwupdDevice *device)
{
	GfuReleaseRowPrivate *priv = gfu_release_row_get_instance_private(self);
	priv->device = g_object_ref(device);
	g_signal_connect_object(priv->device,
				"notify::flags",
				G_CALLBACK(gfu_release_row_notify_props_changed_cb),
				self,
				0);
}

static void
gfu_release_row_dispose(GObject *object)
{
	GfuReleaseRow *self = GFU_RELEASE_ROW(object);
	GfuReleaseRowPrivate *priv = gfu_release_row_get_instance_private(self);

	if (priv->release != NULL)
		g_signal_handlers_disconnect_by_func(priv->release,
						     gfu_release_row_notify_props_changed_cb,
						     self);

	g_clear_object(&priv->device);
	g_clear_object(&priv->release);
	if (priv->pending_refresh_id != 0) {
		g_source_remove(priv->pending_refresh_id);
		priv->pending_refresh_id = 0;
	}

	G_OBJECT_CLASS(gfu_release_row_parent_class)->dispose(object);
}

static void
gfu_release_row_class_init(GfuReleaseRowClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	object_class->dispose = gfu_release_row_dispose;
	gtk_widget_class_set_template_from_resource(widget_class,
						    "/org/gnome/Firmware/gfu-release-row.ui");

	signals[SIGNAL_BUTTON_CLICKED] =
	    g_signal_new("button-clicked",
			 G_TYPE_FROM_CLASS(object_class),
			 G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET(GfuReleaseRowClass, button_clicked),
			 NULL,
			 NULL,
			 g_cclosure_marshal_VOID__VOID,
			 G_TYPE_NONE,
			 0);

	gtk_widget_class_bind_template_child_private(widget_class, GfuReleaseRow, image);
	gtk_widget_class_bind_template_child_private(widget_class, GfuReleaseRow, name);
	gtk_widget_class_bind_template_child_private(widget_class, GfuReleaseRow, version);
	gtk_widget_class_bind_template_child_private(widget_class, GfuReleaseRow, button);
}

static void
gfu_release_row_button_clicked_cb(GtkWidget *widget, GfuReleaseRow *self)
{
	g_signal_emit(self, signals[SIGNAL_BUTTON_CLICKED], 0);
}

static void
gfu_release_row_init(GfuReleaseRow *self)
{
	GfuReleaseRowPrivate *priv = gfu_release_row_get_instance_private(self);

	gtk_widget_init_template(GTK_WIDGET(self));

	g_signal_connect(GTK_BUTTON(priv->button),
			 "clicked",
			 G_CALLBACK(gfu_release_row_button_clicked_cb),
			 self);
}

GtkWidget *
gfu_release_row_new(FwupdDevice *device, FwupdRelease *release)
{
	GtkWidget *self;
	g_return_val_if_fail(FWUPD_IS_RELEASE(release), NULL);
	self = g_object_new(GFU_TYPE_RELEASE_ROW, NULL);
	gfu_release_row_set_device(GFU_RELEASE_ROW(self), device);
	gfu_release_row_set_release(GFU_RELEASE_ROW(self), release);
	return self;
}
