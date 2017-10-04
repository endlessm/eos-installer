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

typedef struct _GisImage {
  /* Human-readable name */
  gchar *name;

  /* Image file to write to disk. This may be a device (eg
   * /dev/mapper/endless-image) or a regular file (eg /path/to/eos-...img.gz)
   */
  GFile *file;

  /* Image file to verify. This may be a device (eg
   * /dev/mapper/endless-image-squashfs) or a regular file (eg
   * /path/to/eos-...img.gz, /path/to/endless.squash).
   *
   * When installing a SquashFS-compressed image file, we rely on the
   * uncompressed file within being mapped to a loopback device (and then to
   * /dev/mapper/endless-image). But we still want to *verify* the compressed
   * SquashFS image.
   *
   * When not installing from SquashFS, this will be the same as 'file'.
   */
  GFile *verify_file;

  /* GPG signature for 'file' */
  GFile *signature;

  /* Size of 'verify_file' */
  guint64 compressed_size;

  /* Size of image when uncompressed and written to disk. */
  guint64 uncompressed_size;
} GisImage;

GType gis_image_get_type (void);
GisImage *gis_image_new (const gchar *name,
                         GFile       *file,
                         GFile       *verify_file,
                         GFile       *signature,
                         guint64      compressed_size,
                         guint64      uncompressed_size);
GisImage *gis_image_copy (const GisImage *image);
void gis_image_free (GisImage *image);
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GisImage, gis_image_free);

typedef enum {
  /* UDisksBlock: block device to reformat */
  GIS_STORE_BLOCK_DEVICE = 0,

  /* UDisksClient: global shared UDisks client proxy */
  GIS_STORE_UDISKS_CLIENT,

  /* UDisksDrive: drive hosting partition hosting the selected image */
  GIS_STORE_IMAGE_DRIVE,

  GIS_STORE_N_OBJECTS
} GISStoreObjectKey;

GObject *gis_store_get_object(gint key);
void gis_store_set_object(gint key, GObject *obj);
void gis_store_clear_object(gint key);

void gis_store_set_selected_image (const GisImage *image);
GisImage *gis_store_get_selected_image (void);

const gchar *gis_store_get_image_uuid(void);
void gis_store_set_image_uuid(const gchar *uuid);

GError *gis_store_get_error(void);
void gis_store_set_error(GError *error);
void gis_store_clear_error(void);

void gis_store_enter_unattended(void);
gboolean gis_store_is_unattended(void);

void gis_store_enter_live_install(void);
gboolean gis_store_is_live_install(void);

void gis_store_set_key_file(GKeyFile *keys);
GKeyFile *gis_store_get_key_file(void);

G_END_DECLS

#endif /* __GIS_STORE_H__ */

