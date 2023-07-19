/*
 * Copyright (C) 2019 Andrew Schwenn <aschwenn@verizon.net>
 * Copyright (C) 2019 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include "config.h"

#include <glib/gi18n.h>

#include "gfu-common.h"

/* formatting helper functions */

gchar *
gfu_common_checksum_format(const gchar *checksum)
{
	const gchar *checksum_type;
	guint len;
	/* guess type based on length */
	if (checksum == NULL)
		return NULL;
	len = strlen(checksum);
	if (len == 32)
		checksum_type = "MD5";
	else if (len == 40)
		checksum_type = "SHA1";
	else if (len == 64)
		checksum_type = "SHA256";
	else if (len == 128)
		checksum_type = "SHA512";
	else
		checksum_type = "SHA1";
	return g_strdup_printf("%s(%s)", checksum_type, checksum);
}

const gchar *
gfu_common_seconds_to_string(guint64 seconds)
{
	guint64 minutes, hours;
	if (seconds == 0)
		return NULL;
	if (seconds >= 60) {
		minutes = seconds / 60;
		seconds = seconds % 60;
		if (minutes >= 60) {
			hours = minutes / 60;
			minutes = minutes % 60;
			return g_strdup_printf("%u hr, %u min", (guint)hours, (guint)minutes);
		}
		if (seconds == 0)
			return g_strdup_printf("%u min", (guint)minutes);
		return g_strdup_printf("%u min, %u sec", (guint)minutes, (guint)seconds);
	}
	return g_strdup_printf("%u sec", (guint)seconds);
}

gchar *
gfu_common_xml_to_text(const gchar *xml, GError **error)
{
	g_autoptr(GString) str = g_string_new(NULL);
	g_autoptr(XbNode) n = NULL;
	g_autoptr(XbSilo) silo = NULL;

	if (xml == NULL) {
		g_set_error_literal(error, FWUPD_ERROR, FWUPD_ERROR_INTERNAL, "Null input XML");
		return NULL;
	}

	/* parse XML */
	silo = xb_silo_new_from_xml(xml, error);
	if (silo == NULL)
		return NULL;

	n = xb_silo_get_root(silo);
	while (n != NULL) {
		g_autoptr(XbNode) n2 = NULL;

		/* support <p>, <ul>, <ol> and <li>, ignore all else */
		if (g_strcmp0(xb_node_get_element(n), "p") == 0) {
			g_string_append_printf(str, "%s\n\n", xb_node_get_text(n));
		} else if (g_strcmp0(xb_node_get_element(n), "ul") == 0) {
			g_autoptr(GPtrArray) children = xb_node_get_children(n);
			for (guint i = 0; i < children->len; i++) {
				XbNode *nc = g_ptr_array_index(children, i);
				if (g_strcmp0(xb_node_get_element(nc), "li") == 0) {
					g_string_append_printf(str,
							       " • %s\n",
							       xb_node_get_text(nc));
				}
			}
			g_string_append(str, "\n");
		} else if (g_strcmp0(xb_node_get_element(n), "ol") == 0) {
			g_autoptr(GPtrArray) children = xb_node_get_children(n);
			for (guint i = 0; i < children->len; i++) {
				XbNode *nc = g_ptr_array_index(children, i);
				if (g_strcmp0(xb_node_get_element(nc), "li") == 0) {
					g_string_append_printf(str,
							       " %u. %s\n",
							       i + 1,
							       xb_node_get_text(nc));
				}
			}
			g_string_append(str, "\n");
		}

		n2 = xb_node_get_next(n);
		g_set_object(&n, n2);
	}

	/* remove extra newline */
	if (str->len > 0)
		g_string_truncate(str, str->len - 1);

	/* success */
	return g_string_free(g_steal_pointer(&str), FALSE);
}

