/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2016-2017 Endless Mobile, Inc.
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

#ifndef GIS_SCRIBE_H
#define GIS_SCRIBE_H

#include <gio/gio.h>

G_BEGIN_DECLS

GQuark gis_install_error_quark (void);
#define GIS_INSTALL_ERROR gis_install_error_quark()

#define GIS_TYPE_SCRIBE (gis_scribe_get_type ())
G_DECLARE_FINAL_TYPE (GisScribe, gis_scribe, GIS, SCRIBE, GObject)

GisScribe *
gis_scribe_new (GFile       *image,
                guint64      image_size,
                GFile       *signature,
                const gchar *drive_path,
                gint         drive_fd,
                gboolean     convert_to_mbr);

void
gis_scribe_write_async (GisScribe          *self,
                        GCancellable       *cancellable,
                        GAsyncReadyCallback callback,
                        gpointer            user_data);

gboolean
gis_scribe_write_finish (GisScribe    *self,
                         GAsyncResult *result,
                         GError      **error);

guint
gis_scribe_get_step (GisScribe *self);

gdouble
gis_scribe_get_progress (GisScribe *self);

G_END_DECLS

#endif /* GIS_SCRIBE_H */
