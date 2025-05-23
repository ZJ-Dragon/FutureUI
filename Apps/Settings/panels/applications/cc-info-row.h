/* cc-info-row.h
 *
 * Copyright 2018 Matthias Clasen <matthias.clasen@gmail.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define CC_TYPE_INFO_ROW (cc_info_row_get_type())
G_DECLARE_FINAL_TYPE (CcInfoRow, cc_info_row, CC, INFO_ROW, AdwActionRow)

CcInfoRow* cc_info_row_new          (void);

void       cc_info_row_set_expanded (CcInfoRow *row,
                                     gboolean expanded);

gboolean   cc_info_row_get_expanded (CcInfoRow *row);

G_END_DECLS
