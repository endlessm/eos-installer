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
 *     Cosimo Cecchi <cosimo@endlessm.com>
 */

/* Endless EULA page {{{1 */

#define PAGE_ID "endless-eula"

#include "config.h"
#include "gis-endless-eula-page.h"

#include "endless-eula-resources.h"

#include <evince-view.h>
#include <evince-document.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

typedef struct {
  GDBusProxy *metrics_proxy;
} GisEndlessEulaPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisEndlessEulaPage, gis_endless_eula_page, GIS_TYPE_PAGE);

#define OBJ(type,name) ((type)gtk_builder_get_object(GIS_PAGE(page)->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)

#define METRICS_PRIVACY_POLICY_URI "metrics-privacy-policy"

static void
sync_metrics_active_state (GisEndlessEulaPage *page)
{
  GisEndlessEulaPagePrivate *priv = gis_endless_eula_page_get_instance_private (page);
  GError *error = NULL;
  GtkWidget *widget;
  gboolean metrics_active;

  widget = WID ("metrics-checkbutton");
  metrics_active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

  if (!priv->metrics_proxy)
    return;

  g_dbus_proxy_call_sync (priv->metrics_proxy,
                          "SetEnabled",
                          g_variant_new ("(b)", metrics_active),
                          G_DBUS_CALL_FLAGS_NONE, -1,
                          NULL, &error);

  if (error != NULL)
    {
      g_critical ("Unable to set the enabled state of metrics daemon: %s\n", error->message);
      g_error_free (error);
    }
}

static GtkWidget *
build_policy_view (void)
{
  GtkWidget *label;
  PangoAttrList *attr_list;
  PangoAttribute *attr;

  label = gtk_label_new (_("Endless collects metrics on user behavior and actions.\n"
                           "All data sent is anonymous.\n"
                           "We use the data to improve the system."));
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

  attr_list = pango_attr_list_new ();
  attr = pango_attr_scale_new (PANGO_SCALE_LARGE);
  pango_attr_list_insert (attr_list, attr);

  gtk_label_set_attributes (GTK_LABEL (label), attr_list);
  pango_attr_list_unref (attr_list);

  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
  gtk_widget_set_vexpand (label, TRUE);

  return label;
}

static void
show_metrics_privacy_policy (GisEndlessEulaPage *page)
{
  GtkWindow *toplevel;
  GtkWidget *dialog, *view, *content_area;

  toplevel = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (page)));
  dialog = gtk_dialog_new_with_buttons (_("Privacy Policy"),
                                        toplevel,
                                        GTK_DIALOG_MODAL |
                                        GTK_DIALOG_DESTROY_WITH_PARENT |
                                        GTK_DIALOG_USE_HEADER_BAR,
                                        NULL, NULL);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 400, 400);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_container_set_border_width (GTK_CONTAINER (content_area), 16);

  view = build_policy_view ();
  gtk_container_add (GTK_CONTAINER (content_area), view);

  gtk_widget_show_all (dialog);
}

static gboolean
metrics_privacy_label_link_cb (GtkLabel           *label,
                               gchar              *uri,
                               GisEndlessEulaPage *page)
{
  if (g_strcmp0 (uri, METRICS_PRIVACY_POLICY_URI) == 0)
    {
      show_metrics_privacy_policy (page);
      return TRUE;
    }

  return FALSE;
}

static void
gis_endless_eula_page_finalize (GObject *object)
{
  GisEndlessEulaPage *page = GIS_ENDLESS_EULA_PAGE (object);
  GisEndlessEulaPagePrivate *priv = gis_endless_eula_page_get_instance_private (page);

  g_clear_object (&priv->metrics_proxy);

  G_OBJECT_CLASS (gis_endless_eula_page_parent_class)->finalize (object);
}

static GFile *
get_terms_document (void)
{
  const gchar * const * languages;
  const gchar * const * data_dirs;
  const gchar *language;
  gchar *path = NULL;
  gint i, j;
  gboolean found = FALSE;
  GFile *file;

  data_dirs = g_get_system_data_dirs ();
  languages = g_get_language_names ();
  path = NULL;

  for (i = 0; languages[i] != NULL; i++)
    {
      language = languages[i];

      for (j = 0; data_dirs[j] != NULL; j++)
        {
          path = g_build_filename (data_dirs[j],
                                   "eos-license-service",
                                   "terms",
                                   language,
                                   "Endless-Mobile-Terms-of-Use.pdf",
                                   NULL);

          if (g_file_test (path, G_FILE_TEST_EXISTS))
            {
              found = TRUE;
              break;
            }

          g_free (path);
          path = NULL;
        }

      if (found)
        break;
    }

  if (!found)
    {
      g_critical ("Unable to find terms and conditions PDF on the system");
      return NULL;
    }

  file = g_file_new_for_path (path);
  g_free (path);

  return file;
}

