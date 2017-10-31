/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2016-2017 Endless Mobile, Inc.
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
 */
#include "config.h"
#include "gis-scribe.h"

#include <glib/gi18n.h>

#include "gduxzdecompressor.h"

#define IMAGE_KEYRING "/usr/share/keyrings/eos-image-keyring.gpg"
#define BUFFER_SIZE (1 * 1024 * 1024)

typedef struct _GisScribe {
  GObject parent;

  GFile *image;
  guint64 image_size;
  GFile *signature;
  gchar *keyring_path;
  gchar *drive_path;
  gboolean convert_to_mbr;

  gboolean started;
  guint step;
  gdouble progress;

  GPid gpg_pid;
  GIOChannel *gpg_stdout;
  guint gpg_watch;

  GInputStream *decompressed;

  GMutex mutex;

  /* The fields below are shared with and modified by the worker thread, so all
   * access must be guarded by .mutex. All other fields of this struct are
   * either immutable, not touched by the worker thread, or not touched from
   * the main thread while the worker thread is running.
   */
  gint drive_fd;
  guint64 bytes_written;
  guint set_indeterminate_progress_id;
} GisScribe;

G_DEFINE_QUARK (install-error, gis_install_error)

G_DEFINE_TYPE (GisScribe, gis_scribe, G_TYPE_OBJECT)

typedef enum {
  PROP_IMAGE = 1,
  PROP_IMAGE_SIZE,
  PROP_SIGNATURE,
  PROP_KEYRING_PATH,
  PROP_DRIVE_PATH,
  PROP_DRIVE_FD,
  PROP_CONVERT_TO_MBR,
  PROP_STEP,
  PROP_PROGRESS,
  N_PROPERTIES
} GisScribePropertyId;

static GParamSpec *props[N_PROPERTIES] = { 0 };

