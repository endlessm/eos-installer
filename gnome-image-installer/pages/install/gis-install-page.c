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

/* Install page {{{1 */

#define PAGE_ID "install"

#include "config.h"
#include "install-resources.h"
#include "gis-install-page.h"
#include "gis-store.h"

#include <udisks/udisks.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "gduxzdecompressor.h"

struct _GisInstallPagePrivate {
  GMutex copy_mutex;
  GFile *image;
  GObject *decompressor;
  GInputStream *decompressed;
  gint64 decompressed_size;
  gint drive_fd;
  gint64 bytes_written;
  GThread *copythread;
  GPid gpg;
  guint pulse_id;
};
typedef struct _GisInstallPagePrivate GisInstallPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisInstallPage, gis_install_page, GIS_TYPE_PAGE);

G_DEFINE_QUARK(install-error, gis_install_error);
#define GIS_INSTALL_ERROR gis_install_error_quark()

#define OBJ(type,name) ((type)gtk_builder_get_object(GIS_PAGE(page)->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)

#define IMAGE_KEYRING "/usr/share/keyrings/eos-image-keyring.gpg"

static gboolean
gis_install_page_prepare_read (GisPage *page, GError **error)
{
  GisInstallPage *install = GIS_INSTALL_PAGE (page);
  GisInstallPagePrivate *priv = gis_install_page_get_instance_private (install);
  GInputStream *input;
  gchar *basename = NULL;
  GError *e = NULL;

  priv->decompressed_size = gis_store_get_required_size();
  priv->image = G_FILE(gis_store_get_object(GIS_STORE_IMAGE));
  g_object_ref (priv->image);
  basename = g_file_get_basename(priv->image);
  if (basename == NULL)
    {
      *error = g_error_new(GIS_INSTALL_ERROR, 0, _("Image verification error."));
      return FALSE;
    }

  /* TODO: use more magical means */
  if (g_str_has_suffix (basename, "gz"))
    {
      priv->decompressor = G_OBJECT (g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP));
    }
  else if (g_str_has_suffix (basename, "xz"))
    {
      priv->decompressor = G_OBJECT (gdu_xz_decompressor_new ());
    }
  else
    {
      *error = g_error_new(GIS_INSTALL_ERROR, 0, _("Image verification error."));
      g_free (basename);
      g_object_unref (input);
      return FALSE;
    }
  g_free (basename);

  input = (GInputStream *) g_file_read (priv->image, NULL, &e);
  if (e != NULL)
    {
      g_propagate_error (error, e);
      return FALSE;
    }
  priv->decompressed = g_converter_input_stream_new (G_INPUT_STREAM (input),
                                                     G_CONVERTER (priv->decompressor));
  g_object_unref (input);

  return TRUE;
}

static gboolean
gis_install_page_prepare_write (GisPage *page, GError **error)
{
  GisInstallPage *install = GIS_INSTALL_PAGE (page);
  GisInstallPagePrivate *priv = gis_install_page_get_instance_private (install);
  GError *e = NULL;
  GUnixFDList *fd_list = NULL;
  GVariant *fd_index = NULL;
  gint fd = -1;
  UDisksBlock *block = UDISKS_BLOCK(gis_store_get_object(GIS_STORE_BLOCK_DEVICE));

  if (block == NULL)
    {
      *error = g_error_new(GIS_INSTALL_ERROR, 0, _("Image verification error."));
      return FALSE;
    }

  if (!udisks_block_call_open_for_restore_sync (block,
                                                g_variant_new ("a{sv}", NULL), /* options */
                                                NULL, /* fd_list */
                                                &fd_index,
                                                &fd_list,
                                                NULL, /* cancellable */
                                                &e))
    {
      g_propagate_error (error, e);
      return FALSE;
    }

  fd = g_unix_fd_list_get (fd_list, g_variant_get_handle (fd_index), &e);
  if (fd < 0)
    {
      g_prefix_error (&e,
                      "Error extracing fd with handle %d from D-Bus message: ",
                      g_variant_get_handle (fd_index));
      g_propagate_error (error, e);
      return FALSE;
    }

  if (fd_index != NULL)
    g_variant_unref (fd_index);

  g_clear_object (&fd_list);

  priv->drive_fd = fd;

  return TRUE;
}

