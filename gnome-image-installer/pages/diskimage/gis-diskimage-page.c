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
#include "gis-store.h"

#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <errno.h>

struct _GisDiskImagePagePrivate {
  gint dummy;
};
typedef struct _GisDiskImagePagePrivate GisDiskImagePagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisDiskImagePage, gis_diskimage_page, GIS_TYPE_PAGE);

#define OBJ(type,name) ((type)gtk_builder_get_object(GIS_PAGE(page)->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)


static void
gis_diskimage_page_selection_changed(GtkTreeSelection *selection, GisPage *page)
{
  GtkTreeIter i;
  gchar *image = NULL;
  GtkTreeModel *model = NULL;
  GFile *file = NULL;

  if (!gtk_tree_selection_get_selected(selection, &model, &i))
    {
      gis_page_set_complete (page, FALSE);
      return;
    }

  gtk_tree_model_get(model, &i, 2, &image, -1);

  file = g_file_new_for_path (image);
  gis_store_set_object (GIS_STORE_IMAGE, G_OBJECT (file));
  g_object_unref(file);

  gis_page_set_complete (page, TRUE);
}

static gchar *get_display_name(gchar *fullname)
{
  GRegex *reg;
  GMatchInfo *info;
  gchar *name = NULL;

  reg = g_regex_new ("eos-eos(\\d\\.\\d).*\\.(\\w*)\\.img\\.(x|g)z", 0, 0, NULL);
  g_regex_match (reg, fullname, 0, &info);
  if (g_match_info_matches (info))
    {
      gchar *version = g_match_info_fetch(info, 1);
      gchar *flavour = g_match_info_fetch(info, 2);
      name = g_strdup_printf("EOS %s %s", version, flavour);
      g_free(version);
      g_free(flavour);
    }

  return name;
}

static void add_image(GtkListStore *store, gchar *image)
{
  GtkTreeIter i;
  GError *error = NULL;
  GFile *f = g_file_new_for_path (image);
  GFileInfo *fi = g_file_query_info (f, G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                     G_FILE_QUERY_INFO_NONE, NULL,
                                     &error);
  if (fi != NULL)
    {
      gchar *size = g_strdup_printf ("%.02f GB", (float)g_file_info_get_size (fi)/1024.0/1024.0/1024.0);
      gchar *displayname = get_display_name(image);
      if (displayname == NULL)
          displayname = g_file_get_basename(f);

      gtk_list_store_append (store, &i);
      gtk_list_store_set (store, &i,
                          0, displayname,
                          1, size, 2, image, -1);
      g_free (size);
      g_free (displayname);
    }
  else
    {
      g_error ("Could not get file info: %s", error->message);
    }
  g_object_unref(f);
  g_object_unref(fi);
}

static void
gis_disktarget_page_populate_model(GisPage *page, gchar *path)
{
  GError *error = NULL;
  gchar *file = NULL;
  GtkListStore *store = OBJ(GtkListStore*, "image_store");
  GDir *dir = g_dir_open (path, 0, &error);

  if (dir == NULL)
    return;

  gtk_list_store_clear(store);

  for (file = (gchar*)g_dir_read_name (dir); file != NULL; file = (gchar*)g_dir_read_name (dir))
    {
      gchar *fullpath = g_build_path ("/", path, file, NULL);
      add_image(store, fullpath);
      g_free (fullpath);
    }

  g_dir_close (dir);
}

static void
gis_diskimage_page_browse(GtkButton *selection, GisPage *page)
{
  GtkWidget *dialog;
  gint res;

  dialog = gtk_file_chooser_dialog_new ("Open image",
                                        GTK_WINDOW (gtk_widget_get_toplevel(GTK_WIDGET(page))),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        _("Cancel"), GTK_RESPONSE_CANCEL,
                                        _("Open"), GTK_RESPONSE_ACCEPT,
                                        NULL);
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(dialog), "/eosimages");
  res = gtk_dialog_run (GTK_DIALOG (dialog));

  if (res == GTK_RESPONSE_ACCEPT)
    {
      GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
      char *file = gtk_file_chooser_get_filename (chooser);
      GtkListStore *store = OBJ (GtkListStore*, "image_store");
      GtkTreeSelection *selection = OBJ(GtkTreeSelection*, "image_selection");
      GtkTreeIter iter;
      gint n;

      add_image (store, file);
      g_free (file);

      n = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL);
      if (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL (store), &iter, NULL, n-1))
        {
          gtk_tree_selection_select_iter(selection, &iter);
        }
    }

  gtk_widget_destroy (dialog);
}

static void
gis_diskimage_page_shown (GisPage *page)
{
  GisDiskImagePage *diskimage = GIS_DISK_IMAGE_PAGE (page);
  GisDiskImagePagePrivate *priv = gis_diskimage_page_get_instance_private (diskimage);

  gis_driver_save_data (GIS_PAGE (page)->driver);

  /* FIXME: This should be the mount point of the image partition. Make it generic.
   * TODO : Fall back chain through something to working directory?
   */
  gis_disktarget_page_populate_model(page, "/eosimages");
}

static void
gis_diskimage_page_constructed (GObject *object)
{
  GisDiskImagePage *page = GIS_DISK_IMAGE_PAGE (object);

  G_OBJECT_CLASS (gis_diskimage_page_parent_class)->constructed (object);

  gtk_container_add (GTK_CONTAINER (page), WID ("diskimage-page"));

  gis_page_set_complete (GIS_PAGE (page), FALSE);

  g_signal_connect(OBJ(GObject*, "image_selection"),
                       "changed", G_CALLBACK(gis_diskimage_page_selection_changed),
                       page);

  g_signal_connect(OBJ(GObject*, "browse_button"),
                       "clicked", G_CALLBACK(gis_diskimage_page_browse),
                       page);

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
