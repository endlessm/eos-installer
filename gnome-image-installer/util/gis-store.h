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
#include <glib.h>

G_BEGIN_DECLS

typedef enum {
  GIS_STORE_IMAGE = 0,
  GIS_STORE_BLOCK_DEVICE,
  GIS_STORE_SECONDARY_IMAGE,
  GIS_STORE_SECONDARY_BLOCK_DEVICE,
  GIS_STORE_N_OBJECTS
} GisStoreObjectKey;

typedef enum {
  GIS_STORE_TARGET_EMPTY = 0,
  GIS_STORE_TARGET_PRIMARY,
  GIS_STORE_TARGET_SECONDARY,
  GIS_STORE_N_TARGETS
} GisStoreTargetName;

typedef struct _GisStoreTarget {
  GisStoreTargetName target;
  gchar *image;
  gchar *signature;
  gchar *device;
  gint64 write_size;
} GisStoreTarget;

GObject *gis_store_get_object(gint key);
void gis_store_set_object(gint key, GObject *obj);
void gis_store_clear_object(gint key);

gint64 gis_store_get_required_size();
void gis_store_set_required_size(gint64 size);

gint64 gis_store_get_image_size (void);
void gis_store_set_image_size (gint64 size);

const gchar *gis_store_get_image_name();
void gis_store_set_image_name(gchar *name);
void gis_store_clear_image_name();

const gchar *gis_store_get_image_drive();
void gis_store_set_image_drive(const gchar *drive);

const gchar *gis_store_get_image_signature(void);
void gis_store_set_image_signature(const gchar *signature);

const GisStoreTarget *gis_store_get_target(GisStoreTargetName target);
void gis_store_set_target(GisStoreTargetName target, const gchar *image, const gchar *signature, const gchar *device);
void gis_store_set_target_write_size(GisStoreTargetName target, gint64 write_size);

GError *gis_store_get_error();
void gis_store_set_error(GError *error);
void gis_store_clear_error();

void gis_store_enter_unattended(const gchar *vendor, const gchar *product);
gboolean gis_store_is_unattended();

const gchar *gis_store_get_vendor (void);
const gchar *gis_store_get_product (void);

void gis_store_enter_live_install();
gboolean gis_store_is_live_install();

void gis_store_set_key_file(GKeyFile *keys);
GKeyFile *gis_store_get_key_file();

G_END_DECLS

#endif /* __GIS_STORE_H__ */