static gboolean
gis_install_page_teardown (GisPage *page)
{
  GisInstallPage *install = GIS_INSTALL_PAGE (page);
  GisInstallPagePrivate *priv = gis_install_page_get_instance_private (install);

  g_mutex_lock (&priv->copy_mutex);

  if (priv->image != NULL)
    g_object_unref (priv->image);
  priv->image = NULL;

  if (priv->decompressor != NULL)
    g_object_unref (priv->decompressor);
  priv->decompressor = NULL;

  if (priv->decompressed != NULL)
    g_object_unref (priv->decompressed);
  priv->decompressed = NULL;

  if (priv->drive_fd)
    {
      syncfs(priv->drive_fd);
      close(priv->drive_fd);
    }
  priv->drive_fd = -1;
  priv->bytes_written = 0;

  g_mutex_unlock (&priv->copy_mutex);

  gtk_progress_bar_set_fraction (OBJ (GtkProgressBar*, "install_progress"), 1.0);

  gis_assistant_next_page (gis_driver_get_assistant (page->driver));

  return FALSE;
}

static gboolean
gis_install_page_update_progress(GisPage *page)
{
  GisInstallPage *install = GIS_INSTALL_PAGE (page);
  GisInstallPagePrivate *priv = gis_install_page_get_instance_private (install);
  GtkProgressBar *bar = OBJ (GtkProgressBar*, "install_progress");

  g_mutex_lock (&priv->copy_mutex);

  gtk_progress_bar_set_fraction (bar, (gdouble)priv->bytes_written/(gdouble)priv->decompressed_size);

  g_mutex_unlock (&priv->copy_mutex);

  return TRUE;
}

static gpointer
gis_install_page_copy (GisPage *page)
{
  GisInstallPage *install = GIS_INSTALL_PAGE (page);
  GisInstallPagePrivate *priv = gis_install_page_get_instance_private (install);
  GError *error = NULL;
  gchar *buffer = NULL;
  gssize buffer_size = 1 * 1024 * 1024;
  gssize r, w = -1;
  guint timer_id;

  buffer = (gchar*) g_malloc0 (buffer_size);
  priv->bytes_written = 0;
  timer_id = g_timeout_add (1000, (GSourceFunc)gis_install_page_update_progress, page);

  g_thread_yield();

  do
    {
      r = g_input_stream_read (priv->decompressed,
                               buffer, buffer_size,
                               NULL, &error);

      if (r < 0 || error != NULL)
        {
          gis_store_set_error (error);
          g_error_free (error);
        }

      /* We lock to protect the bytes_written. It's probably dangerous to
       * assume the lock protects from anything else...
       */
      g_mutex_lock (&priv->copy_mutex);
      if (r > 0)
        {
          w = write (priv->drive_fd, buffer, r);
          priv->bytes_written += w;
        }

      g_mutex_unlock (&priv->copy_mutex);
    }
  while (r > 0 && w > 0);

  g_idle_add ((GSourceFunc)gis_install_page_teardown, page);

  g_source_remove (timer_id);

  return NULL;
}


static gboolean
gis_install_page_prepare (GisPage *page)
{
  GError *error = NULL;
  GisInstallPage *install = GIS_INSTALL_PAGE (page);
  GisInstallPagePrivate *priv = gis_install_page_get_instance_private (install);
  gchar *msg = g_strdup_printf (_("Step %d of %d"), 2, 2);
  gtk_label_set_text (OBJ (GtkLabel*, "install_label"), msg);
  g_free (msg);

  g_mutex_init (&priv->copy_mutex);

  g_source_remove (priv->pulse_id);

  if (!gis_install_page_prepare_read (page, &error))
    {
      gis_store_set_error (error);
      g_error_free (error);
      gis_install_page_teardown(page);
      return FALSE;
    }

  if (!gis_install_page_prepare_write (page, &error))
    {
      gis_store_set_error (error);
      g_error_free (error);
      gis_install_page_teardown(page);
      return FALSE;
    }

  priv->copythread = g_thread_new ("image-copy-thread",
                                   (GThreadFunc)gis_install_page_copy, page);

  return FALSE;
}