static void
gis_scribe_set_property (GObject      *object,
                         guint         property_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GisScribe *self = GIS_SCRIBE (object);

  switch ((GisScribePropertyId) property_id)
    {
    case PROP_IMAGE:
      g_clear_object (&self->image);
      self->image = G_FILE (g_value_dup_object (value));
      break;

    case PROP_IMAGE_SIZE:
      self->image_size = g_value_get_uint64 (value);
      break;

    case PROP_SIGNATURE:
      g_clear_object (&self->signature);
      self->signature = G_FILE (g_value_dup_object (value));
      break;

    case PROP_KEYRING_PATH:
      g_free (self->keyring_path);
      self->keyring_path = g_value_dup_string (value);
      break;

    case PROP_DRIVE_PATH:
      g_free (self->drive_path);
      self->drive_path = g_value_dup_string (value);
      break;

    case PROP_DRIVE_FD:
      if (self->drive_fd != -1)
        close (self->drive_fd);
      self->drive_fd = g_value_get_int (value);
      break;

    case PROP_CONVERT_TO_MBR:
      self->convert_to_mbr = g_value_get_boolean (value);
      break;

    case PROP_STEP:
    case PROP_PROGRESS:
    case N_PROPERTIES:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gis_scribe_get_property (GObject      *object,
                         guint         property_id,
                         GValue       *value,
                         GParamSpec   *pspec)
{
  GisScribe *self = GIS_SCRIBE (object);

  switch ((GisScribePropertyId) property_id)
    {
    case PROP_IMAGE:
      g_value_set_object (value, self->image);
      break;

    case PROP_IMAGE_SIZE:
      g_value_set_uint64 (value, self->image_size);
      break;

    case PROP_SIGNATURE:
      g_value_set_object (value, self->signature);
      break;

    case PROP_KEYRING_PATH:
      g_value_set_string (value, self->keyring_path);
      break;

    case PROP_DRIVE_PATH:
      g_value_set_string (value, self->drive_path);
      break;

    case PROP_DRIVE_FD:
      g_value_set_int (value, self->drive_fd);
      break;

    case PROP_CONVERT_TO_MBR:
      g_value_set_int (value, self->convert_to_mbr);
      break;

    case PROP_STEP:
      g_value_set_uint (value, self->step);
      break;

    case PROP_PROGRESS:
      g_value_set_double (value, self->progress);
      break;

    case N_PROPERTIES:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gis_scribe_constructed (GObject *object)
{
  GisScribe *self = GIS_SCRIBE (object);

  G_OBJECT_CLASS (gis_scribe_parent_class)->constructed (object);

  g_return_if_fail (self->image != NULL);
  g_return_if_fail (self->signature != NULL);
  g_return_if_fail (self->keyring_path != NULL);
  g_return_if_fail (self->drive_path != NULL);
  g_return_if_fail (self->drive_fd >= 0);
}

static void
gis_scribe_dispose (GObject *object)
{
  GisScribe *self = GIS_SCRIBE (object);

  g_clear_object (&self->image);
  g_clear_object (&self->signature);
  g_clear_object (&self->decompressed);

  G_OBJECT_CLASS (gis_scribe_parent_class)->dispose (object);
}

static void
gis_scribe_finalize (GObject *object)
{
  GisScribe *self = GIS_SCRIBE (object);

  g_clear_pointer (&self->keyring_path, g_free);
  g_clear_pointer (&self->drive_path, g_free);
  g_mutex_clear (&self->mutex);

  if (self->drive_fd != -1)
    close (self->drive_fd);
  self->drive_fd = -1;

  G_OBJECT_CLASS (gis_scribe_parent_class)->finalize (object);
}

static void
gis_scribe_class_init (GisScribeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gis_scribe_constructed;
  object_class->set_property = gis_scribe_set_property;
  object_class->get_property = gis_scribe_get_property;
  object_class->dispose = gis_scribe_dispose;
  object_class->finalize = gis_scribe_finalize;

  props[PROP_IMAGE] = g_param_spec_object (
      "image",
      "Image",
      "Image file to write to disk.",
      G_TYPE_FILE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_IMAGE_SIZE] = g_param_spec_uint64 (
      "image-size",
      "Image Size",
      "Uncompressed size of :image, in bytes.",
      0, G_MAXUINT64, 0,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_SIGNATURE] = g_param_spec_object (
      "signature",
      "Signature",
      "Detached GPG signature for :image.",
      G_TYPE_FILE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_KEYRING_PATH] = g_param_spec_string (
      "keyring-path",
      "Keyring path",
      "Path to GPG keyring holding image signing public keys",
      IMAGE_KEYRING,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /* Providing both the path and the fd seems redundant. However: in the app
   * proper, we open the fd using udisks so we can write to it as an
   * unprivileged user, but also need to be able to run a script on the device,
   * elevated with pkexec.
   *
   * Why not accept a UDisksBlock and perform this step internally? It's
   * convenient for testing to be able to operate on a regular file, too.
   */
  props[PROP_DRIVE_PATH] = g_param_spec_string (
      "drive-path",
      "Drive path",
      "Path to target drive.",
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_DRIVE_FD] = g_param_spec_int (
      "drive-fd",
      "Drive FD",
      "Writable file descriptor for drive-path, which is guaranteed to be "
      "close()d by this class.",
      -1, G_MAXINT, -1,  /* -1 for "invalid" */
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_CONVERT_TO_MBR] = g_param_spec_boolean (
      "convert-to-mbr",
      "Convert to MBR?",
      "Whether to convert the partition table from GPT to MBR after writing",
      FALSE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_STEP] = g_param_spec_uint (
      "step",
      "Step",
      "Current step, indexed from 1",
      1, 2, 1,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  props[PROP_PROGRESS] = g_param_spec_double (
      "progress",
      "Progress",
      "Progress in the current step, between 0 and 1 inclusive, "
      "or -1 if exact progress can't be determined.",
      -1, 1, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPERTIES, props);
}

static void
gis_scribe_init (GisScribe *self)
{
  g_mutex_init (&self->mutex);
  self->drive_fd = -1;
  self->step = 1;
}

GisScribe *
gis_scribe_new (GFile       *image,
                guint64      image_size,
                GFile       *signature,
                const gchar *drive_path,
                gint         drive_fd,
                gboolean     convert_to_mbr)
{
  g_return_val_if_fail (G_IS_FILE (image), NULL);
  g_return_val_if_fail (image_size > 0, NULL);
  g_return_val_if_fail (G_IS_FILE (signature), NULL);
  g_return_val_if_fail (drive_path != NULL, NULL);
  g_return_val_if_fail (drive_fd >= 0, NULL);

  return g_object_new (
      GIS_TYPE_SCRIBE,
      "image", image,
      "image-size", image_size,
      "signature", signature,
      "drive-path", drive_path,
      "drive-fd", drive_fd,
      "convert-to-mbr", convert_to_mbr,
      NULL);
}

static gboolean
gis_scribe_update_progress (gpointer data)
{
  GisScribe *self = GIS_SCRIBE (data);
  guint64 bytes_written;

  g_mutex_lock (&self->mutex);
  bytes_written = self->bytes_written;
  g_mutex_unlock (&self->mutex);

  self->progress = ((gdouble) bytes_written) / ((gdouble) self->image_size);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PROGRESS]);

  return G_SOURCE_CONTINUE;
}

static gboolean
gis_scribe_set_indeterminate_progress (gpointer data)
{
  GisScribe *self = GIS_SCRIBE (data);

  g_mutex_lock (&self->mutex);
  self->set_indeterminate_progress_id = 0;
  g_mutex_unlock (&self->mutex);

  self->progress = -1;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PROGRESS]);

  return G_SOURCE_REMOVE;
}

static gboolean
gis_scribe_convert_to_mbr (GisScribe *self,
                           GError   **error)
{
  const char *cmd = "/usr/sbin/eos-repartition-mbr";
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) process = NULL;

  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_NONE);
  /* pkexec won't let us run the program if $SHELL isn't in /etc/shells,
   * so remove it from the environment.
   */
  g_subprocess_launcher_unsetenv (launcher, "SHELL");
  process = g_subprocess_launcher_spawn (launcher, error,
                                         "pkexec", cmd, self->drive_path, NULL);
  if (process == NULL ||
      !g_subprocess_wait_check (process, NULL, error))
    {
      g_prefix_error (error, "failed to run %s %s: ",
                      cmd, self->drive_path);
      return FALSE;
    }

  return TRUE;
}

