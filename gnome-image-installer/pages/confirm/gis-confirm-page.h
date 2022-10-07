/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright © 2018 Endless Mobile, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GIS_CONFIRM_PAGE_H__
#define __GIS_CONFIRM_PAGE_H__

#include "gnome-initial-setup.h"

G_BEGIN_DECLS

#define GIS_TYPE_CONFIRM_PAGE               (gis_confirm_page_get_type ())
#define GIS_CONFIRM_PAGE(obj)                           (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIS_TYPE_CONFIRM_PAGE, GisConfirmPage))
#define GIS_CONFIRM_PAGE_CLASS(klass)                   (G_TYPE_CHECK_CLASS_CAST ((klass),  GIS_TYPE_CONFIRM_PAGE, GisConfirmPageClass))
#define GIS_IS_CONFIRM_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIS_TYPE_CONFIRM_PAGE))
#define GIS_IS_CONFIRM_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GIS_TYPE_CONFIRM_PAGE))
#define GIS_CONFIRM_PAGE_GET_CLASS(obj)                 (G_TYPE_INSTANCE_GET_CLASS ((obj),  GIS_TYPE_CONFIRM_PAGE, GisConfirmPageClass))

typedef struct _GisConfirmPage        GisConfirmPage;
typedef struct _GisConfirmPageClass   GisConfirmPageClass;

struct _GisConfirmPage
{
  GisPage parent;
};

struct _GisConfirmPageClass
{
  GisPageClass parent_class;
};

GType gis_confirm_page_get_type (void);

void gis_prepare_confirm_page (GisDriver *driver);

G_END_DECLS

#endif /* __GIS_CONFIRM_PAGE_H__ */
