/*
 * Copyright © 2018 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *       Christian J. Kellner <christian@kellner.me>
 */

#include "config.h"

#include "bolt-time.h"

char *
bolt_epoch_format (guint64 seconds, const char *format)
{
  g_autoptr(GDateTime) dt = NULL;

  dt = g_date_time_new_from_unix_utc ((gint64) seconds);

  if (dt == NULL)
    return NULL;

  return g_date_time_format (dt, format);
}

guint64
bolt_now_in_seconds (void)
{
  gint64 now = g_get_real_time ();

  return (guint64) now / G_USEC_PER_SEC;
}
