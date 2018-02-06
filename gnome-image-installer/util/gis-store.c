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
static guint64 _size = 0;
static guint64 _image_size = 0;
static gchar *_name = NULL;
static gchar *_signature = NULL;
static GError *_error = NULL;
static GisUnattendedConfig *_config = NULL;
static gboolean _live_install = FALSE;
static gchar *_uuid = NULL;

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

guint64 gis_store_get_required_size(void)
{
  return _size;
}

void gis_store_set_required_size(guint64 size)
{
  _size = size;
}

guint64 gis_store_get_image_size (void)
{
  return _image_size;
}

void gis_store_set_image_size (guint64 size)
{
  _image_size = size;
}

gchar *gis_store_get_image_name(void)
{
  return _name;
}

void gis_store_set_image_name(gchar *name)
{
  g_free (_name);
  _name = g_strdup (name);
}

void gis_store_clear_image_name(void)
{
  g_free (_name);
  _name = NULL;
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

/**
 * gis_store_enter_unattended:
 * @config: (transfer none): unattended mode configuration
 *
 * Enter unattended mode.
 */
void
gis_store_enter_unattended (GisUnattendedConfig *config)
{
  g_return_if_fail (config != NULL);
  g_return_if_fail (_config == NULL);

  _config = g_object_ref (config);
}

gboolean
gis_store_is_unattended (void)
{
  return _config != NULL;
}

/**
 * gis_store_get_unattended_config:
 *
 * Returns: (nullable) (transfer none): the unattended config, or %NULL if we
 *  are not in unattended mode.
 */
GisUnattendedConfig *
gis_store_get_unattended_config (void)
{
  return _config;
}

void gis_store_enter_live_install(void)
{
  _live_install = TRUE;
}

gboolean gis_store_is_live_install(void)
{
  return _live_install;
}

/* Epilogue {{{1 */
/* vim: set foldmethod=marker: */
