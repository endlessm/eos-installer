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

/* DiskTarget page {{{1 */

#define PAGE_ID "disktarget"

#include "config.h"
#include "disktarget-resources.h"
#include "gis-disktarget-page.h"
#include "gis-store.h"

#include <udisks/udisks.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <errno.h>

G_DEFINE_QUARK(disk-error, gis_disk_error);

struct _GisDiskTargetPagePrivate {
  UDisksClient *client;
  gboolean has_valid_disks;
};
typedef struct _GisDiskTargetPagePrivate GisDiskTargetPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisDiskTargetPage, gis_disktarget_page, GIS_TYPE_PAGE);

#define OBJ(type,name) ((type)gtk_builder_get_object(GIS_PAGE(page)->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)

static void
check_can_continue(GisDiskTargetPage *page)
{
  GisDiskTargetPagePrivate *priv = gis_disktarget_page_get_instance_private (page);
  GtkToggleButton *button = OBJ (GtkToggleButton*, "confirmbutton");
  GtkToggleButton *pbutton = OBJ (GtkToggleButton*, "partitionbutton");
  GisImage *image = gis_store_get_selected_image ();
  UDisksBlock *block = UDISKS_BLOCK (gis_store_get_object (GIS_STORE_BLOCK_DEVICE));
  UDisksDrive *drive = NULL;

  if (!priv->has_valid_disks)
    return;

  gis_page_set_complete (GIS_PAGE (page), FALSE);

  if (block == NULL)
    return;

  drive = udisks_client_get_drive_for_block (priv->client, UDISKS_BLOCK(block));
  if (drive == NULL)
    return;

  if (udisks_drive_get_size (drive) < image->uncompressed_size)
    return;

  if (!gtk_toggle_button_get_active (button))
    return;

  if (gtk_widget_get_visible (GTK_WIDGET (pbutton)) && !gtk_toggle_button_get_active (pbutton))
    return;

  gis_page_set_complete (GIS_PAGE (page), TRUE);
}

static void
gis_disktarget_page_confirm_toggled(GtkToggleButton *button, GisDiskTargetPage *page)
{
  check_can_continue(page);
}

static void
gis_disktarget_page_selection_changed(GtkWidget *combo, GisPage *page)
{
  GtkTreeIter i;
  GisDiskTargetPage *disktarget = GIS_DISK_TARGET_PAGE (page);
  GisDiskTargetPagePrivate *priv = gis_disktarget_page_get_instance_private (disktarget);
  GObject *block = NULL;
  GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
  gboolean has_data_partitions = FALSE;

  gtk_widget_hide (WID ("partitionbutton"));

  if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &i))
    {
      gtk_widget_hide (WID ("confirm_box"));
      gtk_widget_hide (WID ("partitionbutton"));
      gtk_widget_show (WID ("error_box"));
      return;
    }

  gtk_tree_model_get(model, &i, 2, &block, 3, &has_data_partitions, -1);

  if (block != NULL)
    {
      GisImage *image = gis_store_get_selected_image ();
      UDisksDrive *drive = udisks_client_get_drive_for_block (priv->client, UDISKS_BLOCK(block));
      gis_store_set_object (GIS_STORE_BLOCK_DEVICE, block);
      g_object_unref(block);

      if (udisks_drive_get_size (drive) < image->uncompressed_size)
        {
          g_autofree gchar *size = g_format_size_full (
              image->uncompressed_size,
              G_FORMAT_SIZE_LONG_FORMAT);
          g_autofree gchar *msg = g_strdup_printf (
              _("The location you have chosen is too small: you need %s to reformat with %s."),
              size,
              image->name);

          gtk_label_set_text (OBJ (GtkLabel*, "too_small_label"), msg);
          gtk_widget_hide (WID ("confirm_box"));
          gtk_widget_show (WID ("error_box"));
          check_can_continue (disktarget);
          return;
        }
    }

  if (gis_store_is_unattended())
    {
      if (gtk_tree_model_iter_n_children (model, NULL) > 1)
        {
          const gchar *text = gtk_label_get_text (OBJ (GtkLabel*, "suitable_disks_label"));
          GError *error = g_error_new_literal (GIS_DISK_ERROR, 0, text);
          gis_store_set_error (error);
          g_clear_error (&error);
        }
      gis_assistant_next_page (gis_driver_get_assistant (page->driver));
    }
  else
    {
      gtk_widget_show (WID ("confirm_box"));
      if (has_data_partitions)
        gtk_widget_show (WID ("partitionbutton"));
      gtk_widget_hide (WID ("error_box"));
      check_can_continue(disktarget);
    }
}

