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

struct _GisDiskTargetPagePrivate {
  UDisksClient *client;
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
  UDisksBlock *block = UDISKS_BLOCK (gis_store_get_object (GIS_STORE_BLOCK_DEVICE));
  UDisksDrive *drive = NULL;

  gis_page_set_complete (GIS_PAGE (page), FALSE);

  if (block == NULL)
    return;

  drive = udisks_client_get_drive_for_block (priv->client, UDISKS_BLOCK(block));
  if (drive == NULL)
    return;

  if (udisks_drive_get_size(drive) < gis_store_get_required_size())
    return;

  if (!gtk_toggle_button_get_active (button))
    return;

  gis_page_set_complete (GIS_PAGE (page), TRUE);
}

static void
gis_disktarget_page_confirm_toggled(GtkToggleButton *button, GisDiskTargetPage *page)
{
  check_can_continue(page);
}

static void
gis_disktarget_page_selection_changed(GtkWidget *combo, GisDiskTargetPage *page)
{
  GtkTreeIter i;
  GisDiskTargetPage *disktarget = GIS_DISK_TARGET_PAGE (page);
  GisDiskTargetPagePrivate *priv = gis_disktarget_page_get_instance_private (disktarget);
  gchar *disk, *size = NULL;
  GObject *block = NULL;
  GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
  GtkLabel *disk_label = OBJ(GtkLabel*, "disk_label");
  GtkLabel *size_label = OBJ(GtkLabel*, "size_label");

  gis_page_set_complete (GIS_PAGE (page), FALSE);

  if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &i))
    {
      gtk_label_set_text(disk_label, "");
      gtk_label_set_text(size_label, "");
      return;
    }

  gtk_tree_model_get(model, &i, 0, &disk, 1, &size, 2, &block, -1);

  if (disk != NULL)
    gtk_label_set_text(disk_label, disk);
  else
    gtk_label_set_text(disk_label, "");

  if (size != NULL)
    gtk_label_set_text(size_label, size);
  else
    gtk_label_set_text(size_label, "");

  if (block != NULL)
    {
      UDisksDrive *drive = udisks_client_get_drive_for_block (priv->client, UDISKS_BLOCK(block));
      gis_store_set_object (GIS_STORE_BLOCK_DEVICE, block);
      g_object_unref(block);
      if (udisks_drive_get_size(drive) < gis_store_get_required_size())
        {
          gtk_label_set_text(disk_label, "The selected image is too large for this disk!");
          gtk_label_set_text(size_label, "");
          return;
        }
    }

  check_can_continue(page);
}

static void
gis_disktarget_page_populate_model(GisPage *page, UDisksClient *client)
{
  GList *l;
  GDBusObjectManager *manager = udisks_client_get_object_manager(client);
  GList *objects = g_dbus_object_manager_get_objects(manager);
  GtkListStore *store = OBJ(GtkListStore*, "target_store");
  GtkTreeIter i;

  gtk_list_store_clear(store);
  for (l = objects; l != NULL; l = l->next)
    {
      gchar *targetname, *targetsize;
      UDisksObject *object = UDISKS_OBJECT(l->data);
      UDisksDrive *drive = udisks_object_peek_drive(object);
      UDisksBlock *block;

      if (drive == NULL)
        continue;

      if (udisks_drive_get_optical(drive))
        continue;

      if (udisks_drive_get_removable(drive))
        continue;

      if (udisks_drive_get_ejectable(drive))
        continue;

      block = udisks_client_get_block_for_drive(client, drive, TRUE);
      if (block == NULL)
        continue;

      /*
      printf("%soptical\n", udisks_drive_get_optical(drive) ? "is  ": "not ");
      printf("%sremovable\n", udisks_drive_get_media_removable(drive) ? "is  ": "not ");
      printf("%sejectable\n", udisks_drive_get_ejectable(drive) ? "is  ": "not ");
      */

      targetname = g_strdup_printf("%s %s",
                                   udisks_drive_get_vendor(drive),
                                   udisks_drive_get_model(drive));
      targetsize = g_strdup_printf("%.02f GB",
                                   udisks_drive_get_size(drive)/1024.0/1024.0/1024.0);
      gtk_list_store_append(store, &i);
      gtk_list_store_set(store, &i, 0, targetname, 1, targetsize, 2, G_OBJECT(block), -1);
      g_free(targetname);
      g_free(targetsize);
    }

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &i))
    {
      GtkComboBox *combo = OBJ (GtkComboBox*, "diskcombo");
      gtk_combo_box_set_active_iter (combo, &i);
    }

}

static void
gis_disktarget_page_shown (GisPage *page)
{
  GError *error;
  GisDiskTargetPage *disktarget = GIS_DISK_TARGET_PAGE (page);
  GisDiskTargetPagePrivate *priv = gis_disktarget_page_get_instance_private (disktarget);

  gis_driver_save_data (GIS_PAGE (page)->driver);

  priv->client = udisks_client_new_sync(NULL, &error);
  if (priv->client == NULL)
    {
      g_error("Unable to enumearate disks: %s", error->message);
      g_error_free(error);
    }
  else
    {
      gis_disktarget_page_populate_model(page, priv->client);
    }
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