const gchar *
gfu_common_device_flag_to_string(guint64 flags)
{
	g_autoptr(GString) str = g_string_new(NULL);
	for (guint j = 0; j < 64; j++) {
		if ((flags & ((guint64)1 << j)) == 0)
			continue;
		g_string_append_printf(str, "%s\n", fwupd_device_flag_to_string((guint64)1 << j));
	}
	if (str->len == 0) {
		g_string_append(str, fwupd_device_flag_to_string(0));
	} else {
		g_string_truncate(str, str->len - 1);
	}
	return g_string_free(g_steal_pointer(&str), FALSE);
}

static const gchar *
gfu_common_release_flag_to_string(FwupdReleaseFlags release_flag)
{
	if (release_flag == FWUPD_RELEASE_FLAG_NONE)
		return NULL;
	if (release_flag == FWUPD_RELEASE_FLAG_TRUSTED_PAYLOAD) {
		/* TRANSLATORS: We verified the payload against the server */
		return _("Trusted payload");
	}
	if (release_flag == FWUPD_RELEASE_FLAG_TRUSTED_METADATA) {
		/* TRANSLATORS: We verified the meatdata against the server */
		return _("Trusted metadata");
	}
	if (release_flag == FWUPD_RELEASE_FLAG_IS_UPGRADE) {
		/* TRANSLATORS: version is newer */
		return _("Is upgrade");
	}
	if (release_flag == FWUPD_RELEASE_FLAG_IS_DOWNGRADE) {
		/* TRANSLATORS: version is older */
		return _("Is downgrade");
	}
	if (release_flag == FWUPD_RELEASE_FLAG_BLOCKED_VERSION) {
		/* TRANSLATORS: version cannot be installed due to policy */
		return _("Blocked version");
	}
	if (release_flag == FWUPD_RELEASE_FLAG_BLOCKED_APPROVAL) {
		/* TRANSLATORS: version cannot be installed due to policy */
		return _("Not approved");
	}
	if (release_flag == FWUPD_RELEASE_FLAG_IS_ALTERNATE_BRANCH) {
		/* TRANSLATORS: is not the main firmware stream */
		return _("Alternate branch");
	}
#if FWUPD_CHECK_VERSION(1, 7, 5)
	if (release_flag == FWUPD_RELEASE_FLAG_IS_COMMUNITY) {
		/* TRANSLATORS: is not supported by the vendor */
		return _("Community supported");
	}
#endif
#if FWUPD_CHECK_VERSION(1, 9, 1)
	if (release_flag == FWUPD_RELEASE_FLAG_TRUSTED_REPORT) {
		/* TRANSLATORS: the QA team is someone we trust */
		return _("Report is trusted");
	}
#endif

	/* fall back for unknown types */
	return fwupd_release_flag_to_string(release_flag);
}

const gchar *
gfu_common_release_flags_to_strings(guint64 flags)
{
	g_autoptr(GString) str = g_string_new(NULL);
	for (guint j = 0; j < 64; j++) {
		if ((flags & ((guint64)1 << j)) == 0)
			continue;
		g_string_append_printf(str,
				       "%s\n",
				       gfu_common_release_flag_to_string((guint64)1 << j));
	}
	if (str->len == 0)
		return NULL;
	g_string_truncate(str, str->len - 1);
	return g_string_free(g_steal_pointer(&str), FALSE);
}

/* handle needs-reboot and needs-shutdown */

gboolean
gfu_common_system_shutdown(GError **error)
{
	g_autoptr(GDBusConnection) connection = NULL;
	g_autoptr(GVariant) val = NULL;

	connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, error);
	if (connection == NULL)
		return FALSE;

#ifdef HAVE_LOGIND
	/* shutdown using logind */
	val = g_dbus_connection_call_sync(connection,
					  "org.freedesktop.login1",
					  "/org/freedesktop/login1",
					  "org.freedesktop.login1.Manager",
					  "PowerOff",
					  g_variant_new("(b)", TRUE),
					  NULL,
					  G_DBUS_CALL_FLAGS_NONE,
					  -1,
					  NULL,
					  error);
