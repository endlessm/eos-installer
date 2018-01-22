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

#define EOS_GROUP "EndlessOS"
#define LOCALE_KEY "locale"

#define COMPUTER_GROUP_PREFIX "Computer"
#define VENDOR_KEY "vendor"
#define PRODUCT_KEY "product"

#define IMAGE_GROUP_PREFIX "Image"
#define FILENAME_KEY "filename"
#define BLOCK_DEVICE_KEY "block-device"

typedef struct _GisUnattendedConfig {
  GObject parent;

  gchar *file_path;
  gboolean initialized;

  GKeyFile *key_file;

  gchar *locale;
  /* Equal-length arrays of (owned) char *, where the pair at a given index
   * corresponds to a computer listed in the unattended config file.
   */
  GPtrArray *vendors;
  GPtrArray *products;

  /** Basename of image file */
  gchar *filename;
  gchar *block_device;
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
  self->vendors = g_ptr_array_new_with_free_func (g_free);
  self->products = g_ptr_array_new_with_free_func (g_free);
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
  g_clear_pointer (&self->locale, g_free);
  g_clear_pointer (&self->vendors, g_ptr_array_unref);
  g_clear_pointer (&self->products, g_ptr_array_unref);
  g_clear_pointer (&self->filename, g_free);
  g_clear_pointer (&self->block_device, g_free);

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
key_file_get_optional_string (GKeyFile    *key_file,
                              const gchar *group_name,
                              const gchar *key,
                              gchar      **value_out,
                              GError     **error)
{
  g_autoptr(GError) local_error = NULL;
  g_autofree gchar *value = NULL;

  value = g_key_file_get_string (key_file, group_name, key, &local_error);
  if (value == NULL &&
      !g_error_matches (local_error, G_KEY_FILE_ERROR,
                        G_KEY_FILE_ERROR_GROUP_NOT_FOUND) &&
      !g_error_matches (local_error, G_KEY_FILE_ERROR,
                        G_KEY_FILE_ERROR_KEY_NOT_FOUND))
    {
      /* Other possible errors include invalid UTF-8, which is reported here
       * rather than by g_key_file_load_from_file().
       */
      g_set_error_literal (error, GIS_UNATTENDED_ERROR,
                           GIS_UNATTENDED_ERROR_READ,
                           local_error->message);
      return FALSE;
    }

  *value_out = g_steal_pointer (&value);
  return TRUE;
}

static gboolean
key_file_get_optional_nonempty_string (GKeyFile    *key_file,
                                       const gchar *group_name,
                                       const gchar *key,
                                       gchar      **value_out,
                                       GError     **error)
{
  g_return_val_if_fail (value_out != NULL && *value_out == NULL, FALSE);

  if (!key_file_get_optional_string (key_file, group_name, key, value_out,
                                     error))
    return FALSE;

  if (*value_out != NULL && **value_out == '\0')
    {
      g_set_error (error, GIS_UNATTENDED_ERROR,
                   GIS_UNATTENDED_ERROR_INVALID_IMAGE,
                   /* Translators: this error refers to a configuration
                    * file. The placeholder is the name of a field in
                    * the file.
                    */
                   _("%s key is empty"),
                   key);
      return FALSE;
    }

  return TRUE;
}

static gboolean
gis_unattended_config_populate_fields (GisUnattendedConfig *self,
                                       GError **error)
{
  g_auto(GStrv) groups = g_key_file_get_groups (self->key_file, NULL);
  gchar **group;
  const gchar *seen_image_group = NULL;

  if (!key_file_get_optional_string (self->key_file,
                                     EOS_GROUP, LOCALE_KEY,
                                     &self->locale,
                                     error))
    return FALSE;

  for (group = groups; *group != NULL; group++)
    {
      if (g_str_has_prefix (*group, COMPUTER_GROUP_PREFIX))
        {
          g_autoptr(GError) local_error = NULL;
          g_autofree gchar *vendor = NULL;
          g_autofree gchar *product = NULL;

          /* both fields are mandatory, so fail on any error */
          if (NULL == (vendor =
                       g_key_file_get_string (self->key_file, *group,
                                              VENDOR_KEY, &local_error)) ||
              NULL == (product =
                       g_key_file_get_string (self->key_file, *group,
                                              PRODUCT_KEY, &local_error)))
            {
              g_set_error_literal (error, GIS_UNATTENDED_ERROR,
                                   GIS_UNATTENDED_ERROR_INVALID_COMPUTER,
                                   local_error->message);
              return FALSE;
            }

          g_ptr_array_add (self->vendors, g_steal_pointer (&vendor));
          g_ptr_array_add (self->products, g_steal_pointer (&product));
        }
      else if (g_str_has_prefix (*group, IMAGE_GROUP_PREFIX))
        {
          if (seen_image_group != NULL)
            {
              g_set_error (error, GIS_UNATTENDED_ERROR,
                           GIS_UNATTENDED_ERROR_INVALID_IMAGE,
                           /* Translators: this error refers to a configuration
                            * file. The placeholders are all the name of
                            * sections in an .ini-style file.
                            */
                           _("More than one %s section (%s and %s)"),
                           "Image", seen_image_group, *group);
              return FALSE;
            }

          seen_image_group = *group;
          if (!key_file_get_optional_nonempty_string (self->key_file,
                                                      *group, FILENAME_KEY,
                                                      &self->filename,
                                                      error) ||
              !key_file_get_optional_nonempty_string (self->key_file,
                                                      *group, BLOCK_DEVICE_KEY,
                                                      &self->block_device,
                                                      error))
            return FALSE;
        }
    }

  return TRUE;
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

  return gis_unattended_config_populate_fields (self, error);
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
 * gis_unattended_config_get_locale:
 *
 * Returns: (nullable) (transfer none): the locale specified in @self, or %NULL
 *  if none was specified.
 */
const gchar *
gis_unattended_config_get_locale (GisUnattendedConfig *self)
{
  return self->locale;
}

/**
 * gis_unattended_config_get_image:
 *
 * Returns: the basename of the configured image, or %NULL if no specific image
 *  is configured.
 */
const gchar *
gis_unattended_config_get_image (GisUnattendedConfig *self)
{
  return self->filename;
}

/**
 * gis_unattended_config_matches_device:
 * @device: full path to a block device
 *
 * Returns: %TRUE if @device matches the configured target device, or if none
 *  is configured.
 */
gboolean
gis_unattended_config_matches_device (GisUnattendedConfig *self,
                                      const gchar *device)
{
  g_autofree gchar *basename = NULL;

  if (self->block_device == NULL)
    return TRUE;

  if (self->block_device[0] == '/')
    return g_strcmp0 (device, self->block_device) == 0;

  basename = g_path_get_basename (device);
  return g_str_has_prefix (basename, self->block_device);
}

/**
 * gis_unattended_config_match_computer:
 * @vendor: (nullable): the current computer's vendor, or %NULL if it could not
 *  be determined
 * @product: (nullable): the current computer's model, or %NULL if it could not
 *  be determined
 *
 * Compares @vendor and @product to the computer(s) listed in the unattended
 * configuration, ASCII case-insensitively. In the special case where @vendor
 * or @product is %NULL, this function will never return
 * %GIS_UNATTENDED_COMPUTER_MATCHES.
 *
 * Returns: whether the current computer matches the computers listed in @self
 */
GisUnattendedComputerMatch
gis_unattended_config_match_computer (GisUnattendedConfig *self,
                                      const gchar *vendor,
                                      const gchar *product)
{
  guint n_computers = self->vendors->len;
  guint i;

  g_assert_cmpuint (self->vendors->len, ==, self->products->len);

  if (n_computers == 0)
    return GIS_UNATTENDED_COMPUTER_NOT_SPECIFIED;

  if (vendor == NULL || product == NULL)
    return GIS_UNATTENDED_COMPUTER_DOES_NOT_MATCH;

  for (i = 0; i < n_computers; i++)
    {
      if (g_strcmp0 (g_ptr_array_index (self->vendors, i), vendor) == 0 &&
          g_strcmp0 (g_ptr_array_index (self->products, i), product) == 0)
        return GIS_UNATTENDED_COMPUTER_MATCHES;
    }

  return GIS_UNATTENDED_COMPUTER_DOES_NOT_MATCH;
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
