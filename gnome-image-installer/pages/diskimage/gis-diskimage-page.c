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
#include "gpt_gz.h"
#include "gpt_lzma.h"

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>

#include <udisks/udisks.h>
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

G_DEFINE_QUARK(image-error, gis_image_error);
#define GIS_IMAGE_ERROR gis_image_error_quark()


static void
gis_diskimage_page_selection_changed(GtkWidget *combo, GisPage *page)
{
  GtkTreeIter i;
  gchar *image, *name = NULL;
  GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
  GFile *file = NULL;

  if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &i))
    {
      gis_page_set_complete (page, FALSE);
      return;
    }

  gtk_tree_model_get(model, &i, 0, &name, 2, &image, -1);

  gis_store_set_image_name (name);
  g_free (name);

  file = g_file_new_for_path (image);
  gis_store_set_object (GIS_STORE_IMAGE, G_OBJECT (file));
  if (g_str_has_suffix (image, ".gz"))
    {
      gint64 size = get_gzip_disk_image_size (image);
      if (size <= 0)
        {
          size = 1*1024*1024*1024;
          size *= 8;
        }
      gis_store_set_required_size (size);
    }
  else if (g_str_has_suffix (image, ".xz"))
    {
      gint64 size = get_xz_disk_image_size (image);
      if (size <= 0)
        {
          size = 1*1024*1024*1024;
          size *= 8;
        }
      gis_store_set_required_size (size);
    }
  g_object_unref(file);

  gis_page_set_complete (page, TRUE);

  if (gis_store_is_unattended())
    {
      if (gtk_tree_model_iter_n_children (model, NULL) > 1)
        {
          GError *error = g_error_new (GIS_IMAGE_ERROR, 0, _("No suitable images were found."));
          gis_store_set_error (error);
          g_clear_error (&error);
        }
      gis_assistant_next_page (gis_driver_get_assistant (page->driver));
    }
}

static gchar *get_display_name(gchar *fullname)
{
  GRegex *reg;
  GMatchInfo *info;
  gchar *name = NULL;

  reg = g_regex_new ("^.*/([^-]+)-([^-]+)-([^-]+)-([^.]+)\\.([^.]+)\\.([^.]+)(?:\\.(disk\\d))?\\.img\\.([gx]z)$", 0, 0, NULL);
  g_regex_match (reg, fullname, 0, &info);
  if (g_match_info_matches (info))
    {
      gchar *product = g_match_info_fetch (info, 1);
      gchar *version = g_match_info_fetch (info, 2);
      gchar *personality = g_match_info_fetch (info, 6);
      gchar *type = g_match_info_fetch (info, 7);
      gchar *language = NULL;

      /* Split images not supported yet */
      if (strlen (type) > 0)
        {
          g_free (version);
          g_free (personality);
          g_free (type);
          return NULL;
        }

      if (g_str_equal (product, "eos"))
        {
          g_free (product);
          product = g_strdup ("Endless OS");
        }
      else if (g_str_equal (product, "eosinstaller"))
        {
          g_free (product);
          product = g_strdup ("Endless OS Installer");
        }
      else if (g_str_equal (product, "eosnonfree"))
        {
          g_free (product);
          product = g_strdup ("Endless OS (non-free)");
        }

      if (g_str_has_prefix (version, "eos"))
        {
          gchar *tmp = g_strdup(version+3);
          g_free (version);
          version = tmp;
        }

      if (g_str_equal (personality, "base"))
        {
          g_free (personality);
          personality = g_strdup(_("Light"));
          name = g_strdup_printf ("%s %s %s", product, version, personality);
        }
      else
        {
          g_free (language);
          language = gnome_get_language_from_locale (personality, NULL);
          /* TODO: what is this stupid grumblegrumble... */
          if (language != NULL && g_strrstr (language, "[") != NULL)
            {
              gchar **split = g_strsplit (language, " [", 0);
              g_free (language);
              language = g_strdup (split[0]);
              g_strfreev (split);
            }
          
          g_free (personality);
          personality = g_strdup (_("Full"));
          
          if (language != NULL)
            name = g_strdup_printf ("%s %s %s %s", product, version, language, personality);
        }
        
      g_free (version);
      g_free (personality);
      g_free (type);
      g_free (language);
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
      gchar *size = NULL;
      gchar *displayname = NULL;

      if ((g_str_has_suffix (image, ".gz") && get_gzip_is_valid_eos_gpt (image) == 1)
       || (g_str_has_suffix (image, ".xz") && get_xz_is_valid_eos_gpt (image) == 1))
        {
          displayname = get_display_name (image);
        }

      if (displayname != NULL)
        {
          size = g_strdup_printf ("%.02f GB", (float)g_file_info_get_size (fi)/1024.0/1024.0/1024.0);

          gtk_list_store_append (store, &i);
          gtk_list_store_set (store, &i,
                              0, displayname,
                              1, size, 2, image, -1);
          g_free (size);
          g_free (displayname);
        }
    }
  else
    {
      g_warning ("Could not get file info: %s", error->message);
    }

  g_object_unref(f);
  g_object_unref(fi);
}

