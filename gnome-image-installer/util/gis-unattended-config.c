/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright © 2018 Endless Mobile, Inc.
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
 */
#include "config.h"
#include "gis-unattended-config.h"

#include <glib/gi18n.h>

#include "glnx-errors.h"

typedef struct _GisUnattendedConfig {
  GObject parent;

  gchar *file_path;
  gboolean initialized;

  GKeyFile *key_file;
} GisUnattendedConfig;

G_DEFINE_QUARK (gis-unattended-error, gis_unattended_error);

static void gis_unattended_config_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_EXTENDED (GisUnattendedConfig,
                        gis_unattended_config,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                               gis_unattended_config_initable_iface_init));

typedef enum {
  PROP_FILE_PATH = 1,
  N_PROPERTIES
} GisUnattendedConfigPropertyId;

static GParamSpec *props[N_PROPERTIES] = { 0 };

static void
gis_unattended_config_init (GisUnattendedConfig *self)
{
  self->key_file = g_key_file_new ();
}

static void
gis_unattended_config_set_property (GObject      *object,
                                    guint         property_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GisUnattendedConfig *self = GIS_UNATTENDED_CONFIG (object);

  switch ((GisUnattendedConfigPropertyId) property_id)
    {
    case PROP_FILE_PATH:
      g_free (self->file_path);
      self->file_path = g_value_dup_string (value);
      break;

    case N_PROPERTIES:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gis_unattended_config_dispose (GObject *object)
{
  GisUnattendedConfig *self = GIS_UNATTENDED_CONFIG (object);

  g_clear_pointer (&self->key_file, g_key_file_unref);

  G_OBJECT_CLASS (gis_unattended_config_parent_class)->dispose (object);
}

static void
gis_unattended_config_finalize (GObject *object)
{
  GisUnattendedConfig *self = GIS_UNATTENDED_CONFIG (object);

  g_clear_pointer (&self->file_path, g_free);

  G_OBJECT_CLASS (gis_unattended_config_parent_class)->finalize (object);
}

static void
gis_unattended_config_class_init (GisUnattendedConfigClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = gis_unattended_config_set_property;
  object_class->dispose = gis_unattended_config_dispose;
  object_class->finalize = gis_unattended_config_finalize;

  props[PROP_FILE_PATH] = g_param_spec_string (
      "file-path",
      "file path",
      "Path to unattended.ini file",
      NULL,
      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPERTIES, props);
}

static gboolean
gis_unattended_config_initable_init (GInitable    *initable,
                                     GCancellable *cancellable,
                                     GError      **error)
{
  GisUnattendedConfig *self = GIS_UNATTENDED_CONFIG (initable);
  g_autoptr(GError) local_error = NULL;

  g_return_val_if_fail (self->file_path != NULL, FALSE);

  /* This class does not support being initialized multiple times. */
  g_return_val_if_fail (!self->initialized, FALSE);
  self->initialized = TRUE;

  if (!g_key_file_load_from_file (self->key_file,
                                  self->file_path,
                                  G_KEY_FILE_NONE,
                                  &local_error))
    {
      if (!g_error_matches (local_error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
        {
          /* Force all other errors into the GIS_UNATTENDED_ERROR domain,
           * but leave ENOENT untouched so it can be easily caught.
           */
          local_error->domain = GIS_UNATTENDED_ERROR;
          local_error->code = GIS_UNATTENDED_ERROR_READ;
        }

      g_propagate_error (error, g_steal_pointer (&local_error));
      return FALSE;
    }

  return TRUE;
}

static void
gis_unattended_config_initable_iface_init (GInitableIface *iface)
{
  iface->init = gis_unattended_config_initable_init;
}

GisUnattendedConfig *
gis_unattended_config_new (const gchar *file_path,
                           GError **error)
{
  g_return_val_if_fail (file_path != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_initable_new (GIS_TYPE_UNATTENDED_CONFIG, NULL, error,
                         "file-path", file_path,
                         NULL);
}

/**
 * gis_unattended_config_get_key_file:
 *
 * This is a transitional function which we should remove in favour of
 * structured accessors.
 *
 * Returns: (transfer none): the underlying key file.
 */
GKeyFile *
gis_unattended_config_get_key_file (GisUnattendedConfig *self)
{
  return self->key_file;
}
