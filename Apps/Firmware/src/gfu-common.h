/*
 * Copyright (C) 2019 Andrew Schwenn <aschwenn@verizon.net>
 * Copyright (C) 2019 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#pragma once

#include <errno.h>
#include <fwupd.h>
#include <xmlb.h>

G_BEGIN_DECLS

/* formatting helper functions */
gchar *
gfu_common_checksum_format(const gchar *checksum);
const gchar *
gfu_common_seconds_to_string(guint64 seconds);
gchar *
gfu_common_xml_to_text(const gchar *xml, GError **error);
const gchar *
gfu_common_device_flag_to_string(guint64 flags);
const gchar *
gfu_common_release_flags_to_strings(guint64 flags);
const gchar *
gfu_status_to_string(FwupdStatus status);
gchar *
gfu_common_strjoin_array(const gchar *separator, GPtrArray *array);

/* handle needs-reboot and needs-shutdown */
gboolean
gfu_common_system_shutdown(GError **error);
gboolean
gfu_common_system_reboot(GError **error);

/* GTK helper functions */
const gchar *
gfu_common_device_flags_to_string(guint64 device_flag);
const gchar *
gfu_common_device_icon_from_flag(FwupdDeviceFlags device_flag);

G_END_DECLS