#elif defined(HAVE_CONSOLEKIT)
	/* shutdown using ConsoleKit */
	val = g_dbus_connection_call_sync(connection,
					  "org.freedesktop.ConsoleKit",
					  "/org/freedesktop/ConsoleKit/Manager",
					  "org.freedesktop.ConsoleKit.Manager",
					  "Stop",
					  NULL,
					  NULL,
					  G_DBUS_CALL_FLAGS_NONE,
					  -1,
					  NULL,
					  error);
#else
	g_set_error_literal(error,
			    FWUPD_ERROR,
			    FWUPD_ERROR_NOT_SUPPORTED,
			    "No supported backend compiled in to perform the operation.");
#endif
	return val != NULL;
}

gboolean
gfu_common_system_reboot(GError **error)
{
	g_autoptr(GDBusConnection) connection = NULL;
	g_autoptr(GVariant) val = NULL;

	connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, error);
	if (connection == NULL)
		return FALSE;
#ifdef HAVE_LOGIND
	/* reboot using logind */
	val = g_dbus_connection_call_sync(connection,
					  "org.freedesktop.login1",
					  "/org/freedesktop/login1",
					  "org.freedesktop.login1.Manager",
					  "Reboot",
					  g_variant_new("(b)", TRUE),
					  NULL,
					  G_DBUS_CALL_FLAGS_NONE,
					  -1,
					  NULL,
					  error);
#elif defined(HAVE_CONSOLEKIT)
	/* reboot using ConsoleKit */
	val = g_dbus_connection_call_sync(connection,
					  "org.freedesktop.ConsoleKit",
					  "/org/freedesktop/ConsoleKit/Manager",
					  "org.freedesktop.ConsoleKit.Manager",
					  "Restart",
					  NULL,
					  NULL,
					  G_DBUS_CALL_FLAGS_NONE,
					  -1,
					  NULL,
					  error);
#else
	g_set_error_literal(error,
			    FWUPD_ERROR,
			    FWUPD_ERROR,
			    "No supported backend compiled in to perform the operation.");
#endif
	return val != NULL;
}

/* GTK helper functions */

