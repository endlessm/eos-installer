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
 */
#pragma once

#include <glib.h>

GQuark gis_image_error_quark (void);
#define GIS_IMAGE_ERROR gis_image_error_quark()

GQuark gis_disk_error_quark (void);
#define GIS_DISK_ERROR gis_disk_error_quark()

GQuark gis_install_error_quark (void);
#define GIS_INSTALL_ERROR gis_install_error_quark()

