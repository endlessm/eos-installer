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
#include <gio/gunixfdlist.h>
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
  GIOChannel *gpgout;
  guint gpg_watch;
  UDisksClient *client;
  guint pulse_id;

  GtkWidget *warning_dialog;
  gboolean   writing;
};
typedef struct _GisInstallPagePrivate GisInstallPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisInstallPage, gis_install_page, GIS_TYPE_PAGE);

G_DEFINE_QUARK(install-error, gis_install_error);

#define OBJ(type,name) ((type)gtk_builder_get_object(GIS_PAGE(page)->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)

#define IMAGE_KEYRING "/usr/share/keyrings/eos-image-keyring.gpg"

static gboolean
delete_event_cb (GtkWidget      *toplevel,
                 GdkEvent       *event,
                 GisInstallPage *self)
{
  GisInstallPagePrivate *priv = gis_install_page_get_instance_private (self);
  GtkWidget *button;
  gboolean should_propagate;
  gint response_id;

  /* If we're not writing, it's still safe to quit */
  if (!priv->writing)
    return GDK_EVENT_PROPAGATE;

  priv->warning_dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
                                                 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                 GTK_MESSAGE_WARNING,
                                                 GTK_BUTTONS_NONE,
                                                 _("Stop reformatting the disk?"));

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (priv->warning_dialog),
                                            _("The reformatting process has already begun. Cancelling "
                                              "now will leave this system unbootable."));

  /* Stop Reformatting */
  button = gtk_button_new_with_label (_("Stop Reformatting"));
  gtk_style_context_add_class (gtk_widget_get_style_context (button), "destructive-action");
  gtk_dialog_add_action_widget (GTK_DIALOG (priv->warning_dialog), button, GTK_RESPONSE_OK);

  gtk_widget_show (button);

  /* Continue Reformatting */
  button = gtk_button_new_with_label (_("Continue Reformatting"));
  gtk_dialog_add_action_widget (GTK_DIALOG (priv->warning_dialog), button, GTK_RESPONSE_CANCEL);

  gtk_widget_set_can_default (button, TRUE);
  gtk_widget_show (button);

  /* Let's avoid accidents */
  gtk_dialog_set_default_response (GTK_DIALOG (priv->warning_dialog), GTK_RESPONSE_CANCEL);
  gtk_widget_grab_focus (button);

  /* Run the dialog */
  response_id = gtk_dialog_run (GTK_DIALOG (priv->warning_dialog));
  gtk_widget_destroy (priv->warning_dialog);
  priv->warning_dialog = NULL;

  should_propagate = response_id == GTK_RESPONSE_OK ? GDK_EVENT_PROPAGATE : GDK_EVENT_STOP;

  return should_propagate;
}

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
      gchar *parse_name = g_file_get_parse_name(priv->image);
      g_warning ("g_file_get_basename(\"%s\") returned NULL", parse_name);
      g_free (parse_name);
      *error = g_error_new (GIS_INSTALL_ERROR, 0, _("Internal error"));
      g_return_val_if_reached (FALSE);
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
  else if (g_str_has_suffix (basename, "img") || g_strcmp0 (basename, "endless-image") == 0)
    {
      priv->decompressor = NULL;
    }
  else
    {
      g_warning ("%s ends in neither '.xz', '.gz' nor '.img'", basename);
      *error = g_error_new(GIS_INSTALL_ERROR, 0, _("Internal error"));
      g_free (basename);
      g_return_val_if_reached (FALSE);
    }
  g_free (basename);

  input = (GInputStream *) g_file_read (priv->image, NULL, &e);
  if (e != NULL)
    {
      g_propagate_error (error, e);
      return FALSE;
    }

  if (priv->decompressor)
    {
      priv->decompressed = g_converter_input_stream_new (G_INPUT_STREAM (input),
                                                         G_CONVERTER (priv->decompressor));
      g_object_unref (input);
    }
  else
    {
      priv->decompressed = input;
    }

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
      g_warning ("gis_store_get_object(GIS_STORE_BLOCK_DEVICE) == NULL");
      *error = g_error_new(GIS_INSTALL_ERROR, 0, _("Internal error"));
      g_return_val_if_reached (FALSE);
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
                      "Error extracting fd with handle %d from D-Bus message: ",
                      g_variant_get_handle (fd_index));
      g_propagate_error (error, e);
      return FALSE;
    }

  if (fd_index != NULL)
    g_variant_unref (fd_index);

  g_clear_object (&fd_list);

  g_mutex_lock (&priv->copy_mutex);
  priv->drive_fd = fd;
  g_mutex_unlock (&priv->copy_mutex);

  return TRUE;
}