static gboolean
gis_disktarget_page_has_data_partitions(GisPage *page, GList *objects, UDisksBlock* block)
{
  GisDiskTargetPage *disktarget = GIS_DISK_TARGET_PAGE (page);
  GisDiskTargetPagePrivate *priv = gis_disktarget_page_get_instance_private (disktarget);
  UDisksPartitionTable *table;
  UDisksObject *obj = NULL;
  gint datapartitions = 0;
  GList *l;

  for (l = objects; l != NULL; l = l->next)
    {
      UDisksBlock *blockiter = udisks_object_peek_block (UDISKS_OBJECT (l->data));
      if (blockiter == block)
        {
          obj = UDISKS_OBJECT (l->data);
          break;
        }
    }

  table = udisks_object_peek_partition_table (obj);

  if (table == NULL)
    return FALSE;

  l = udisks_client_get_partitions(priv->client, table);
  if (g_list_length (l) < 2)
    return FALSE;

  for (; l != NULL; l = l->next)
    {
      UDisksPartition *part = UDISKS_PARTITION(l->data);
      const gchar *type = udisks_partition_get_type_(part);

#define TYPE_IS(t) g_str_equal (type, t)
      if (TYPE_IS("ebd0a0a2-b9e5-4433-87c0-68b6b72699c7") /* Microsoft basic data partition */
       || TYPE_IS("af9b60a0-1431-4f62-bc68-3311714a69ad") /* Logical Disk Manager data partition	*/
       || TYPE_IS("0fc63daf-8483-4772-8e79-3d69d8477de4") /* Linux filesystem */
       || TYPE_IS("44479540-f297-41b2-9af7-d131d5f0458a") /* Root partition (x86) */
       || TYPE_IS("4f68bce3-e8cd-4db1-96e7-fbcaf984b709") /* Root partition (x86-64) */
       || TYPE_IS("69dad710-2ce4-4e3c-b16c-21a1d49abed3") /* Root partition (32-bit ARM) */
       || TYPE_IS("b921b045-1df0-41c3-af44-4c6f280d3fae") /* Root partition (64-bit ARM) */
       || TYPE_IS("e6d6d379-f507-44c2-a23c-238f2a3df928") /* LVM partition */
       || TYPE_IS("933ac7e1-2eb4-4f13-b844-0e14e2aef915") /* /home partition */
       || TYPE_IS("3b8f8425-20e0-4f3b-907f-1a25a76f98e8") /* /srv (server data) */
       || TYPE_IS("7ffec5c9-2d00-49b7-8941-3ea10a5586b7") /* Plain dm-crypt partition */
       || TYPE_IS("ca7d7ccb-63ed-4c53-861c-1742536059cc") /* LUKS partition */
       || TYPE_IS("0x07") /* Windows / OSX data partition */
       || TYPE_IS("0x0b") /* FAT32 (CHS) */
       || TYPE_IS("0x0c") /* FAT32 (LBA) */
       || TYPE_IS("0x83") /* Linux data */
       || TYPE_IS("0x8e") /* Linux LVM */
         )
        {
          datapartitions++;
        }
    }

  if (datapartitions > 1)
    return TRUE;

  return FALSE;
}

