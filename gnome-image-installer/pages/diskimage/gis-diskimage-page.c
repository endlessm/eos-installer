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

/* DiskImage page {{{1 */

#define PAGE_ID "diskimage"

#include "config.h"
#include "diskimage-resources.h"
#include "gis-diskimage-page.h"

#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <errno.h>

struct _GisDiskImagePagePrivate {
};
typedef struct _GisDiskImagePagePrivate GisDiskImagePagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisDiskImagePage, gis_diskimage_page, GIS_TYPE_PAGE);

#define OBJ(type,name) ((type)gtk_builder_get_object(GIS_PAGE(page)->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)


static void
done_cb (GtkButton *button, GisDiskImagePage *page)
{
  switch (gis_driver_get_mode (GIS_PAGE (page)->driver))
    {
    case GIS_DRIVER_MODE_NEW_USER:
      gis_driver_hide_window (GIS_PAGE (page)->driver);
      break;
    case GIS_DRIVER_MODE_EXISTING_USER:
      gis_add_setup_done_file ();
      break;
    }

    launch_tutorial (page);
}

static void
gis_diskimage_page_shown (GisPage *page)
{
  GisDiskImagePage *summary = GIS_DISK_IMAGE_PAGE (page);
  GisDiskImagePagePrivate *priv = gis_diskimage_page_get_instance_private (summary);

  gis_driver_save_data (GIS_PAGE (page)->driver);

}

static void
gis_diskimage_page_constructed (GObject *object)
{
  GisDiskImagePage *page = GIS_DISK_IMAGE_PAGE (object);

  G_OBJECT_CLASS (gis_diskimage_page_parent_class)->constructed (object);

  gtk_container_add (GTK_CONTAINER (page), WID ("diskimage-page"));

  gis_page_set_complete (GIS_PAGE (page), TRUE);

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_diskimage_page_locale_changed (GisPage *page)
{
  gis_page_set_title (page, _("Select File"));
}

static void
gis_diskimage_page_class_init (GisDiskImagePageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_diskimage_page_locale_changed;
  page_class->shown = gis_diskimage_page_shown;
  object_class->constructed = gis_diskimage_page_constructed;
}

static void
gis_diskimage_page_init (GisDiskImagePage *page)
{
  g_resources_register (diskimage_get_resource ());
}

void
gis_prepare_diskimage_page (GisDriver *driver)
{
  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_DISK_IMAGE_PAGE,
                                     "driver", driver,
                                     NULL));
}