static void
gis_install_page_unmount_image_partition (GisPage *page)
{
  GisInstallPage *install = GIS_INSTALL_PAGE (page);
  GisInstallPagePrivate *priv = gis_install_page_get_instance_private (install);
  GDBusObjectManager *manager = udisks_client_get_object_manager(priv->client);
  GList *objects = g_dbus_object_manager_get_objects(manager);
  GList *l;
  const gchar *label;

  if (gis_store_is_live_install ())
    label = "eoslive";
  else
    label = "eosimages";

  for (l = objects; l != NULL; l = l->next)
    {
      UDisksFilesystem *fs;
      UDisksBlock *block = udisks_object_peek_block (UDISKS_OBJECT (l->data));

      if (block == NULL)
        continue;

      if (!g_str_equal (label, udisks_block_get_id_label (block)))
        continue;

      fs = udisks_object_peek_filesystem (UDISKS_OBJECT (l->data));
      udisks_filesystem_call_unmount_sync (fs, g_variant_new ("a{sv}", NULL), NULL, NULL);
    }
}

static void
gis_install_page_close_drive (GisPage *page)
{
  GisInstallPage *install = GIS_INSTALL_PAGE (page);
  GisInstallPagePrivate *priv = gis_install_page_get_instance_private (install);

  g_mutex_lock (&priv->copy_mutex);

  if (priv->drive_fd)
    {
      syncfs (priv->drive_fd);
      close (priv->drive_fd);
      g_spawn_command_line_sync ("partprobe", NULL, NULL, NULL, NULL);
    }

  priv->drive_fd = -1;

  g_mutex_unlock (&priv->copy_mutex);
}

static void
gis_install_page_stop_pulsing (GisInstallPage *page)
{
  GisInstallPagePrivate *priv = gis_install_page_get_instance_private (page);
  GtkProgressBar *progress = OBJ (GtkProgressBar*, "install_progress");

  if (priv->pulse_id)
    {
      gtk_widget_remove_tick_callback ((GtkWidget *) progress, priv->pulse_id);
      priv->pulse_id = 0;
    }
}

static gboolean
gis_install_page_teardown (GisPage *page)
{
  GisInstallPage *install = GIS_INSTALL_PAGE (page);
  GisInstallPagePrivate *priv = gis_install_page_get_instance_private (install);
  GtkProgressBar *progress = OBJ (GtkProgressBar*, "install_progress");

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

  priv->bytes_written = 0;

  gis_install_page_stop_pulsing (install);

  g_mutex_unlock (&priv->copy_mutex);

  gis_install_page_close_drive (page);

  gis_install_page_unmount_image_partition (page);

  gtk_progress_bar_set_fraction (progress, 1.0);

  /*
   * If there's a message dialog asking whether the user wants to quit, and
   * we finished writing, hide and ignore that dialog.
   */
  if (priv->warning_dialog)
    {
      gtk_dialog_response (GTK_DIALOG (priv->warning_dialog), GTK_RESPONSE_CANCEL);
      gtk_widget_destroy (priv->warning_dialog);
      priv->warning_dialog = NULL;
    }

  g_signal_handlers_disconnect_by_func (gtk_widget_get_toplevel (GTK_WIDGET (page)),
                                        delete_event_cb,
                                        page);

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

static gboolean
gis_install_page_pulse_progress (GtkProgressBar *bar)
{
  gtk_progress_bar_pulse (bar);

  return TRUE;
}

static void
gis_install_page_ensure_pulsing (GisInstallPage *page,
                                 GtkProgressBar *progress)
{
  GisInstallPagePrivate *priv = gis_install_page_get_instance_private (page);

  if (priv->pulse_id == 0)
    {
      gtk_progress_bar_set_pulse_step (progress, 1. / 60.);
      priv->pulse_id = gtk_widget_add_tick_callback (
          GTK_WIDGET (progress),
          (GtkTickCallback) gis_install_page_pulse_progress,
          NULL, NULL);
    }
}

static gboolean
gis_install_page_is_efi_system (GisPage *page)
{
  return g_file_test ("/sys/firmware/efi", G_FILE_TEST_IS_DIR);
}

static gboolean
gis_install_page_convert_to_mbr (GisPage *page, GError **error)
{
  UDisksBlock *block = UDISKS_BLOCK (gis_store_get_object (GIS_STORE_BLOCK_DEVICE));
  static const char *cmd = "/usr/sbin/eos-repartition-mbr";
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) process = NULL;
  g_autofree gchar *rootdev = NULL;
  gint ret = 0;
  GError *err = NULL;

  if (block == NULL)
    {
      g_printerr ("reached convert_to_mbr without a block device!\n");
      g_set_error (error, GIS_INSTALL_ERROR, 0, _("Internal error"));
      return FALSE;
    }

  rootdev = g_strdup (udisks_block_get_device (block));
  g_print("launching %s %s via pkexec\n", cmd, rootdev);

  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_NONE);
  /* pkexec won't let us run the program if $SHELL isn't in /etc/shells,
   * so remove it from the environment.
   */
  g_subprocess_launcher_unsetenv (launcher, "SHELL");
  process = g_subprocess_launcher_spawn (launcher, &err,
                                         "pkexec", cmd, rootdev, NULL);
  if (process == NULL ||
      !g_subprocess_wait (process, NULL, &err))
    {
      g_printerr ("failed to run %s: %s\n", cmd, err->message);
      g_propagate_error (error, err);
      return FALSE;
    }

  ret = g_subprocess_get_exit_status (process);

  if (ret != 0)
    {
      g_printerr ("%s returned %i\n", cmd, ret);
      g_set_error (error, GIS_INSTALL_ERROR, 0, _("Internal error"));
      return FALSE;
    }

  g_print ("%s succeeded\n", cmd);

  return TRUE;
}

