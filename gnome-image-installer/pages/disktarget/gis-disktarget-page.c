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
#define GIS_DISK_ERROR gis_disk_error_quark()

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
  GObject *block = NULL;
  GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));

  gis_page_set_complete (GIS_PAGE (page), FALSE);

  if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &i))
    {
      gtk_widget_hide (WID ("confirm_box"));
      gtk_widget_show (WID ("error_box"));
      return;
    }

  gtk_tree_model_get(model, &i, 2, &block, -1);

  if (block != NULL)
    {
      UDisksDrive *drive = udisks_client_get_drive_for_block (priv->client, UDISKS_BLOCK(block));
      gis_store_set_object (GIS_STORE_BLOCK_DEVICE, block);
      g_object_unref(block);
      if (udisks_drive_get_size(drive) < gis_store_get_required_size())
        {
          gchar *msg = g_strdup_printf (
            _("The location you have chosen is too small - you need more space to install %s (%.02f GB)"),
            gis_store_get_image_name(), gis_store_get_required_size()/1024.0/1024.0/1024.0);
          gtk_label_set_text (OBJ (GtkLabel*, "too_small_label"), msg);
          g_free (msg);
          gtk_widget_hide (WID ("confirm_box"));
          gtk_widget_show (WID ("error_box"));
          return;
        }
    }

  gtk_widget_show (WID ("confirm_box"));
  gtk_widget_hide (WID ("error_box"));
  check_can_continue(page);
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

  priv->has_valid_disks = FALSE;
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

      if (udisks_drive_get_size(drive) >= gis_store_get_required_size())
        {
          priv->has_valid_disks = TRUE;
        }

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

  if (priv->has_valid_disks)
    {
      gtk_widget_set_visible (WID ("suitable_disks_label"), FALSE);
    }
  else
    {
      GisAssistant *assistant = gis_driver_get_assistant (page->driver);
      GList *pages = g_list_last (gis_assistant_get_all_pages (assistant));
      const gchar *text = gtk_label_get_text (OBJ (GtkLabel*, "suitable_disks_label"));
      GError *error = g_error_new_literal (GIS_DISK_ERROR, 0, text);

      pages = g_list_remove (pages, pages->prev->data);
      gtk_widget_set_visible (WID ("suitable_disks_label"), TRUE);
      gis_page_set_forward_text (page, _("Finish"));
      gis_assistant_locale_changed (assistant);
      gis_store_set_error (error);
      g_clear_error (&error);
      gis_page_set_complete (GIS_PAGE (page), TRUE);
    }
}

static void
gis_disktarget_page_shown (GisPage *page)
{
  GError *error;
  GisDiskTargetPage *disktarget = GIS_DISK_TARGET_PAGE (page);
  GisDiskTargetPagePrivate *priv = gis_disktarget_page_get_instance_private (disktarget);

  gis_driver_save_data (GIS_PAGE (page)->driver);

  if (gis_store_get_error() != NULL)
    {
      gis_assistant_next_page (gis_driver_get_assistant (page->driver));
      return;
    }

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