static void
gis_disktarget_page_populate_model(GisPage *page, gchar *path)
{
  GError *error = NULL;
  gchar *file = NULL;
  gchar *ufile = NULL;
  GtkListStore *store = OBJ(GtkListStore*, "image_store");
  GDir *dir = g_dir_open (path, 0, &error);
  GtkTreeIter iter;

  if (dir == NULL)
    {
      gis_store_set_error (error);
      g_clear_error (&error);
      gis_assistant_next_page (gis_driver_get_assistant (page->driver));
      return;
    }

  gtk_list_store_clear(store);

  if (gis_store_is_unattended())
    {
      GKeyFile *keys = gis_store_get_key_file();
      if (keys != NULL)
        {
          ufile = g_key_file_get_string (keys, "Unattended", "image", NULL);
        }
    }

  for (file = (gchar*)g_dir_read_name (dir); file != NULL; file = (gchar*)g_dir_read_name (dir))
    {
      gchar *fullpath = g_build_path ("/", path, file, NULL);
      if (gis_store_is_unattended() && ufile != NULL)
        {
          if (g_str_equal (ufile, file))
              add_image(store, fullpath);
        }
      else
        {
          add_image(store, fullpath);
        }
      g_free (fullpath);
    }

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter))
    {
      GtkComboBox *combo = OBJ (GtkComboBox*, "imagecombo");
      gtk_combo_box_set_active_iter (combo, &iter);
    }
  else
    {
      error = g_error_new (GIS_IMAGE_ERROR, 0, _("No suitable images were found."));
      gis_store_set_error (error);
      g_clear_error (&error);
      gis_assistant_next_page (gis_driver_get_assistant (page->driver));
    }

  g_dir_close (dir);
}

static void
gis_diskimage_page_mount_ready (GObject *source, GAsyncResult *res, GisPage *page)
{
  UDisksFilesystem *fs = UDISKS_FILESYSTEM (source);
  GError *error = NULL;
  gchar *path = NULL;

  if (!udisks_filesystem_call_mount_finish (fs, &path, res, &error))
    {
      gis_store_set_error (error);
      g_error_free (error);
      gis_assistant_next_page (gis_driver_get_assistant (page->driver));
      return;
    }

  gis_disktarget_page_populate_model(page, path);
}

static void
gis_diskimage_page_mount (GisPage *page)
{
  GError *error = NULL;
  UDisksClient *client = udisks_client_new_sync(NULL, &error);
  GDBusObjectManager *manager = udisks_client_get_object_manager(client);
  GList *objects = g_dbus_object_manager_get_objects(manager);
  GList *l;

  for (l = objects; l != NULL; l = l->next)
    {
      UDisksObject *object = UDISKS_OBJECT (l->data);
      UDisksBlock *block = udisks_object_peek_block (object);
      UDisksFilesystem *fs = NULL;
      const gchar *const*mounts = NULL;

      if (block == NULL)
        continue;

      if (!g_str_equal ("eosimages", udisks_block_get_id_label (block)))
        continue;

      fs = udisks_object_peek_filesystem (object);

      if (fs == NULL)
        {
          continue;
        }

      mounts = udisks_filesystem_get_mount_points (fs);

      if (mounts != NULL && mounts[0] != NULL)
        {
          gis_disktarget_page_populate_model(page, (gchar*)mounts[0]);
          return;
        }

      udisks_filesystem_call_mount (fs, g_variant_new ("a{sv}", NULL), NULL,
                                    (GAsyncReadyCallback)gis_diskimage_page_mount_ready, page);

      return;
    }

  error = g_error_new (GIS_IMAGE_ERROR, 0, _("No suitable images were found."));
  gis_store_set_error (error);
  g_clear_error (&error);
  gis_assistant_next_page (gis_driver_get_assistant (page->driver));
}

static void
gis_diskimage_page_shown (GisPage *page)
{
  GisDiskImagePage *diskimage = GIS_DISK_IMAGE_PAGE (page);
  GisDiskImagePagePrivate *priv = gis_diskimage_page_get_instance_private (diskimage);

  gis_driver_save_data (GIS_PAGE (page)->driver);

  gis_diskimage_page_mount (page);
}

static void
gis_diskimage_page_constructed (GObject *object)
{
  GisDiskImagePage *page = GIS_DISK_IMAGE_PAGE (object);

  G_OBJECT_CLASS (gis_diskimage_page_parent_class)->constructed (object);

  gtk_container_add (GTK_CONTAINER (page), WID ("diskimage-page"));

  gis_page_set_complete (GIS_PAGE (page), FALSE);

  g_signal_connect(OBJ(GObject*, "imagecombo"),
                       "changed", G_CALLBACK(gis_diskimage_page_selection_changed),
                       page);

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_diskimage_page_locale_changed (GisPage *page)
{
  gis_page_set_title (page, _("Install Endless OS"));
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