static void
gis_scribe_write_thread (GTask        *task,
                         gpointer      source_object,
                         gpointer      task_data,
                         GCancellable *cancellable)
{
  GisScribe *self = GIS_SCRIBE (source_object);
  g_autofree gchar *buffer = (gchar*) g_malloc0 (BUFFER_SIZE);
  GError *error = NULL;
  gssize r = -1, w = -1;
  guint timer_id;

  timer_id = g_timeout_add_seconds (1, gis_scribe_update_progress, self);

  g_thread_yield ();

  do
    {
      r = g_input_stream_read (self->decompressed, buffer, BUFFER_SIZE,
                               cancellable, &error);

      if (r > 0)
        {
          /* We lock to protect the bytes_written and access to drive_fd. */
          g_mutex_lock (&self->mutex);

          w = write (self->drive_fd, buffer, r);
          self->bytes_written += w;

          g_mutex_unlock (&self->mutex);
        }
    }
  while (r > 0 && w > 0);

  g_source_remove (timer_id);

  if (r < 0 || error != NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  /* Sync, probe and repartition  can take a long time; notify the UI thread of
   * indeterminate progress.
   */
  g_mutex_lock (&self->mutex);
  self->set_indeterminate_progress_id =
    g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                     gis_scribe_set_indeterminate_progress,
                     g_object_ref (self), g_object_unref);
  g_mutex_unlock (&self->mutex);

  g_thread_yield ();

  syncfs (self->drive_fd);
  close (self->drive_fd);
  self->drive_fd = -1;

  g_spawn_command_line_sync ("partprobe", NULL, NULL, NULL, NULL);
  if (self->convert_to_mbr)
    gis_scribe_convert_to_mbr (self, &error);

  g_mutex_lock (&self->mutex);
  /* If we didn't get around to setting progress to -1 in the main thread, it's
   * too late now anyway!
   */
  if (self->set_indeterminate_progress_id != 0)
    g_source_remove (self->set_indeterminate_progress_id);
  self->set_indeterminate_progress_id = 0;
  g_mutex_unlock (&self->mutex);

  if (error != NULL)
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, TRUE);
}