static UDisksDrive *
gis_disktarget_page_get_root_drive (UDisksClient *client)
{
  GDBusObjectManager *manager = udisks_client_get_object_manager(client);
  GList *objects = g_dbus_object_manager_get_objects(manager);
  GList *l;
  UDisksDrive *drive = NULL;

  for (l = objects; drive == NULL && l != NULL; l = l->next)
    {
      UDisksObject *object = UDISKS_OBJECT (l->data);
      UDisksFilesystem *fs = udisks_object_peek_filesystem (object);
      UDisksBlock *block = udisks_object_peek_block (object);
      const gchar *const* mounts = NULL;

      if (fs == NULL || block == NULL)
        {
          continue;
        }

      mounts = udisks_filesystem_get_mount_points (fs);
      if (mounts == NULL || !g_strv_contains (mounts, "/"))
        {
          continue;
        }

      g_print ("found root filesystem %s\n",
               g_dbus_object_get_object_path (G_DBUS_OBJECT (object)));
      drive = udisks_client_get_drive_for_block (client, block);
      if (drive == NULL)
        g_warning ("Couldn't get UDisksDrive for block");
    }

  return drive;
}

static void
gis_disktarget_page_populate_model(GisPage *page, UDisksClient *client)
{
  GList *l;
  GisDiskTargetPage *disktarget = GIS_DISK_TARGET_PAGE (page);
  GisDiskTargetPagePrivate *priv = gis_disktarget_page_get_instance_private (disktarget);
  GDBusObjectManager *manager = udisks_client_get_object_manager(client);
  GList *objects = g_dbus_object_manager_get_objects(manager);
  GtkListStore *store = OBJ(GtkListStore*, "target_store");
  GtkTreeIter i;
  gchar *umodel = NULL;
  UDisksDrive *root = NULL;
  UDisksDrive *image_drive = UDISKS_DRIVE (gis_store_get_object (GIS_STORE_IMAGE_DRIVE));
  const gchar *image_drive_path = NULL;
  GisImage *image = gis_store_get_selected_image ();

  if (image_drive != NULL)
    image_drive_path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (image_drive));

  if (gis_store_is_unattended())
    {
      GKeyFile *keys = gis_store_get_key_file();
      if (keys != NULL)
        {
          umodel = g_key_file_get_string (keys, "Unattended", "model", NULL);
        }
    }

  priv->has_valid_disks = FALSE;
  gtk_list_store_clear(store);
  root = gis_disktarget_page_get_root_drive (client);
  for (l = objects; l != NULL; l = l->next)
    {
      gchar *targetname, *targetsize;
      UDisksObject *object = UDISKS_OBJECT(l->data);
      const gchar *object_path;
      UDisksDrive *drive = udisks_object_peek_drive(object);
      UDisksBlock *block;
      gboolean has_data_partitions;
      if (drive == NULL)
        continue;

      object_path = g_dbus_object_get_object_path (G_DBUS_OBJECT (object));
      g_print ("considering drive %s\n", object_path);

#define skip_if(cond, reason, ...) \
      if (cond) \
        { \
          g_print ("skipping %s: " reason "\n", object_path, ##__VA_ARGS__); \
          continue; \
        }

      skip_if (drive == root, "it is the root device");

      if (gis_store_is_unattended() && umodel != NULL)
        {
          const gchar *model = udisks_drive_get_model (drive);
          skip_if (!g_str_equal (umodel, model),
                   "its model '%s' does not match '%s'", model, umodel);
        }

      skip_if (udisks_drive_get_optical (drive), "optical");
      skip_if (udisks_drive_get_ejectable (drive), "ejectable");

      block = udisks_client_get_block_for_drive(client, drive, TRUE);
      skip_if (block == NULL, "no corresponding block object");

      skip_if (0 == g_strcmp0 (object_path, image_drive_path),
               "it hosts the image partition");
#undef skip_if

      if (udisks_drive_get_size(drive) >= image->uncompressed_size)
        {
          priv->has_valid_disks = TRUE;
        }

      has_data_partitions = gis_disktarget_page_has_data_partitions(page, objects, block);

      targetname = g_strdup_printf("%s %s",
                                   udisks_drive_get_vendor(drive),
                                   udisks_drive_get_model(drive));
      targetsize = g_format_size_full (udisks_drive_get_size(drive),
                                       G_FORMAT_SIZE_DEFAULT);
      gtk_list_store_append(store, &i);
      gtk_list_store_set(store, &i, 0, targetname, 1, targetsize,
                                    2, G_OBJECT(block), 3, has_data_partitions, -1);
      g_free(targetname);
      g_free(targetsize);
    }
  g_clear_object (&root);

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &i))
    {
      GtkComboBox *combo = OBJ (GtkComboBox*, "diskcombo");
      gtk_combo_box_set_active_iter (combo, &i);
    }
  else
    {
      gtk_widget_hide (WID ("confirm_box"));
      gtk_widget_hide (WID ("partitionbutton"));
      gtk_widget_hide (WID ("too_small_box"));
      gtk_widget_show (WID ("error_box"));
    }

  gtk_widget_set_visible (WID ("suitable_disks_box"), !priv->has_valid_disks);
  if (!priv->has_valid_disks)
    {
      GisAssistant *assistant = gis_driver_get_assistant (page->driver);
      GList *pages = g_list_last (gis_assistant_get_all_pages (assistant));
      const gchar *text = gtk_label_get_text (OBJ (GtkLabel*, "suitable_disks_label"));
      GError *error = g_error_new_literal (GIS_DISK_ERROR, 0, text);

      pages = g_list_remove (pages, pages->prev->data);
      gis_page_set_forward_text (page, _("Finish"));
      gis_assistant_locale_changed (assistant);
      gis_store_set_error (error);
      g_clear_error (&error);
      gis_page_set_complete (GIS_PAGE (page), TRUE);

      if (gis_store_is_unattended())
        gis_assistant_next_page (gis_driver_get_assistant (page->driver));
    }
}