const gchar *
gfu_common_device_flags_to_string(guint64 device_flag)
{
	if (device_flag == FWUPD_DEVICE_FLAG_NONE) {
		return NULL;
	}
	if (device_flag == FWUPD_DEVICE_FLAG_INTERNAL) {
		/* TRANSLATORS: Device cannot be removed easily*/
		return _("Internal device");
	}
	if (device_flag == FWUPD_DEVICE_FLAG_UPDATABLE) {
		/* TRANSLATORS: Device is updatable in this or any other mode */
		return _("Updatable");
	}
	if (device_flag == FWUPD_DEVICE_FLAG_ONLY_OFFLINE) {
		/* TRANSLATORS: Update can only be done from offline mode */
		return _("Update requires a reboot");
	}
	if (device_flag == FWUPD_DEVICE_FLAG_REQUIRE_AC) {
		/* TRANSLATORS: Must be plugged in to an outlet */
		return _("System requires external power source");
	}
	if (device_flag == FWUPD_DEVICE_FLAG_LOCKED) {
		/* TRANSLATORS: Is locked and can be unlocked */
		return _("Device is locked");
	}
	if (device_flag == FWUPD_DEVICE_FLAG_SUPPORTED) {
		/* TRANSLATORS: Is found in current metadata */
		return _("Supported on LVFS");
	}
	if (device_flag == FWUPD_DEVICE_FLAG_NEEDS_BOOTLOADER) {
		/* TRANSLATORS: Requires a bootloader mode to be manually enabled by the user */
		return _("Requires a bootloader");
	}
	if (device_flag == FWUPD_DEVICE_FLAG_NEEDS_REBOOT) {
		/* TRANSLATORS: Requires a reboot to apply firmware or to reload hardware */
		return _("Needs a reboot after installation");
	}
	if (device_flag == FWUPD_DEVICE_FLAG_NEEDS_SHUTDOWN) {
		/* TRANSLATORS: Requires system shutdown to apply firmware */
		return _("Needs shutdown after installation");
	}
	if (device_flag == FWUPD_DEVICE_FLAG_REPORTED) {
		/* TRANSLATORS: Has been reported to a metadata server */
		return _("Reported to LVFS");
	}
	if (device_flag == FWUPD_DEVICE_FLAG_NOTIFIED) {
		/* TRANSLATORS: User has been notified */
		return _("User has been notified");
	}
	if (device_flag == FWUPD_DEVICE_FLAG_USE_RUNTIME_VERSION) {
		/* skip */
		return NULL;
	}
	if (device_flag == FWUPD_DEVICE_FLAG_INSTALL_PARENT_FIRST) {
		/* TRANSLATORS: Install composite firmware on the parent before the child */
		return _("Install to parent device first");
	}
	if (device_flag == FWUPD_DEVICE_FLAG_IS_BOOTLOADER) {
		/* TRANSLATORS: Is currently in bootloader mode */
		return _("Is in bootloader mode");
	}
	if (device_flag == FWUPD_DEVICE_FLAG_WAIT_FOR_REPLUG) {
		/* TRANSLATORS: The hardware is waiting to be replugged */
		return _("Hardware is waiting to be replugged");
	}
	if (device_flag == FWUPD_DEVICE_FLAG_IGNORE_VALIDATION) {
		/* TRANSLATORS: Ignore validation safety checks when flashing this device */
		return _("Ignore validation safety checks");
	}
	if (device_flag == FWUPD_DEVICE_FLAG_ANOTHER_WRITE_REQUIRED) {
		/* skip */
		return NULL;
	}
	if (device_flag == FWUPD_DEVICE_FLAG_NO_AUTO_INSTANCE_IDS) {
		/* skip */
		return NULL;
	}
	if (device_flag == FWUPD_DEVICE_FLAG_NEEDS_ACTIVATION) {
		/* TRANSLATORS: Device update needs to be separately activated */
		return _("Device update needs activation");
	}
	if (device_flag == FWUPD_DEVICE_FLAG_ENSURE_SEMVER) {
		/* skip */
		return NULL;
	}
	if (device_flag == FWUPD_DEVICE_FLAG_HISTORICAL) {
		/* skip */
		return NULL;
	}
	if (device_flag == FWUPD_DEVICE_FLAG_ONLY_SUPPORTED) {
		/* skip */
		return NULL;
	}
	if (device_flag == FWUPD_DEVICE_FLAG_WILL_DISAPPEAR) {
		/* TRANSLATORS: Device will not return after update completes */
		return _("Device will not re-appear after update completes");
	}
	if (device_flag == FWUPD_DEVICE_FLAG_CAN_VERIFY) {
		/* TRANSLATORS: Device supports some form of checksum verification */
		return _("Cryptographic hash verification is available");
	}
	if (device_flag == FWUPD_DEVICE_FLAG_CAN_VERIFY_IMAGE) {
		/* skip */
		return NULL;
	}
	if (device_flag == FWUPD_DEVICE_FLAG_DUAL_IMAGE) {
		/* TRANSLATORS: Device supports a safety mechanism for flashing */
		return _("Device stages updates");
	}
	if (device_flag == FWUPD_DEVICE_FLAG_SELF_RECOVERY) {
		/* TRANSLATORS: Device supports a safety mechanism for flashing */
		return _("Device can recover flash failures");
	}
	if (device_flag == FWUPD_DEVICE_FLAG_USABLE_DURING_UPDATE) {
		/* TRANSLATORS: Device remains usable during update */
		return _("Device is usable for the duration of the update");
	}
	if (device_flag == FWUPD_DEVICE_FLAG_INSTALL_ALL_RELEASES) {
		/* TRANSLATORS: Device does not jump directly to the newest */
		return _("Device installs all intermediate releases");
	}
	if (device_flag == FWUPD_DEVICE_FLAG_SKIPS_RESTART) {
		/* TRANSLATORS: Device does not restart and requires a manual action */
		return _("Device skips restart");
	}
	if (device_flag == FWUPD_DEVICE_FLAG_HAS_MULTIPLE_BRANCHES) {
		/* TRANSLATORS: the stream can be provided by different vendors */
		return _("Device supports switching to a different stream of firmware");
	}
	if (device_flag == FWUPD_DEVICE_FLAG_BACKUP_BEFORE_INSTALL) {
		/* TRANSLATORS: it is saved to the users local disk */
		return _("Device firmware will be saved before installing updates");
	}
#if FWUPD_CHECK_VERSION(1, 6, 2)
	if (device_flag == FWUPD_DEVICE_FLAG_WILDCARD_INSTALL) {
		/* TRANSLATORS: on some systems certain devices have to have matching versions,
		 * e.g. the EFI driver for a given network card cannot be different */
		return _("All devices of the same type will be updated at the same time");
	}
#endif
#if FWUPD_CHECK_VERSION(1, 6, 2)
	if (device_flag == FWUPD_DEVICE_FLAG_ONLY_VERSION_UPGRADE) {
		/* TRANSLATORS: some devices can only be updated to a new semver and cannot
		 * be downgraded or reinstalled with the sexisting version */
		return _("Only version upgrades are allowed");
	}
#endif
#if FWUPD_CHECK_VERSION(1, 7, 0)
	if (device_flag == FWUPD_DEVICE_FLAG_UNREACHABLE) {
		/* TRANSLATORS: the device is currently unreachable, perhaps because it is in a
		 * lower power state or is out of wireless range */
		return _("Device is currently unreachable");
	}
#endif
#if FWUPD_CHECK_VERSION(1, 7, 1)
	if (device_flag == FWUPD_DEVICE_FLAG_AFFECTS_FDE) {
		/* TRANSLATORS: a volume with full-disk-encryption was found on this machine,
		 * typically a Windows NTFS partition with BitLocker */
		return _("May invalidate secrets used to decrypt volumes");
	}
#endif
#if FWUPD_CHECK_VERSION(1, 7, 5)
	if (device_flag == FWUPD_DEVICE_FLAG_END_OF_LIFE) {
		/* TRANSLATORS: the vendor is no longer supporting the device */
		return _("End of life");
	}
#endif
#if FWUPD_CHECK_VERSION(1, 7, 6)
	if (device_flag == FWUPD_DEVICE_FLAG_SIGNED_PAYLOAD) {
		/* TRANSLATORS: firmware is verified on-device the payload using strong crypto */
		return _("Signed payload");
	}
#endif
#if FWUPD_CHECK_VERSION(1, 7, 6)
	if (device_flag == FWUPD_DEVICE_FLAG_UNSIGNED_PAYLOAD) {
		/* TRANSLATORS: firmware payload is unsigned and it is possible to modify it */
		return _("Unsigned payload");
	}
#endif
#if FWUPD_CHECK_VERSION(1, 8, 11)
	if (device_flag == FWUPD_DEVICE_FLAG_EMULATED) {
		/* TRANSLATORS: this device is not actually real */
		return _("Emulated");
	}
#endif
#if FWUPD_CHECK_VERSION(1, 8, 11)
	if (device_flag == FWUPD_DEVICE_FLAG_EMULATION_TAG) {
		/* TRANSLATORS: we're saving all USB events for emulation */
		return _("Tagged for emulation");
	}
#endif
	if (device_flag == FWUPD_DEVICE_FLAG_UNKNOWN) {
		return NULL;
	}
	return NULL;
}

