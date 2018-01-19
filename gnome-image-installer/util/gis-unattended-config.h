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

#ifndef GIS_UNATTENDED_CONFIG_H
#define GIS_UNATTENDED_CONFIG_H

#include <gio/gio.h>

G_BEGIN_DECLS

GQuark gis_unattended_error_quark (void);
#define GIS_UNATTENDED_ERROR (gis_unattended_error_quark ())

/**
 * GisUnattendedError:
 * @GIS_UNATTENDED_ERROR_READ: the unattended.ini file could not be read
 * @GIS_UNATTENDED_ERROR_INVALID_COMPUTER: a [Computer...] definition in
 *  unattended.ini had missing or incorrect fields
 * @GIS_UNATTENDED_ERROR_INVALID_IMAGE: the [Image...] definition in
 *  unattended.ini had missing or incorrect fields, or there was more than one
 *  of them
 * @GIS_UNATTENDED_ERROR_IMAGE_AMBIGUOUS: no [Image...] definition was
 *  provided, and more than one image was found
 * @GIS_UNATTENDED_ERROR_DEVICE_NOT_FOUND: no suitable block devices were
 *  found (or none matching the [Image...] definition, if provided)
 * @GIS_UNATTENDED_ERROR_DEVICE_AMBIGUOUS: more than one suitable block device
 *  was found
 */
typedef enum {
    GIS_UNATTENDED_ERROR_READ,
    GIS_UNATTENDED_ERROR_INVALID_COMPUTER,
    GIS_UNATTENDED_ERROR_INVALID_IMAGE,
    GIS_UNATTENDED_ERROR_IMAGE_NOT_FOUND,
    GIS_UNATTENDED_ERROR_IMAGE_AMBIGUOUS,
    GIS_UNATTENDED_ERROR_DEVICE_NOT_FOUND,
    GIS_UNATTENDED_ERROR_DEVICE_AMBIGUOUS,
} GisUnattendedError;

#define GIS_TYPE_UNATTENDED_CONFIG (gis_unattended_config_get_type ())
G_DECLARE_FINAL_TYPE (GisUnattendedConfig, gis_unattended_config, GIS, UNATTENDED_CONFIG, GObject);

/**
 * GisUnattendedComputerMatch:
 * @GIS_UNATTENDED_COMPUTER_NOT_SPECIFIED: the unattended configuration doesn't
 *  specify any target computers. Confirmation is required before reformatting.
 * @GIS_UNATTENDED_COMPUTER_MATCHES: the unattended configuration specifies
 *  some target computers, and the given vendor/product matches. No explicit
 *  confirmation is required before reformatting.
 * @GIS_UNATTENDED_COMPUTER_DOES_NOT_MATCH: the unattended configuration
 *  specifies some target computers, but the given vendor/product doesn't match
 *  any of them. Explicit confirmation, with a warning, is required before
 *  reformatting.
 */
typedef enum {
    GIS_UNATTENDED_COMPUTER_NOT_SPECIFIED,
    GIS_UNATTENDED_COMPUTER_MATCHES,
    GIS_UNATTENDED_COMPUTER_DOES_NOT_MATCH
} GisUnattendedComputerMatch;

GisUnattendedConfig *gis_unattended_config_new (const gchar *file_path,
                                                GError **error);

const gchar *gis_unattended_config_get_locale (GisUnattendedConfig *self);

GisUnattendedComputerMatch gis_unattended_config_match_computer (GisUnattendedConfig *self,
                                                                 const gchar *vendor,
                                                                 const gchar *product);

GKeyFile *gis_unattended_config_get_key_file (GisUnattendedConfig *self);

G_END_DECLS

#endif /* GIS_UNATTENDED_CONFIG_H */
