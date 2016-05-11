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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Original code written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#ifndef __GIS_DISK_TARGET_PAGE_H__
#define __GIS_DISK_TARGET_PAGE_H__

#include "gnome-image-installer.h"

G_BEGIN_DECLS

#define GIS_TYPE_DISK_TARGET_PAGE               (gis_disktarget_page_get_type ())
#define GIS_DISK_TARGET_PAGE(obj)                           (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIS_TYPE_DISK_TARGET_PAGE, GisDiskTargetPage))
#define GIS_DISK_TARGET_PAGE_CLASS(klass)                   (G_TYPE_CHECK_CLASS_CAST ((klass),  GIS_TYPE_DISK_TARGET_PAGE, GisDiskTargetPageClass))
#define GIS_IS_DISK_TARGET_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIS_TYPE_DISK_TARGET_PAGE))
#define GIS_IS_DISK_TARGET_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GIS_TYPE_DISK_TARGET_PAGE))
#define GIS_DISK_TARGET_PAGE_GET_CLASS(obj)                 (G_TYPE_INSTANCE_GET_CLASS ((obj),  GIS_TYPE_DISK_TARGET_PAGE, GisDiskTargetPageClass))

typedef struct _GisDiskTargetPage        GisDiskTargetPage;
typedef struct _GisDiskTargetPageClass   GisDiskTargetPageClass;

struct _GisDiskTargetPage
{
  GisPage parent;
};

struct _GisDiskTargetPageClass
{
  GisPageClass parent_class;
};

GType gis_disktarget_page_get_type (void);

void gis_prepare_disktarget_page (GisDriver *driver);

G_END_DECLS

#endif /* __GIS_DISK_TARGET_PAGE_H__ */