static void
gis_scribe_begin_write (GisScribe *self,
                        GTask     *task)
{
  g_autofree gchar *basename = NULL;
  g_autoptr(GConverter) decompressor = NULL;
  GInputStream *input = NULL;
  GError *error = NULL;

  basename = g_file_get_basename (self->image);
  if (basename == NULL)
    {
      g_autofree gchar *parse_name = g_file_get_parse_name (self->image);
      g_task_return_new_error (task,
                               GIS_INSTALL_ERROR, 0,
                               "g_file_get_basename(\"%s\") returned NULL",
                               parse_name);
      return;
    }

  /* TODO: use more magical means */
  if (g_str_has_suffix (basename, "gz"))
    {
      GZlibCompressorFormat cf = G_ZLIB_COMPRESSOR_FORMAT_GZIP;
      decompressor = G_CONVERTER (g_zlib_decompressor_new (cf));
    }
  else if (g_str_has_suffix (basename, "xz"))
    {
      decompressor = G_CONVERTER (gdu_xz_decompressor_new ());
    }
  else if (g_str_has_suffix (basename, "img") || g_strcmp0 (basename, "endless-image") == 0)
    {
      decompressor = NULL;
    }
  else
    {
      g_task_return_new_error (task, GIS_INSTALL_ERROR, 0,
                               "%s ends in neither '.xz', '.gz' nor '.img'",
                               basename);
      return;
    }

  input = G_INPUT_STREAM (g_file_read (self->image,
                                       g_task_get_cancellable (task),
                                       &error));
  if (input == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  if (decompressor)
    {
      self->decompressed = g_converter_input_stream_new (input, decompressor);
      g_object_unref (input);
    }
  else
    {
      self->decompressed = input;
    }

  self->step = 2;
  self->progress = 0;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_STEP]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PROGRESS]);

  g_task_run_in_thread (task, gis_scribe_write_thread);
}

static gboolean
gis_scribe_gpg_progress (GIOChannel  *source,
                         GIOCondition cond,
                         gpointer     data)
{
  GTask *task = G_TASK (data);
  GisScribe *self = GIS_SCRIBE (g_task_get_source_object (task));
  g_autofree gchar *line = NULL;
  g_auto(GStrv) arr = NULL;
  gdouble curr, full;
  gchar *units = NULL;

  if (self->step > 1)
    return G_SOURCE_CONTINUE;

  if (g_io_channel_read_line (source, &line, NULL, NULL, NULL) != G_IO_STATUS_NORMAL
      || line == NULL)
    return G_SOURCE_CONTINUE;

  if (!g_str_has_prefix (line, "[GNUPG:] PROGRESS"))
    return G_SOURCE_CONTINUE;

  /* https://git.gnupg.org/cgi-bin/gitweb.cgi?p=gnupg.git;a=blob;f=doc/DETAILS;h=0be55f4d;hb=refs/heads/master#l1043
   * [GNUPG:] PROGRESS <what> <char> <cur> <total> [<units>]
   * For example:
   * [GNUPG:] PROGRESS /dev/mapper/endless- ? 676 4442 MiB
   */
  arr = g_strsplit (g_strchomp (line), " ", -1);
  curr = g_ascii_strtod (arr[4], NULL);
  full = g_ascii_strtod (arr[5], NULL);
  units = arr[6];

  if (full < 1024 && g_strcmp0 (units, "B") == 0)
    {
      /* GPG reports progress reading the signature, not just the image. Assume
       * any file less than 1 KiB is the signature and ignore it.
       */
      return G_SOURCE_CONTINUE;
    }

  if (full == 0)
    {
      /* gpg can't determine the file size. Should not happen, since we
       * gave it a hint.
       */
      self->progress = -1;
    }
  else
    {
      self->progress = CLAMP (curr / full, 0, 1);
    }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PROGRESS]);
  return G_SOURCE_CONTINUE;
}