const gchar *
gfu_common_device_icon_from_flag(FwupdDeviceFlags device_flag)
{
	if (device_flag == FWUPD_DEVICE_FLAG_INTERNAL)
		return "drive-harddisk-symbolic";
	if (device_flag == FWUPD_DEVICE_FLAG_UPDATABLE)
		return "software-update-available-symbolic";
	if (device_flag == FWUPD_DEVICE_FLAG_ONLY_OFFLINE)
		return "network-offline-symbolic";
	if (device_flag == FWUPD_DEVICE_FLAG_REQUIRE_AC)
		return "battery-symbolic";
	if (device_flag == FWUPD_DEVICE_FLAG_LOCKED)
		return "locked-symbolic";
	if (device_flag == FWUPD_DEVICE_FLAG_SUPPORTED)
		return "network-transmit-receive-symbolic";
	if (device_flag == FWUPD_DEVICE_FLAG_NEEDS_BOOTLOADER)
		return "computer-symbolic";
	if (device_flag == FWUPD_DEVICE_FLAG_NEEDS_REBOOT)
		return "system-reboot-symbolic";
	if (device_flag == FWUPD_DEVICE_FLAG_NEEDS_SHUTDOWN)
		return "system-shutdown-symbolic";
	if (device_flag == FWUPD_DEVICE_FLAG_REPORTED)
		return "task-due-symbolic";
	if (device_flag == FWUPD_DEVICE_FLAG_NOTIFIED)
		return "task-due-symbolic";
	if (device_flag == FWUPD_DEVICE_FLAG_USE_RUNTIME_VERSION)
		return "system-run-symbolic";
	if (device_flag == FWUPD_DEVICE_FLAG_INSTALL_PARENT_FIRST)
		return "system-software-install-symbolic";
	if (device_flag == FWUPD_DEVICE_FLAG_IS_BOOTLOADER)
		return "computer-symbolic";
	if (device_flag == FWUPD_DEVICE_FLAG_WAIT_FOR_REPLUG)
		return "battery-low-symbolic";
	if (device_flag == FWUPD_DEVICE_FLAG_IGNORE_VALIDATION)
		return "dialog-error-symbolic";
	if (device_flag == FWUPD_DEVICE_FLAG_ANOTHER_WRITE_REQUIRED)
		return "media-floppy-symbolic";
	if (device_flag == FWUPD_DEVICE_FLAG_NO_AUTO_INSTANCE_IDS)
		return "dialog-error-symbolic";
	if (device_flag == FWUPD_DEVICE_FLAG_NEEDS_ACTIVATION)
		return "emblem-important-symbolic";
	if (device_flag == FWUPD_DEVICE_FLAG_ENSURE_SEMVER)
		return "emblem-important-symbolic";
	if (device_flag == FWUPD_DEVICE_FLAG_WILL_DISAPPEAR)
		return "emblem-important-symbolic";
	if (device_flag == FWUPD_DEVICE_FLAG_CAN_VERIFY)
		return "emblem-important-symbolic";
	if (device_flag == FWUPD_DEVICE_FLAG_DUAL_IMAGE)
		return "emblem-important-symbolic";
	if (device_flag == FWUPD_DEVICE_FLAG_SELF_RECOVERY)
		return "emblem-important-symbolic";
	if (device_flag == FWUPD_DEVICE_FLAG_USABLE_DURING_UPDATE)
		return "emblem-important-symbolic";
	if (device_flag == FWUPD_DEVICE_FLAG_INSTALL_ALL_RELEASES)
		return "media-skip-forward-symbolic";
	if (device_flag == FWUPD_DEVICE_FLAG_SKIPS_RESTART)
		return "process-stop-symbolic";
	if (device_flag == FWUPD_DEVICE_FLAG_HAS_MULTIPLE_BRANCHES)
		return "emblem-shared-symbolic";
	if (device_flag == FWUPD_DEVICE_FLAG_BACKUP_BEFORE_INSTALL)
		return "drive-harddisk-symbolic";
#if FWUPD_CHECK_VERSION(1, 6, 2)
	if (device_flag == FWUPD_DEVICE_FLAG_WILDCARD_INSTALL)
		return "edit-copy-symbolic";
#endif
#if FWUPD_CHECK_VERSION(1, 6, 2)
	if (device_flag == FWUPD_DEVICE_FLAG_ONLY_VERSION_UPGRADE)
		return "go-up-symbolic";
#endif
#if FWUPD_CHECK_VERSION(1, 7, 0)
	if (device_flag == FWUPD_DEVICE_FLAG_UNREACHABLE)
		return "network-offline-symbolic";
#endif
#if FWUPD_CHECK_VERSION(1, 7, 1)
	if (device_flag == FWUPD_DEVICE_FLAG_AFFECTS_FDE)
		return "system-lock-screen-symbolic";
#endif
#if FWUPD_CHECK_VERSION(1, 7, 5)
	if (device_flag == FWUPD_DEVICE_FLAG_END_OF_LIFE)
		return "alarm-symbolic";
#endif
#if FWUPD_CHECK_VERSION(1, 7, 6)
	if (device_flag == FWUPD_DEVICE_FLAG_SIGNED_PAYLOAD)
		return "security-high-symbolic";
#endif
#if FWUPD_CHECK_VERSION(1, 7, 6)
	if (device_flag == FWUPD_DEVICE_FLAG_UNSIGNED_PAYLOAD)
		return "security-low-symbolic";
#endif
#if FWUPD_CHECK_VERSION(1, 8, 11)
	if (device_flag == FWUPD_DEVICE_FLAG_EMULATED)
		return "media-playlist-repeat-symbolic";
#endif
#if FWUPD_CHECK_VERSION(1, 8, 11)
	if (device_flag == FWUPD_DEVICE_FLAG_EMULATION_TAG)
		return "media-playlist-repeat-song-symbolic";
#endif
	if (device_flag == FWUPD_DEVICE_FLAG_UNKNOWN)
		return "unknown-symbolic";
	return NULL;
}

