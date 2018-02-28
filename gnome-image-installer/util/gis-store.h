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

#ifndef __GIS_STORE_H__
#define __GIS_STORE_H__

#include "gnome-image-installer.h"
#include <glib.h>

#include "gis-unattended-config.h"

G_BEGIN_DECLS

typedef enum {
  /* GFile: selected image path */
  GIS_STORE_IMAGE = 0,

  /* UDisksBlock: block device to reformat */
  GIS_STORE_BLOCK_DEVICE,

  /* UDisksClient: global shared UDisks client proxy */
  GIS_STORE_UDISKS_CLIENT,

  /* GFile: mount point of partition holding GIS_STORE_IMAGE. This is not
   * always the parent directory of GIS_STORE_IMAGE!
   */
  GIS_STORE_IMAGE_DIR,

  /* UDisksDrive: drive hosting partition mounted at GIS_STORE_IMAGE_DIR, or
   * NULL if it can't be determined
   */
  GIS_STORE_IMAGE_DRIVE,

  GIS_STORE_N_OBJECTS
} GISStoreObjectKey;

GObject *gis_store_get_object(gint key);
void gis_store_set_object(gint key, GObject *obj);
void gis_store_clear_object(gint key);

guint64 gis_store_get_required_size(void);
void gis_store_set_required_size(guint64 size);

guint64 gis_store_get_image_size (void);
void gis_store_set_image_size (guint64 size);

gchar *gis_store_get_image_name(void);
void gis_store_set_image_name(gchar *name);
void gis_store_clear_image_name(void);

const gchar *gis_store_get_image_uuid(void);
void gis_store_set_image_uuid(const gchar *uuid);

const gchar *gis_store_get_image_signature(void);
void gis_store_set_image_signature(const gchar *signature);

GError *gis_store_get_error(void);
void gis_store_set_error(GError *error);
void gis_store_clear_error(void);

void gis_store_enter_unattended (GisUnattendedConfig *config);
gboolean gis_store_is_unattended (void);
GisUnattendedConfig *gis_store_get_unattended_config (void);

void gis_store_enter_live_install(void);
gboolean gis_store_is_live_install(void);

G_END_DECLS

#endif /* __GIS_STORE_H__ */

