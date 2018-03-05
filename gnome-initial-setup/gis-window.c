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

#include "config.h"

#include "gis-assistant.h"
#include "gis-window.h"

struct _GisWindowPrivate {
  GisAssistant *assistant;
};
typedef struct _GisWindowPrivate GisWindowPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GisWindow, gis_window, GTK_TYPE_APPLICATION_WINDOW)

static gboolean
is_event_on_title (GisWindow *window,
		   GdkEventButton *event)
{
  GisWindowPrivate *priv = gis_window_get_instance_private (window);
  GtkAllocation allocation;
  GtkWidget *titlebar, *src;
  gint x, y;

  titlebar = gis_assistant_get_titlebar (priv->assistant);

  gdk_window_get_user_data (event->window, (gpointer *)&src);
  if (src && src != GTK_WIDGET (window))
    {
      gtk_widget_translate_coordinates (src, GTK_WIDGET (window),
					event->x, event->y, &x, &y);
    }
  else
    {
      x = event->x;
      y = event->y;
    }

  if (titlebar != NULL &&
      gtk_widget_get_visible (titlebar) &&
      gtk_widget_get_child_visible (titlebar))
    {
      gtk_widget_get_allocation (titlebar, &allocation);
      if (allocation.x <= x && allocation.x + allocation.width > x &&
          allocation.y <= y && allocation.y + allocation.height > y)
        return TRUE;
    }

  return FALSE;
}

static gboolean
gis_window_button_press_event (GtkWidget      *widget,
			       GdkEventButton *event)
{
  GisWindow *window = GIS_WINDOW (widget);

  /* eat all the right clicks on the titlebar, since we run in a special session */
  if (is_event_on_title (window, event) &&
      event->button == GDK_BUTTON_SECONDARY)
    return TRUE;

  return GTK_WIDGET_CLASS (gis_window_parent_class)->button_press_event (widget, event);
}

static void
gis_window_class_init (GisWindowClass *klass)
{
  GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);

  wclass->button_press_event = gis_window_button_press_event;
}

static void
gis_window_init (GisWindow *window)
{
  GisWindowPrivate *priv = gis_window_get_instance_private (window);
  GdkGeometry size_hints;

  priv->assistant = g_object_new (GIS_TYPE_ASSISTANT, NULL);
  gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (priv->assistant));
  gtk_widget_show (GTK_WIDGET (priv->assistant));
  gtk_window_set_titlebar (GTK_WINDOW (window), gis_assistant_get_titlebar (priv->assistant));
}

GisAssistant *
gis_window_get_assistant (GisWindow *window)
{
  GisWindowPrivate *priv;

  g_return_val_if_fail (GIS_IS_WINDOW (window), NULL);

  priv = gis_window_get_instance_private (window);
  return priv->assistant;
}

GtkWidget *
gis_window_new (GisDriver *driver)
{
  return g_object_new (GIS_TYPE_WINDOW,
		       "application", driver,
		       "type", GTK_WINDOW_TOPLEVEL,
		       "icon-name", "com.endlessm.Installer",
		       "resizable", TRUE,
		       "window-position", GTK_WIN_POS_CENTER_ALWAYS,
		       "deletable", FALSE,
		       NULL);
}
