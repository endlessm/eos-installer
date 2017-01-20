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
static gint64 _size = 0;
static gint64 _image_size = 0;
static gchar *_name = NULL;
static gchar *_drive = NULL;
static gchar *_signature = NULL;
static gchar *_target_drive = NULL;
static GError *_error = NULL;
static gboolean _unattended = FALSE;
static gchar *_vendor = NULL;
static gchar *_product = NULL;
static gboolean _live_install = FALSE;
static GKeyFile *_keys = NULL;
static GisStoreTarget _targets[GIS_STORE_N_TARGETS] = { { 0, }, };

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

gint64 gis_store_get_required_size()
{
  return _size;
}

void gis_store_set_required_size(gint64 size)
{
  _size = size;
}

gint64 gis_store_get_image_size (void)
{
  return _image_size;
}

void gis_store_set_image_size (gint64 size)
{
  _image_size = size;
}

const gchar *gis_store_get_image_name()
{
  return _name;
}

void gis_store_set_image_name(gchar *name)
{
  g_free (_name);
  _name = g_strdup (name);
}

void gis_store_clear_image_name()
{
  g_free (_name);
  _name = NULL;
}

const gchar *gis_store_get_image_drive()
{
  return _drive;
}

void gis_store_set_image_drive(const gchar *drive)
{
  g_free (_drive);
  _drive = g_strdup (drive);
}

const gchar *gis_store_get_image_signature (void)
{
  return _signature;
}

void gis_store_set_image_signature (const gchar *signature)
{
  g_free (_signature);
  _signature = g_strdup (signature);
}

const GisStoreTarget *gis_store_get_target(GisStoreTargetName target)
{
    g_assert (GIS_STORE_N_TARGETS > target);
    return (const GisStoreTarget *) &_targets[target];
}

void gis_store_set_target(GisStoreTargetName target, const gchar *image, const gchar *signature, const gchar *device)
{
    g_assert (GIS_STORE_N_TARGETS > target);
    g_free (_targets[target].image);
    g_free (_targets[target].signature);
    g_free (_targets[target].device);
    _targets[target].target = target;
    _targets[target].image = g_strdup (image);
    _targets[target].signature = g_strdup (signature);
    _targets[target].device = g_strdup (device);
    _targets[target].write_size = 0;
}

void gis_store_set_target_write_size(GisStoreTargetName target, gint64 write_size)
{
    g_assert (GIS_STORE_N_TARGETS > target);
    _targets[target].write_size = write_size;
}

GError *gis_store_get_error()
{
  return _error;
}

void gis_store_set_error(GError *error)
{
  g_clear_error (&_error);
  _error = g_error_copy (error);
}

void gis_store_clear_error()
{
  g_clear_error (&_error);
}

void gis_store_enter_unattended(const gchar *vendor, const gchar *product)
{
  _unattended = TRUE;
  _vendor = g_strdup (vendor);
  _product = g_strdup (product);
}

gboolean gis_store_is_unattended()
{
  return _unattended;
}

const gchar *gis_store_get_vendor (void)
{
  return _vendor;
}

const gchar *gis_store_get_product (void)
{
  return _product;
}

void gis_store_enter_live_install()
{
  _live_install = TRUE;
}

gboolean gis_store_is_live_install()
{
  return _live_install;
}

void gis_store_set_key_file(GKeyFile *keys)
{
  _keys = keys;
}

GKeyFile *gis_store_get_key_file()
{
  return _keys;
}


/* Epilogue {{{1 */
/* vim: set foldmethod=marker: */
