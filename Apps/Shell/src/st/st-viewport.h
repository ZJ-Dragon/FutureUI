/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-viewport.h: viewport actor
 *
 * Copyright 2009 Intel Corporation.
 * Copyright 2009, 2010 Red Hat, Inc.
 * Copyright 2019 Endless, Inc.
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

#pragma once

#include <st/st-widget.h>

G_BEGIN_DECLS

#define ST_TYPE_VIEWPORT (st_viewport_get_type())
G_DECLARE_DERIVABLE_TYPE (StViewport, st_viewport, ST, VIEWPORT, StWidget)

struct _StViewportClass
{
  StWidgetClass parent_class;
};

G_END_DECLS