static void
gis_install_page_gpg_watch (GPid pid, gint status, GisPage *page)
{
  GError *error = NULL;

  if (!g_spawn_check_exit_status (status, NULL))
  {
    error = g_error_new(GIS_INSTALL_ERROR, status, _("Image verification error."));
    gis_store_set_error (error);
    g_error_free (error);
    gis_install_page_teardown(page);
    return;
  }

  gtk_progress_bar_set_fraction (OBJ (GtkProgressBar*, "install_progress"), 0.0);
  g_spawn_close_pid (pid);
  g_idle_add ((GSourceFunc)gis_install_page_prepare, page);
}

static gboolean
gis_install_page_pulse (GisPage *page)
{
  gtk_progress_bar_pulse (OBJ (GtkProgressBar*, "install_progress"));
  return TRUE;
}

static gboolean
gis_install_page_verify (GisPage *page)
{
  GisInstallPage *install = GIS_INSTALL_PAGE (page);
  GisInstallPagePrivate *priv = gis_install_page_get_instance_private (install);
  GFile *image = G_FILE(gis_store_get_object(GIS_STORE_IMAGE));
  gchar *args[7] = { "gpg", "--keyring", IMAGE_KEYRING, "--verify", "", "", NULL };
  GError *error = NULL;

  args[5] = g_file_get_path (image);
  args[4] = g_strjoin(NULL, args[5], ".asc", NULL);

  if (!g_spawn_async (NULL, args, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                      NULL, NULL, &priv->gpg, NULL))
    {
      error = g_error_new(GIS_INSTALL_ERROR, 0, _("Image verification error."));
      gis_store_set_error (error);
      g_error_free (error);
      gis_install_page_teardown(page);
      g_free (args[5]);
      g_free (args[4]);
      return FALSE;
    }

  gtk_progress_bar_pulse (OBJ (GtkProgressBar*, "install_progress"));

  g_child_watch_add (priv->gpg, (GChildWatchFunc)gis_install_page_gpg_watch, page);
  priv->pulse_id = g_timeout_add (500, (GSourceFunc)gis_install_page_pulse, page);

  g_free (args[5]);
  g_free (args[4]);
  return FALSE;
}

static void
gis_install_page_shown (GisPage *page)
{
  GisInstallPage *install = GIS_INSTALL_PAGE (page);
  GisInstallPagePrivate *priv = gis_install_page_get_instance_private (install);
  gchar *msg = g_strdup_printf (_("Step %d of %d"), 1, 2);
  gtk_label_set_text (OBJ (GtkLabel*, "install_label"), msg);
  g_free (msg);

  gis_driver_save_data (GIS_PAGE (page)->driver);

  if (gis_store_get_error() == NULL)
    g_idle_add ((GSourceFunc)gis_install_page_verify, page);
  else
    gis_assistant_next_page (gis_driver_get_assistant (page->driver));

}

static void
gis_install_page_constructed (GObject *object)
{
  GisInstallPage *page = GIS_INSTALL_PAGE (object);

  G_OBJECT_CLASS (gis_install_page_parent_class)->constructed (object);

  gtk_container_add (GTK_CONTAINER (page), WID ("install-page"));

  gtk_progress_bar_pulse (OBJ (GtkProgressBar*, "install_progress"));

  // XXX: FOR DEBUGGING, hide the buttons in final
  gis_page_set_complete (GIS_PAGE (page), FALSE);

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_install_page_locale_changed (GisPage *page)
{
  gis_page_set_title (page, _("Installing"));
}

static void
gis_install_page_class_init (GisInstallPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_install_page_locale_changed;
  page_class->shown = gis_install_page_shown;
  object_class->constructed = gis_install_page_constructed;
}

static void
gis_install_page_init (GisInstallPage *page)
{
  g_resources_register (install_get_resource ());
}

void
gis_prepare_install_page (GisDriver *driver)
{
  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_INSTALL_PAGE,
                                     "driver", driver,
                                     NULL));
}
