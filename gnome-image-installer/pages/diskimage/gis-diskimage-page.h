/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2012 Red Hat
 *               2016 Endless Mobile, Inc.
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
 *
 * Original code written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#ifndef __GIS_DISK_IMAGE_PAGE_H__
#define __GIS_DISK_IMAGE_PAGE_H__

#include "gnome-image-installer.h"

G_BEGIN_DECLS

#define GIS_TYPE_DISK_IMAGE_PAGE               (gis_diskimage_page_get_type ())
#define GIS_DISK_IMAGE_PAGE(obj)                           (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIS_TYPE_DISK_IMAGE_PAGE, GisDiskImagePage))
#define GIS_DISK_IMAGE_PAGE_CLASS(klass)                   (G_TYPE_CHECK_CLASS_CAST ((klass),  GIS_TYPE_DISK_IMAGE_PAGE, GisDiskImagePageClass))
#define GIS_IS_DISK_IMAGE_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIS_TYPE_DISK_IMAGE_PAGE))
#define GIS_IS_DISK_IMAGE_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GIS_TYPE_DISK_IMAGE_PAGE))
#define GIS_DISK_IMAGE_PAGE_GET_CLASS(obj)                 (G_TYPE_INSTANCE_GET_CLASS ((obj),  GIS_TYPE_DISK_IMAGE_PAGE, GisDiskImagePageClass))

typedef struct _GisDiskImagePage        GisDiskImagePage;
typedef struct _GisDiskImagePageClass   GisDiskImagePageClass;

struct _GisDiskImagePage
{
  GisPage parent;
};

struct _GisDiskImagePageClass
{
  GisPageClass parent_class;
};

GType gis_diskimage_page_get_type (void);

void gis_prepare_diskimage_page (GisDriver *driver);

GQuark gis_image_error_quark (void);
#define GIS_IMAGE_ERROR gis_image_error_quark()

G_END_DECLS

#endif /* __GIS_DISK_IMAGE_PAGE_H__ */
