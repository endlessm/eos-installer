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

#ifndef __GIS_STORE_H__
#define __GIS_STORE_H__

#include "gnome-image-installer.h"

G_BEGIN_DECLS

typedef enum {
  GIS_STORE_IMAGE = 0,
  GIS_STORE_BLOCK_DEVICE,
  GIS_STORE_N_OBJECTS
} GISStoreObjectKey;

GObject *gis_store_get_object(gint key);
void gis_store_set_object(gint key, GObject *obj);
void gis_store_clear_object(gint key);

G_END_DECLS

#endif /* __GIS_STORE_H__ */

