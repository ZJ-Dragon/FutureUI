/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

#include <adwaita.h>
#include <gtk/gtk.h>
#include <NetworkManager.h>

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE (NetVpn, net_vpn, NET, VPN, AdwActionRow)

NetVpn       *net_vpn_new                (NMClient     *client,
                                          NMConnection *connection);

NMConnection *net_vpn_get_connection     (NetVpn       *vpn);

gboolean nm_is_wireguard_connection (NMActiveConnection *c);

G_END_DECLS
