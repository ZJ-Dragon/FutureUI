/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-password-entry.h: Password entry actor based on st-entry
 *
 * Copyright 2019 Endless Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(ST_H_INSIDE) && !defined(ST_COMPILATION)
#error "Only <st/st.h> can be included directly.h"
#endif

#ifndef __ST_PASSWORD_ENTRY_H__
#define __ST_PASSWORD_ENTRY_H__

G_BEGIN_DECLS

#include <st/st-entry.h>

#define ST_TYPE_PASSWORD_ENTRY (st_password_entry_get_type ())

G_DECLARE_FINAL_TYPE (StPasswordEntry, st_password_entry, ST, PASSWORD_ENTRY, StEntry)

StEntry            *st_password_entry_new                        (void);
gboolean            st_password_entry_get_password_visible       (StPasswordEntry *entry);
void                st_password_entry_set_password_visible       (StPasswordEntry *entry,
                                                                  gboolean         value);
gboolean            st_password_entry_get_show_peek_icon         (StPasswordEntry *entry);
void                st_password_entry_set_show_peek_icon         (StPasswordEntry *entry,
                                                                  gboolean         value);

G_END_DECLS

#endif /* __ST_PASSWORD_ENTRY_H__ */

