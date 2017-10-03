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

#include "config.h"

#include "gis-store.h"

static GObject *_objects[GIS_STORE_N_OBJECTS];
static GisImage *_selected_image = NULL;
static GError *_error = NULL;
static gboolean _unattended = FALSE;
static gboolean _live_install = FALSE;
static GKeyFile *_keys = NULL;
static gchar *_uuid = NULL;

G_DEFINE_BOXED_TYPE(GisImage, gis_image, gis_image_copy, gis_image_free);

GisImage *
gis_image_new (const gchar *name,
               GFile       *file,
               GFile       *signature,
               guint64      compressed_size,
               guint64      uncompressed_size)
{
  GisImage *image;

  g_return_val_if_fail (name != NULL, NULL);

  g_return_val_if_fail (file != NULL, NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  g_return_val_if_fail (signature != NULL, NULL);
  g_return_val_if_fail (G_IS_FILE (signature), NULL);

  image = g_slice_new0 (GisImage);
  image->name = g_strdup (name);
  image->file = g_object_ref (file);
  image->signature = g_object_ref (signature);
  image->compressed_size = compressed_size;
  image->uncompressed_size = uncompressed_size;
  return image;
}

GisImage *
gis_image_copy (const GisImage *image)
{
  return gis_image_new (image->name, image->file, image->signature,
      image->compressed_size, image->uncompressed_size);
}

void
gis_image_free (GisImage *image)
{
  g_clear_pointer (&image->name, g_free);
  g_clear_object (&image->file);
  g_clear_object (&image->signature);

  g_slice_free (GisImage, image);
}

GObject *gis_store_get_object(gint key)
{
  if (key >= GIS_STORE_N_OBJECTS || key < 0)
    return NULL;

  return _objects[key];
}

void gis_store_set_object(gint key, GObject *obj)
{
  if (key >= GIS_STORE_N_OBJECTS || key < 0)
    return;

  _objects[key] = g_object_ref(obj);  
}

void gis_store_clear_object(gint key)
{
  if (key >= GIS_STORE_N_OBJECTS || key < 0)
    return;

  g_object_unref(_objects[key]);
  _objects[key] = NULL;
}

GisImage *
gis_store_get_selected_image (void)
{
  return _selected_image;
}

void
gis_store_set_selected_image (const GisImage *image)
{
  g_clear_pointer (&_selected_image, gis_image_free);

  if (image != NULL)
    _selected_image = gis_image_copy (image);
}

const gchar *gis_store_get_image_uuid (void)
{
  return _uuid;
}

void gis_store_set_image_uuid (const gchar *uuid)
{
  g_free (_uuid);
  _uuid = g_strdup (uuid);
}

GError *gis_store_get_error(void)
{
  return _error;
}

void gis_store_set_error(GError *error)
{
  g_clear_error (&_error);
  _error = g_error_copy (error);
}

void gis_store_clear_error(void)
{
  g_clear_error (&_error);
}

void gis_store_enter_unattended(void)
{
  _unattended = TRUE;
}

gboolean gis_store_is_unattended(void)
{
  return _unattended;
}

void gis_store_enter_live_install(void)
{
  _live_install = TRUE;
}

gboolean gis_store_is_live_install(void)
{
  return _live_install;
}

void gis_store_set_key_file(GKeyFile *keys)
{
  _keys = keys;
}

GKeyFile *gis_store_get_key_file(void)
{
  return _keys;
}


/* Epilogue {{{1 */
/* vim: set foldmethod=marker: */
