/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by:
 *     Emmanuele Bassi <emmanuele@endlessm.com>
 */

/* Display page {{{1 */

#define PAGE_ID "display"

#include "config.h"
#include "gis-display-page.h"

#include "display-resources.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <libgnome-desktop/gnome-rr.h>
#include <libgnome-desktop/gnome-rr-config.h>

typedef struct {
  GnomeRRScreen *screen;
  GnomeRRConfig *current_config;
  GnomeRROutputInfo *current_output;
} GisDisplayPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisDisplayPage, gis_display_page, GIS_TYPE_PAGE);

#define OBJ(type,name) ((type)gtk_builder_get_object(GIS_PAGE(page)->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)

static void
read_screen_config (GisDisplayPage *page)
{
  GisDisplayPagePrivate *priv = gis_display_page_get_instance_private (page);
  GnomeRRConfig *current;
  GnomeRROutputInfo **outputs;
  GnomeRROutputInfo *output;
  GtkWidget *check_button;
  int i;

  gnome_rr_screen_refresh (priv->screen, NULL);

  g_clear_object (&priv->current_config);

  current = gnome_rr_config_new_current (priv->screen, NULL);
  gnome_rr_config_ensure_primary (current);
  priv->current_config = current;

  outputs = gnome_rr_config_get_outputs (current);
  output = NULL;

  /* we take the primary and active display */
  for (i = 0; outputs[i] != NULL; i++)
    {
      if (gnome_rr_output_info_is_active (outputs[i]) &&
          gnome_rr_output_info_get_primary (outputs[i]))
        {
          output = outputs[i];
          break;
        }
    }

  check_button = WID ("overscan_checkbutton");

  priv->current_output = output;
  if (priv->current_output == NULL)
    {
      GtkWidget *label, *widget;

      gtk_widget_hide (check_button);

      /* Translators note: this is the same label we use in the
       * Display page of the system settings
       */
      label = gtk_label_new (_("Could not get screen information"));
      widget = WID ("box2");
      gtk_container_add (GTK_CONTAINER (widget), label);
      gtk_widget_show (label);

      return;
    }

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
                                gnome_rr_output_info_get_underscanning (output));


}

static void
toggle_overscan (GisDisplayPage *page)
{
  GisDisplayPagePrivate *priv = gis_display_page_get_instance_private (page);
  GtkWidget *check = WID ("overscan_checkbutton");
  gboolean value = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check));
  GError *error;

  gnome_rr_output_info_set_underscanning (priv->current_output, value);

  gnome_rr_config_sanitize (priv->current_config);
  gnome_rr_config_ensure_primary (priv->current_config);

  error = NULL;
  gnome_rr_config_apply_persistent (priv->current_config, priv->screen, &error);

  if (error != NULL)
    {
      g_warning ("Error applying configuration: %s", error->message);
      g_error_free (error);
    }
}

static void
gis_display_page_dispose (GObject *gobject)
{
  GisDisplayPage *page = GIS_DISPLAY_PAGE (gobject);
  GisDisplayPagePrivate *priv = gis_display_page_get_instance_private (page);

  g_clear_object (&priv->current_config);
  g_clear_object (&priv->screen);

  G_OBJECT_CLASS (gis_display_page_parent_class)->dispose (gobject);
}

static void
gis_display_page_constructed (GObject *object)
{
  GisDisplayPage *page = GIS_DISPLAY_PAGE (object);
  GisDisplayPagePrivate *priv = gis_display_page_get_instance_private (page);
  GError *error = NULL;
  GtkWidget *widget;

  G_OBJECT_CLASS (gis_display_page_parent_class)->constructed (object);

  gtk_container_add (GTK_CONTAINER (page), WID ("display-page"));
  gtk_widget_show (GTK_WIDGET (page));

  /* the page is always complete */
  gis_page_set_complete (GIS_PAGE (page), TRUE);

  priv->screen = gnome_rr_screen_new (gdk_screen_get_default (), NULL);
  if (priv->screen == NULL)
    {
      GtkWidget *label;

      widget = WID ("overscan_checkbutton");
      gtk_widget_hide (widget);

      /* Translators note: this is the same label we use in the
       * Display page of the system settings
       */
      label = gtk_label_new (_("Could not get screen information"));
      widget = WID ("box2");
      gtk_container_add (GTK_CONTAINER (widget), label);
      gtk_widget_show (label);

      return;
    }

  read_screen_config (page);

  widget = WID ("overscan_checkbutton");
  g_signal_connect_swapped (widget, "toggled",
                            G_CALLBACK (toggle_overscan),
                            page);
}

static void
gis_display_page_locale_changed (GisPage *page)
{
  gis_page_set_title (page, _("Display"));
}

static void
gis_display_page_class_init (GisDisplayPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_display_page_locale_changed;
  object_class->constructed = gis_display_page_constructed;
  object_class->dispose = gis_display_page_dispose;
}

static void
gis_display_page_init (GisDisplayPage *page)
{
  g_resources_register (display_get_resource ());
}

void
gis_prepare_display_page (GisDriver *driver)
{
  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_DISPLAY_PAGE,
                                     "driver", driver,
                                     NULL));
}