static void
gis_disktarget_page_shown (GisPage *page)
{
  GisDiskTargetPage *disktarget = GIS_DISK_TARGET_PAGE (page);
  GisDiskTargetPagePrivate *priv = gis_disktarget_page_get_instance_private (disktarget);

  if (gis_store_get_error() != NULL)
    {
      gis_assistant_next_page (gis_driver_get_assistant (page->driver));
      return;
    }

  priv->client = UDISKS_CLIENT (gis_store_get_object (GIS_STORE_UDISKS_CLIENT));
  g_assert (priv->client != NULL);
  gis_disktarget_page_populate_model(page, priv->client);
}

static void
gis_disktarget_page_constructed (GObject *object)
{
  GisDiskTargetPage *page = GIS_DISK_TARGET_PAGE (object);

  G_OBJECT_CLASS (gis_disktarget_page_parent_class)->constructed (object);

  gtk_container_add (GTK_CONTAINER (page), WID ("disktarget-page"));

  gis_page_set_complete (GIS_PAGE (page), FALSE);

  gtk_widget_show (GTK_WIDGET (page));

  g_signal_connect(OBJ(GObject*, "diskcombo"),
                       "changed", G_CALLBACK(gis_disktarget_page_selection_changed),
                       page);

  g_signal_connect(OBJ(GObject*, "confirmbutton"),
                       "toggled", G_CALLBACK(gis_disktarget_page_confirm_toggled),
                       page);
  g_signal_connect(OBJ(GObject*, "partitionbutton"),
                       "toggled", G_CALLBACK(gis_disktarget_page_confirm_toggled),
                       page);

}

static void
gis_disktarget_page_locale_changed (GisPage *page)
{
  gis_page_set_title (page, _("Select Disk"));
}

static void
gis_disktarget_page_class_init (GisDiskTargetPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_disktarget_page_locale_changed;
  page_class->shown = gis_disktarget_page_shown;
  object_class->constructed = gis_disktarget_page_constructed;
}

static void
gis_disktarget_page_init (GisDiskTargetPage *page)
{
  g_resources_register (disktarget_get_resource ());
}

void
gis_prepare_disktarget_page (GisDriver *driver)
{
  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_DISK_TARGET_PAGE,
                                     "driver", driver,
                                     NULL));
}
