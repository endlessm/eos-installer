/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2014 Endless Mobile, Inc
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by:
 *     Emmanuele Bassi <emmanuele@endlessm.com>
 */

#ifndef __GIS_DISPLAY_PAGE_H__
#define __GIS_DISPLAY_PAGE_H__

#include "gnome-initial-setup.h"

G_BEGIN_DECLS

#define GIS_TYPE_DISPLAY_PAGE                   (gis_display_page_get_type ())
#define GIS_DISPLAY_PAGE(obj)                   (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIS_TYPE_DISPLAY_PAGE, GisDisplayPage))
#define GIS_DISPLAY_PAGE_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass),  GIS_TYPE_DISPLAY_PAGE, GisDisplayPageClass))
#define GIS_IS_DISPLAY_PAGE(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIS_TYPE_DISPLAY_PAGE))
#define GIS_IS_DISPLAY_PAGE_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass),  GIS_TYPE_DISPLAY_PAGE))
#define GIS_DISPLAY_PAGE_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj),  GIS_TYPE_DISPLAY_PAGE, GisDisplayPageClass))

typedef struct _GisDisplayPage        GisDisplayPage;
typedef struct _GisDisplayPageClass   GisDisplayPageClass;

struct _GisDisplayPage
{
  GisPage parent;
};

struct _GisDisplayPageClass
{
  GisPageClass parent_class;
};

GType gis_display_page_get_type (void);

void gis_prepare_display_page (GisDriver *driver);

G_END_DECLS

#endif /* __GIS_DISPLAY_PAGE_H__ */