static gpointer
gis_install_page_copy (GisPage *page)
{
  GisInstallPage *install = GIS_INSTALL_PAGE (page);
  GisInstallPagePrivate *priv = gis_install_page_get_instance_private (install);
  GtkProgressBar *progress = OBJ (GtkProgressBar *, "install_progress");
  GError *error = NULL;
  gchar *buffer = NULL;
  gssize buffer_size = 1 * 1024 * 1024;
  gssize r, w = -1;
  guint timer_id;

  buffer = (gchar*) g_malloc0 (buffer_size);
  priv->bytes_written = 0;
  timer_id = g_timeout_add (1000, (GSourceFunc)gis_install_page_update_progress, page);

  g_thread_yield();

  priv->writing = TRUE;

  do
    {
      r = g_input_stream_read (priv->decompressed,
                               buffer, buffer_size,
                               NULL, &error);

      if (r > 0)
        {
          /* We lock to protect the bytes_written and access to drive_fd. */
          g_mutex_lock (&priv->copy_mutex);

          w = write (priv->drive_fd, buffer, r);
          priv->bytes_written += w;

          g_mutex_unlock (&priv->copy_mutex);
        }
    }
  while (r > 0 && w > 0);

  g_source_remove (timer_id);

  if (r < 0 || error != NULL)
    {
      gis_store_set_error (error);
      g_error_free (error);
      goto out;
    }

  /* set up a pulser and start the sync here, as it can be very slow *
   * protect the pulse_id with the lock                              */
  g_mutex_lock (&priv->copy_mutex);
  gis_install_page_ensure_pulsing (install, progress);
  g_mutex_unlock (&priv->copy_mutex);

  g_thread_yield();

  gis_install_page_close_drive (page);

  if (!gis_install_page_is_efi_system (page))
    {
      g_print ("BIOS boot detected, converting system from GPT to MBR\n");

      if (!gis_install_page_convert_to_mbr (page, &error))
        {
          gis_store_set_error (error);
          g_error_free (error);
        }
    }

  priv->writing = FALSE;

out:
  g_idle_add ((GSourceFunc)gis_install_page_teardown, page);

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

  g_source_remove (priv->gpg_watch);
  g_io_channel_shutdown (priv->gpgout, TRUE, NULL);
  g_io_channel_unref (priv->gpgout);
  priv->gpgout = NULL;

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
gis_install_page_verify_failed (GisPage *page,
                                GError  *error)
{
  if (error == NULL)
    {
      error = g_error_new (GIS_INSTALL_ERROR, 0, _("Image verification error."));
    }

  gis_store_set_error (error);
  g_error_free (error);
  gis_install_page_teardown(page);
}

static void
gis_install_page_gpg_watch (GPid pid, gint status, GisPage *page)
{
  if (!g_spawn_check_exit_status (status, NULL))
    {
      gis_install_page_verify_failed (page, NULL);
      return;
    }

  gtk_progress_bar_set_fraction (OBJ (GtkProgressBar*, "install_progress"), 0.0);
  g_spawn_close_pid (pid);
  g_idle_add ((GSourceFunc)gis_install_page_prepare, page);
}

static gboolean
gis_install_page_gpg_progress (GIOChannel *source, GIOCondition cond, GisPage *page)
{
  g_autofree gchar *line = NULL;

  if (g_io_channel_read_line (source, &line, NULL, NULL, NULL) == G_IO_STATUS_NORMAL)
  {
    if (g_str_has_prefix (line, "[GNUPG:] PROGRESS"))
      {
        /* https://git.gnupg.org/cgi-bin/gitweb.cgi?p=gnupg.git;a=blob;f=doc/DETAILS;h=0be55f4d;hb=refs/heads/master#l1043
         * [GNUPG:] PROGRESS <what> <char> <cur> <total> [<units>]
         * For example:
         * [GNUPG:] PROGRESS /dev/mapper/endless- ? 676 4442 MiB
         */
        GtkProgressBar *bar = OBJ (GtkProgressBar*, "install_progress");
        g_auto(GStrv) arr = g_strsplit (line, " ", -1);
        gdouble curr = g_ascii_strtod (arr[4], NULL);
        gdouble full = g_ascii_strtod (arr[5], NULL);

        if (full == 0)
          {
            /* gpg can't determine the file size. Should not happen, since we
             * gave it a hint.
             */
            gis_install_page_ensure_pulsing (GIS_INSTALL_PAGE (page), bar);
          }
        else
          {
            gdouble frac = curr/full;
            gtk_progress_bar_set_fraction (bar, frac);
          }
      }
  }

  return TRUE;
}

static gboolean
gis_install_page_verify (GisPage *page)
{
  GisInstallPage *install = GIS_INSTALL_PAGE (page);
  GisInstallPagePrivate *priv = gis_install_page_get_instance_private (install);
  GFile *image = G_FILE(gis_store_get_object (GIS_STORE_IMAGE));
  gchar *image_path = g_file_get_path (image);
  const gchar *signature_path = gis_store_get_image_signature ();
  GFile *signature = g_file_new_for_path (signature_path);
  gint outfd;
  guint64 size = gis_store_get_required_size ();
  g_autofree gchar *size_str = g_strdup_printf ("%" G_GUINT64_FORMAT, size);
  const gchar * const args[] = { "gpg",
                    "--enable-progress-filter", "--status-fd", "1",
                    /* Trust the one key in this keyring, and no others */
                    "--keyring", IMAGE_KEYRING,
                    "--no-default-keyring",
                    "--trust-model", "always",
                    "--input-size-hint", size_str,
                    "--verify", signature_path, image_path, NULL };
  GError *error = NULL;

  if (!g_file_query_exists (signature, NULL))
    {
      error = g_error_new (GIS_INSTALL_ERROR, 0,
          _("The Endless OS signature file \"%s\" does not exist."),
          signature_path);
      gis_install_page_verify_failed (page, error);
      goto out;
    }

  if (!g_spawn_async_with_pipes (NULL, (gchar **) args, NULL,
                                 G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                                 NULL, NULL, &priv->gpg,
                                 NULL, &outfd, NULL, &error))
    {
      gis_install_page_verify_failed (page, error);
      goto out;
    }

  priv->gpgout = g_io_channel_unix_new (outfd);
  priv->gpg_watch = g_io_add_watch (priv->gpgout, G_IO_IN, (GIOFunc)gis_install_page_gpg_progress, page);

  gtk_progress_bar_set_fraction (OBJ (GtkProgressBar*, "install_progress"), 0.0);

  g_child_watch_add (priv->gpg, (GChildWatchFunc)gis_install_page_gpg_watch, page);

out:
  g_free (image_path);
  g_object_unref (signature);
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

  priv->client = UDISKS_CLIENT (gis_store_get_object (GIS_STORE_UDISKS_CLIENT));

  if (gis_store_get_error () != NULL)
    {
      gis_assistant_next_page (gis_driver_get_assistant (page->driver));
      return;
    }

  g_idle_add ((GSourceFunc)gis_install_page_verify, page);

  /*
   * When the installer is in the middle of the copy operation, we have
   * to show a dialog asking the user if she ~really~ wants to quit the
   * application in the middle of a potentially dangerous operation.
   */
  g_signal_connect (gtk_widget_get_toplevel (GTK_WIDGET (page)),
                    "delete-event",
                    G_CALLBACK (delete_event_cb),
                    page);
}

static void
gis_install_page_constructed (GObject *object)
{
  GisInstallPage *page = GIS_INSTALL_PAGE (object);

  G_OBJECT_CLASS (gis_install_page_parent_class)->constructed (object);

  gtk_container_add (GTK_CONTAINER (page), WID ("install-page"));

  gtk_progress_bar_set_fraction (OBJ (GtkProgressBar*, "install_progress"), 0.0);

  gtk_overlay_add_overlay (OBJ (GtkOverlay*, "graphics_overlay"), WID ("infolabels"));

  // XXX: FOR DEBUGGING, hide the buttons in final
  gis_page_set_complete (GIS_PAGE (page), FALSE);

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_install_page_locale_changed (GisPage *page)
{
  gis_page_set_title (page, _("Reformatting"));
}

static void
gis_install_page_class_init (GisInstallPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  page_class->page_id = PAGE_ID;
  page_class->hide_forward_button = TRUE;
  page_class->hide_backward_button = TRUE;
  page_class->hide_window_controls = TRUE;
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
