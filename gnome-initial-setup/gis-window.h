/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2012 Red Hat
 * Copyright (C) 2014 Endless Mobile, Inc.
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
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 *     Cosimo Cecchi <cosimo@endlessm.com>
 */

#ifndef __GIS_WINDOW_H__
#define __GIS_WINDOW_H__

#include <gtk/gtk.h>

#include "gis-driver.h"

G_BEGIN_DECLS

#define GIS_TYPE_WINDOW            (gis_window_get_type ())
#define GIS_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIS_TYPE_WINDOW, GisWindow))
#define GIS_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GIS_TYPE_WINDOW, GisWindowClass))
#define GIS_IS_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIS_TYPE_WINDOW))
#define GIS_IS_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GIS_TYPE_WINDOW))
#define GIS_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GIS_TYPE_WINDOW, GisWindowClass))

typedef struct _GisWindow        GisWindow;
typedef struct _GisWindowClass   GisWindowClass;

struct _GisWindow
{
  GtkApplicationWindow parent;
};

struct _GisWindowClass
{
  GtkApplicationWindowClass parent_class;
};

GType gis_window_get_type (void);

GtkWidget *gis_window_new (GisDriver *driver);
GisAssistant * gis_window_get_assistant (GisWindow *window);

G_END_DECLS

#endif /* __GIS_WINDOW_H__ */