const gchar *
gfu_status_to_string(FwupdStatus status)
{
	switch (status) {
	case FWUPD_STATUS_IDLE:
		/* TRANSLATORS: daemon is inactive */
		return _("Idle…");
		break;
	case FWUPD_STATUS_DECOMPRESSING:
		/* TRANSLATORS: decompressing the firmware file */
		return _("Decompressing…");
		break;
	case FWUPD_STATUS_LOADING:
		/* TRANSLATORS: parsing the firmware information */
		return _("Loading…");
		break;
	case FWUPD_STATUS_DEVICE_RESTART:
		/* TRANSLATORS: restarting the device to pick up new F/W */
		return _("Restarting device…");
		break;
	case FWUPD_STATUS_DEVICE_READ:
		/* TRANSLATORS: reading from the flash chips */
		return _("Reading…");
		break;
	case FWUPD_STATUS_DEVICE_WRITE:
		/* TRANSLATORS: writing to the flash chips */
		return _("Writing…");
		break;
	case FWUPD_STATUS_DEVICE_ERASE:
		/* TRANSLATORS: erasing contents of the flash chips */
		return _("Erasing…");
		break;
	case FWUPD_STATUS_DEVICE_VERIFY:
		/* TRANSLATORS: verifying we wrote the firmware correctly */
		return _("Verifying…");
		break;
	case FWUPD_STATUS_SCHEDULING:
		/* TRANSLATORS: scheduing an update to be done on the next boot */
		return _("Scheduling…");
		break;
	case FWUPD_STATUS_DOWNLOADING:
		/* TRANSLATORS: downloading from a remote server */
		return _("Downloading…");
		break;
	case FWUPD_STATUS_WAITING_FOR_AUTH:
		/* TRANSLATORS: waiting for user to authenticate */
		return _("Authenticating…");
		break;
	case FWUPD_STATUS_DEVICE_BUSY:
		/* TRANSLATORS: waiting for device to do something */
		return _("Waiting…");
		break;
	case FWUPD_STATUS_SHUTDOWN:
		/* TRANSLATORS: waiting for daemon */
		return _("Shutting down…");
		break;
	default:
		break;
	}
	return fwupd_status_to_string(status);
}

gchar *
gfu_common_strjoin_array(const gchar *separator, GPtrArray *array)
{
	g_autofree const gchar **strv = NULL;

	g_return_val_if_fail(array != NULL, NULL);

	strv = g_new0(const gchar *, array->len + 1);
	for (guint i = 0; i < array->len; i++)
		strv[i] = g_ptr_array_index(array, i);
	return g_strjoinv(separator, (gchar **)strv);
}
