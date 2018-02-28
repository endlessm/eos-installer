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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "gis-dmi.h"
#include "config.h"

#include <gio/gio.h>

#define SYS_VENDOR "/sys/class/dmi/id/sys_vendor"
#define SYS_PRODUCT "/sys/class/dmi/id/product_name"

/**
 * gis_dmi_sanitize_string:
 * @string: (transfer none): a NUL-terminated array of arbitrary bytes (which
 *  are not necessarily valid UTF-8)
 *
 * Returns: (nullable) (transfer full): an ASCII string containing all
 *  printable characters from @string, with leading and trailing whitespace
 *  stripped; %NULL if @string is %NULL or contains no non-whitespace printable
 *  ASCII characters
 */
gchar *
gis_dmi_sanitize_string (const gchar *string)
{
  g_autofree gchar *sanitized = g_strdup (string);
  const gchar *r = string;
  gchar *w = sanitized;

  if (string == NULL)
    return NULL;

  for (; *r != '\0'; r++)
    {
      if (*r < 32 || *r > 126)
        continue;
      *w = *r;
      w++;
    }
  *w = '\0';

  g_strstrip (sanitized);
  if (*sanitized == '\0')
    return NULL;

  return g_steal_pointer (&sanitized);
}

static gchar *
gis_dmi_read_sanitized_string (const gchar *filename,
                               GError     **error)
{
  g_autofree gchar *contents = NULL;

  if (g_file_get_contents (filename, &contents, NULL, error))
    {
      gchar *sanitized = gis_dmi_sanitize_string (contents);
      if (sanitized == NULL)
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                     "%s contained no printable characters",
                     filename);
      return sanitized;
    }
  else
    {
      g_prefix_error (error, "Failed to read %s", filename);
      return NULL;
    }
}

/**
 * gis_dmi_read_vendor_product:
 * @vendor: (out) (transfer full) (not optional): location to store the
 *  computer's vendor
 * @product: (out) (transfer full) (not optional): location to store the
 *  computer's product name
 *
 * Reads sanitized vendor and product information from DMI tables. On success,
 * the returned strings are guaranteed to be non-empty and contain only
 * printable ASCII characters, with no leading or trailing whitespace. On
 * error, both @vendor and @product will be set to %NULL.
 *
 * Returns: %TRUE if both vendor and product were read successfully.
 */
gboolean
gis_dmi_read_vendor_product (gchar  **vendor,
                             gchar  **product,
                             GError **error)
{
  g_autofree gchar *vendor_tmp = NULL;

  if (NULL == (vendor_tmp = gis_dmi_read_sanitized_string (SYS_VENDOR, error))
      ||
      NULL == (*product = gis_dmi_read_sanitized_string (SYS_PRODUCT, error)))
    {
      *vendor = NULL;
      return FALSE;
    }

  *vendor = g_steal_pointer (&vendor_tmp);
  return TRUE;
}