static void
gis_scribe_gpg_watch (GPid     pid,
                      gint     status,
                      gpointer data)
{
  g_autoptr(GTask) task = G_TASK (data);
  GisScribe *self = GIS_SCRIBE (g_task_get_source_object (task));

  g_spawn_close_pid (pid);

  g_source_remove (self->gpg_watch);
  self->gpg_watch = 0;

  g_io_channel_shutdown (self->gpg_stdout, TRUE, NULL);
  g_clear_pointer (&self->gpg_stdout, g_io_channel_unref);

  if (g_spawn_check_exit_status (status, NULL))
    {
      gis_scribe_begin_write (self, task);
    }
  else
    {
      g_task_return_new_error (task, GIS_INSTALL_ERROR, 0,
                               _("Image verification error."));
    }
}

static void
gis_scribe_begin_verify (GisScribe *self,
                         GTask     *task)
{
  g_autofree gchar *image_path = g_file_get_path (self->image);
  g_autofree gchar *signature_path = g_file_get_path (self->signature);
  g_autofree gchar *size_str = g_strdup_printf ("%" G_GUINT64_FORMAT, self->image_size);
  const gchar * const args[] = {
      "gpg",
      "--enable-progress-filter", "--status-fd", "1",
      /* Trust the one key in this keyring, and no others */
      "--keyring", self->keyring_path,
      "--no-default-keyring",
      "--trust-model", "always",
      "--input-size-hint", size_str,
      "--verify", signature_path, image_path, NULL
  };
  gint outfd;
  GError *error = NULL;

  if (!g_file_query_exists (self->signature, NULL))
    {
      g_task_return_new_error (
          task, GIS_INSTALL_ERROR, 0,
          _("The Endless OS signature file \"%s\" does not exist."),
          signature_path);
      g_object_unref (task);
      return;
    }

  if (!g_spawn_async_with_pipes (NULL, (gchar **) args, NULL,
                                 G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                                 NULL, NULL, &self->gpg_pid,
                                 NULL, &outfd, NULL, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      g_object_unref (task);
      return;
    }

  self->gpg_stdout = g_io_channel_unix_new (outfd);
  self->gpg_watch = g_io_add_watch (self->gpg_stdout,
                                    G_IO_IN,
                                    gis_scribe_gpg_progress,
                                    task);

  g_child_watch_add (self->gpg_pid, gis_scribe_gpg_watch, task);
}

void
gis_scribe_write_async (GisScribe          *self,
                        GCancellable       *cancellable,
                        GAsyncReadyCallback callback,
                        gpointer            user_data)
{
  g_autoptr(GTask) task = g_task_new (self, cancellable, callback, user_data);

  if (self->started)
    {
      g_task_return_new_error (task, GIS_INSTALL_ERROR, 0, "already started");
    }
  else
    {
      self->started = TRUE;
      gis_scribe_begin_verify (self, g_steal_pointer (&task));
    }
}

gboolean
gis_scribe_write_finish (GisScribe    *self,
                         GAsyncResult *result,
                         GError      **error)
{
  GTask *task = G_TASK (result);

  return g_task_propagate_boolean (task, error);
}

guint
gis_scribe_get_step (GisScribe *self)
{
  g_return_val_if_fail (GIS_IS_SCRIBE (self), 0);

  return self->step;
}

gdouble
gis_scribe_get_progress (GisScribe *self)
{
  g_return_val_if_fail (GIS_IS_SCRIBE (self), -1);

  return self->progress;
}