static void
load_terms_view (GisEndlessEulaPage *page)
{
  EvDocument *document;
  EvDocumentModel *model;
  GFile *file;
  GtkWidget *widget, *view;
  GError *error = NULL;

  file = get_terms_document ();
  if (file == NULL)
    return;

  document = ev_document_factory_get_document_for_gfile (file,
                                                         EV_DOCUMENT_LOAD_FLAG_NONE,
                                                         NULL,
                                                         &error);
  g_object_unref (file);

  if (error != NULL)
    {
      g_critical ("Unable to load terms and conditions PDF: %s", error->message);
      g_error_free (error);
      return;
    }

  model = ev_document_model_new_with_document (document);
  view = ev_view_new ();
  ev_view_set_model (EV_VIEW (view), model);

  widget = WID ("eula-scrolledwin");
  gtk_container_add (GTK_CONTAINER (widget), view);
  gtk_widget_show (view);
}

static void
load_css_overrides (GisEndlessEulaPage *page)
{
  GtkCssProvider *provider;
  GFile *file;
  GError *error = NULL;

  provider = gtk_css_provider_new ();
  file = g_file_new_for_uri ("resource:///org/gnome/initial-setup/endless-eula-page.css");
  gtk_css_provider_load_from_file (provider, file, &error);
  g_object_unref (file);

  if (error != NULL)
    {
      g_warning ("Unable to load CSS overrides for the endless-eula page: %s",
                 error->message);
      g_error_free (error);
    }
  else
    {
      gtk_style_context_add_provider_for_screen (gtk_widget_get_screen (GTK_WIDGET (page)),
                                                 GTK_STYLE_PROVIDER (provider),
                                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

  g_object_unref (provider);
}

static void
gis_endless_eula_page_constructed (GObject *object)
{
  GisEndlessEulaPage *page = GIS_ENDLESS_EULA_PAGE (object);
  GisEndlessEulaPagePrivate *priv = gis_endless_eula_page_get_instance_private (page);
  GError *error = NULL;
  GtkWidget *widget;

  G_OBJECT_CLASS (gis_endless_eula_page_parent_class)->constructed (object);

  priv->metrics_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                       G_DBUS_PROXY_FLAGS_NONE,
                                                       NULL,
                                                       "com.endlessm.Metrics",
                                                       "/com/endlessm/Metrics",
                                                       "com.endlessm.Metrics.EventRecorderServer",
                                                       NULL, &error);

  if (error != NULL)
    {
      g_critical ("Unable to create a DBus proxy for the metrics daemon: %s", error->message);
      g_error_free (error);
    }

  load_css_overrides (page);

  gtk_container_add (GTK_CONTAINER (page), WID ("endless-eula-page"));
  gtk_widget_show (GTK_WIDGET (page));

  widget = WID ("metrics-checkbutton");
  g_signal_connect_swapped (widget, "toggled",
                            G_CALLBACK (sync_metrics_active_state), page);

  widget = WID ("metrics-privacy-label");
  g_signal_connect (widget, "activate-link",
                    G_CALLBACK (metrics_privacy_label_link_cb), page);

  sync_metrics_active_state (page);
  load_terms_view (page);

  gis_page_set_forward_text (GIS_PAGE (page), _("_Accept and continue"));
  gis_page_set_complete (GIS_PAGE (page), TRUE);
}

static void
gis_endless_eula_page_locale_changed (GisPage *page)
{
  gis_page_set_title (page, _("Terms of Use"));
}

static void
gis_endless_eula_page_class_init (GisEndlessEulaPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_endless_eula_page_locale_changed;
  object_class->constructed = gis_endless_eula_page_constructed;
  object_class->finalize = gis_endless_eula_page_finalize;
}

static void
gis_endless_eula_page_init (GisEndlessEulaPage *page)
{
  g_resources_register (endless_eula_get_resource ());
}

void
gis_prepare_endless_eula_page (GisDriver *driver)
{
  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_ENDLESS_EULA_PAGE,
                                     "driver", driver,
                                     NULL));
}
