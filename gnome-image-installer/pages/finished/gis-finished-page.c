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

/* Finished page {{{1 */

#define PAGE_ID "finished"

#include "config.h"
#include "finished-resources.h"
#include "gis-finished-page.h"
#include "gis-store.h"

#include <gtk/gtkx.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <errno.h>

#include <udisks/udisks.h>

struct _GisFinishedPagePrivate {
  gint led_state;
};
typedef struct _GisFinishedPagePrivate GisFinishedPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisFinishedPage, gis_finished_page, GIS_TYPE_PAGE);

#define OBJ(type,name) ((type)gtk_builder_get_object(GIS_PAGE(page)->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)


static void
reboot_cb (GtkButton *button, GisFinishedPage *page)
{
  g_spawn_command_line_sync ("/usr/bin/systemctl poweroff", NULL, NULL, NULL, NULL);
  g_application_quit(G_APPLICATION (GIS_PAGE (page)->driver));
}

static gboolean
toggle_leds (GisPage *page)
{
  XKeyboardControl values;
  GisFinishedPage *summary = GIS_FINISHED_PAGE (page);
  GisFinishedPagePrivate *priv = gis_finished_page_get_instance_private (summary);

  values.led_mode = priv->led_state;
  XChangeKeyboardControl(GDK_DISPLAY_XDISPLAY(gtk_widget_get_display(GTK_WIDGET(page))), KBLedMode, &values);
  priv->led_state = priv->led_state == 1 ? 0 : 1;
  return TRUE;
}

static void
gis_finished_page_shown (GisPage *page)
{
  GError *error = gis_store_get_error();

  gis_driver_save_data (GIS_PAGE (page)->driver);

  if (error != NULL)
    {
      GisAssistant *assistant = gis_driver_get_assistant (page->driver);
      gtk_label_set_text (OBJ (GtkLabel*, "error_label"), error->message);
      gtk_widget_show (WID ("error_box"));
      gtk_widget_hide (WID ("success_box"));
      gis_assistant_locale_changed (assistant);

      if (gis_store_is_unattended())
        {
          g_timeout_add_seconds (1, (GSourceFunc)toggle_leds, page);
        }
    }
  else
    {
      UDisksDrive *drive = UDISKS_DRIVE (gis_store_get_object (GIS_STORE_IMAGE_DRIVE));
      gboolean optical = udisks_drive_get_optical (drive);

      gtk_widget_set_visible (WID ("removelabel_usb"), !optical);
      gtk_widget_set_visible (WID ("removelabel_dvd"),  optical);
    }
}

static void
gis_finished_page_constructed (GObject *object)
{
  GisFinishedPage *page = GIS_FINISHED_PAGE (object);

  G_OBJECT_CLASS (gis_finished_page_parent_class)->constructed (object);

  gtk_container_add (GTK_CONTAINER (page), WID ("finished-page"));

  gis_page_set_complete (GIS_PAGE (page), TRUE);

  g_signal_connect (OBJ (GtkButton *, "restart_button"), "clicked", G_CALLBACK(reboot_cb), page);

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_finished_page_locale_changed (GisPage *page)
{
  if (gis_store_get_error() == NULL)
    {
      gis_page_set_title (page, _("Reformatting Finished"));
    }
  else
    {
      gis_page_set_title (page, "");
    }
}

static void
gis_finished_page_class_init (GisFinishedPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_finished_page_locale_changed;
  page_class->shown = gis_finished_page_shown;
  object_class->constructed = gis_finished_page_constructed;
}

static void
gis_finished_page_init (GisFinishedPage *page)
{
  g_resources_register (finished_get_resource ());
}

void
gis_prepare_finished_page (GisDriver *driver)
{
  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_FINISHED_PAGE,
                                     "driver", driver,
                                     NULL));
}
