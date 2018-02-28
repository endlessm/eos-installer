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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */
#include "config.h"
#include "gis-scribe.h"

#include <errno.h>
#include <gio/gfiledescriptorbased.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <glib-unix.h>
#include <glib/gi18n.h>

#include <sys/ioctl.h>
/* for BLKGETSIZE64, BLKDISCARD */
#include <linux/fs.h>
/* for sysconf() */
#include <unistd.h>

#include "glnx-errors.h"

#define IMAGE_KEYRING "/usr/share/keyrings/eos-image-keyring.gpg"
#define BUFFER_SIZE (1 * 1024 * 1024)
/* MBR + two copies of (GPT header plus at least 32 512-byte sectors of
 * partition entries)
 */
#define MINIMUM_IMAGE_SIZE ((1 + 33 * 2) * 512)
/* You could imagine a compression format which compresses a degenerate GPT
 * disk image to one byte.
 */
#define MINIMUM_COMPRESSED_SIZE 1

typedef enum {
  GIS_SCRIBE_TASK_TEE        = 1 << 0,
  GIS_SCRIBE_TASK_VERIFY     = 1 << 1,
  GIS_SCRIBE_TASK_DECOMPRESS = 1 << 2,
  GIS_SCRIBE_TASK_WRITE      = 1 << 3,
} GisScribeTask;

static const gchar *
gis_scribe_task_get_label (GisScribeTask flag)
{
  switch (flag)
    {
    case GIS_SCRIBE_TASK_TEE:
      return "tee";
    case GIS_SCRIBE_TASK_VERIFY:
      return "verify";
    case GIS_SCRIBE_TASK_DECOMPRESS:
      return "decompress";
    case GIS_SCRIBE_TASK_WRITE:
      return "write";
    default:
      return "invalid task flag";
    }
}

typedef struct _GisScribe {
  GObject parent;

  GFile *image;
  guint64 image_size_bytes;
  /* Compressed size of 'image'. We need to provide this to GPG so it can
   * indicate its progress, since it reads the image data from a pipe. Equal to
   * 'image_size_bytes' if 'image' is uncompressed.
   */
  guint64 compressed_size_bytes;
  GFile *signature;
  gchar *keyring_path;
  gchar *drive_path;
  gboolean convert_to_mbr;

  gboolean started;
  guint step;
  gdouble gpg_progress;

  /* MIN(gpg_progress, bytes_written / image_size_bytes) */
  gdouble overall_progress;

  GMutex mutex;
  GCond cond;

  /* The fields below are shared with and modified by the worker threads, so all
   * access must be guarded by .mutex. All other fields of this struct are
   * either immutable, not touched by the worker threads, or not touched from
   * the main thread while the worker threads are running.
   */

  /* Bitwise-or of GisScribeTask for tasks that have not yet completed. */
  gint outstanding_tasks;

  /* The first error reported by a subtask, or NULL if all (so far) have
   * completed successfully. In particular, this is non-NULL if
   * (outstanding_tasks & GIS_SCRIBE_TASK_VERIFY) == 0 (ie the GPG step has
   * completed) and GPG returned an error. The write sub-task uses this as a
   * signal to abort the write process.
   */
  GError *error;

  gint drive_fd;
  guint64 bytes_written;
  guint set_indeterminate_progress_id;
  gint64 start_time_usec;
} GisScribe;

/* Data for the subtask which reads the file from disk and feeds it to the
 * (concurrent) verification and writer subtasks.
 */
typedef struct {
  GInputStream *image_input;

  GOutputStream *gpg_stdin;
  GOutputStream *write_pipe;
} GisScribeTeeData;

typedef struct {
  GSubprocess *subprocess;
  GSource *stdout_source;
  /* Wraps gpg_subprocess's stdout pipe. When gpg_stdout_source triggers
   * then reading a line is expected to succeed (or fail) without blocking.
   */
  GDataInputStream *stdout_;
} GisScribeGpgData;

static void
gis_scribe_gpg_data_free (GisScribeGpgData *data)
{
  /* Cleaned up when GPG subprocess dies. Since g_task_attach_source() takes a
   * reference to the task, once the source is created the task should remain
   * alive until the source is destroyed, and hence so should this data.
   */
  g_assert (data->stdout_source == NULL);

  g_clear_object (&data->stdout_);
  g_clear_object (&data->subprocess);

  g_slice_free (GisScribeGpgData, data);
}

G_DEFINE_QUARK (install-error, gis_install_error)

G_DEFINE_TYPE (GisScribe, gis_scribe, G_TYPE_OBJECT)

typedef enum {
  PROP_IMAGE = 1,
  PROP_IMAGE_SIZE,
  PROP_COMPRESSED_SIZE,
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
      self->image_size_bytes = g_value_get_uint64 (value);
      break;

    case PROP_COMPRESSED_SIZE:
      self->compressed_size_bytes = g_value_get_uint64 (value);
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
      g_value_set_uint64 (value, self->image_size_bytes);
      break;

    case PROP_COMPRESSED_SIZE:
      g_value_set_uint64 (value, self->compressed_size_bytes);
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
      g_value_set_double (value, self->overall_progress);
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

  G_OBJECT_CLASS (gis_scribe_parent_class)->dispose (object);
}

static void
gis_scribe_finalize (GObject *object)
{
  GisScribe *self = GIS_SCRIBE (object);

  g_assert_cmpint (self->outstanding_tasks, ==, 0);

  g_clear_pointer (&self->keyring_path, g_free);
  g_clear_pointer (&self->drive_path, g_free);
  g_clear_error (&self->error);
  g_mutex_clear (&self->mutex);
  g_cond_clear (&self->cond);

  if (self->drive_fd != -1)
    close (self->drive_fd);
  self->drive_fd = -1;

  G_OBJECT_CLASS (gis_scribe_parent_class)->finalize (object);
}

static void
gis_scribe_class_init (GisScribeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* Ensure write() to a pipe whose read end is closed fails with EPIPE rather
   * than killing the process with SIGPIPE. This precaution is also taken by
   * Gtk+ and by any use of GSocket, so when run as part of the full
   * application this is redundant, but when used in isolation this class does
   * not use either of those.
   *
   * (You might hope that you could use send() with MSG_NOSIGNAL but this can
   * only be used with socket fds, not pipes.)
   */
  signal (SIGPIPE, SIG_IGN);

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
      MINIMUM_IMAGE_SIZE, G_MAXUINT64, MINIMUM_IMAGE_SIZE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_COMPRESSED_SIZE] = g_param_spec_uint64 (
      "compressed-size",
      "Compressed Size",
      "Compressed size of :image, in bytes.",
      MINIMUM_COMPRESSED_SIZE, G_MAXUINT64, MINIMUM_COMPRESSED_SIZE,
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

  /**
   * GisScribe:step:
   *
   * Current step, indexed from 1.
   */
  props[PROP_STEP] = g_param_spec_uint (
      "step",
      "Step",
      "Current step, indexed from 1",
      1, 2, 1,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GisScribe:progress:
   *
   * Progress in the current GisScribe:step, between 0 and 1 inclusive, or -1
   * if exact progress can't be determined. Within a given step, this property
   * is either a constant -1, or increases linearly with progress. It's not
   * guaranteed to reach 1 before the step completes.
   */
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
  g_cond_init (&self->cond);

  self->drive_fd = -1;
  self->step = 1;
}

GisScribe *
gis_scribe_new (GFile       *image,
                guint64      image_size_bytes,
                guint64      compressed_size_bytes,
                GFile       *signature,
                const gchar *drive_path,
                gint         drive_fd,
                gboolean     convert_to_mbr)
{
  g_return_val_if_fail (G_IS_FILE (image), NULL);
  g_return_val_if_fail (image_size_bytes > MINIMUM_IMAGE_SIZE, NULL);
  g_return_val_if_fail (compressed_size_bytes > MINIMUM_COMPRESSED_SIZE, NULL);
  g_return_val_if_fail (G_IS_FILE (signature), NULL);
  g_return_val_if_fail (drive_path != NULL, NULL);
  g_return_val_if_fail (drive_fd >= 0, NULL);

  return g_object_new (
      GIS_TYPE_SCRIBE,
      "image", image,
      "image-size", image_size_bytes,
      "compressed-size", compressed_size_bytes,
      "signature", signature,
      "drive-path", drive_path,
      "drive-fd", drive_fd,
      "convert-to-mbr", convert_to_mbr,
      NULL);
}

static void
gis_scribe_close_input_stream_or_warn (GInputStream *stream,
                                       GCancellable *cancellable,
                                       const gchar  *label)
{
  g_autoptr(GError) error = NULL;

  if (!g_input_stream_close (stream, cancellable, &error))
    g_warning ("error closing %s: %s", label, error->message);
}

static void
gis_scribe_close_output_stream_or_warn (GOutputStream *stream,
                                        GCancellable  *cancellable,
                                        const gchar   *label)
{
  g_autoptr(GError) error = NULL;

  if (!g_output_stream_close (stream, cancellable, &error))
    g_warning ("error closing %s: %s", label, error->message);
}

/* Called once per second while the main write operation is in progress.
 */
static gboolean
gis_scribe_update_progress (gpointer data)
{
  GisScribe *self = GIS_SCRIBE (data);
  guint64 bytes_written;
  gdouble write_progress;
  gdouble progress;

  g_mutex_lock (&self->mutex);
  bytes_written = self->bytes_written;
  g_mutex_unlock (&self->mutex);

  write_progress = ((gdouble) bytes_written) / ((gdouble) self->image_size_bytes);
  /* You'd expect these to be identical ± 1 MiB in the uncompressed case, and
   * pretty close in the compressed case assuming the compression ratio is
   * roughly constant throughout the file.
   */
  progress = MIN (self->gpg_progress, write_progress);

  g_debug ("%s: GPG progress %3.0f%%, write progress %3.0f%%",
           G_STRFUNC, self->gpg_progress * 100, write_progress * 100);

  if (progress != self->overall_progress)
    {
      self->overall_progress = progress;
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PROGRESS]);
    }

  return G_SOURCE_CONTINUE;
}

/* Run BLKDISCARD on the whole device. This tells flash-based disks that all
 * data is now unused, which frees up wear levelling algorithms and boosts
 * performance for next time each eraseblock is written.
 */
static gboolean
gis_scribe_blkdiscard (gint     fd,
                       GError **error)
{
  guint64 range[2];

  range[0] = 0;

  if (ioctl (fd, BLKGETSIZE64, &range[1]))
    return glnx_throw_errno_prefix (error, "can't get size for blkdiscard");

  if (ioctl (fd, BLKDISCARD, &range))
    return glnx_throw_errno_prefix (error, "blkdiscard failed");

  return TRUE;
}

static gboolean
gis_scribe_write_thread_await_gpg (GisScribe *self,
                                   GError   **error)
{
  g_autoptr(GError) local_error = NULL;

  g_mutex_lock (&self->mutex);
  /* Wait until all other tasks have ended */
  while ((self->outstanding_tasks & ~GIS_SCRIBE_TASK_WRITE) != 0)
    g_cond_wait (&self->cond, &self->mutex);

  /* This task itself should still be outstanding */
  g_assert_cmpint (self->outstanding_tasks, ==, GIS_SCRIBE_TASK_WRITE);

  if (self->error != NULL)
    local_error = g_error_copy (self->error);
  g_mutex_unlock (&self->mutex);

  if (local_error == NULL)
    return TRUE;

  /* This error will already have been reported by whichever task failed, but
   * we have to throw *some* error from this task.
   */
  g_propagate_error (error, g_steal_pointer (&local_error));
  return FALSE;
}

/* Allocates size bytes, page-aligned. */
G_GNUC_MALLOC
G_GNUC_ALLOC_SIZE(1)
static void *
gis_scribe_malloc_aligned (size_t size)
{
  const size_t pagesize = sysconf (_SC_PAGESIZE);
  void *buf = NULL;

  if (posix_memalign (&buf, pagesize, size) != 0 || buf == NULL)
    g_error ("%s: failed to allocate %" G_GSIZE_FORMAT " bytes "
             "aligned to page size %" G_GSIZE_FORMAT ": %s",
             G_STRFUNC, size, pagesize, g_strerror (errno));

  return buf;
}

static gboolean
gis_scribe_write_thread_copy (GisScribe     *self,
                              GInputStream  *decompressed,
                              gint           fd,
                              GOutputStream *output,
                              GCancellable  *cancellable,
                              GError       **error)
{
  g_autofree gchar *buffer = gis_scribe_malloc_aligned (BUFFER_SIZE);
  g_autofree gchar *first_mib = gis_scribe_malloc_aligned (BUFFER_SIZE);
  gsize first_mib_bytes_read = 0;
  gsize r = 0;
  gsize w = 0;

  /* Read the first 1 MiB; write zeros to the target drive. This ensures the
   * system won't boot until the image is fully written.
   */
  memset (first_mib, 0, BUFFER_SIZE);
  if (!g_output_stream_write_all (output, first_mib, BUFFER_SIZE,
                                  &w, cancellable, error)
      || !g_input_stream_read_all (decompressed, first_mib, BUFFER_SIZE,
                                   &first_mib_bytes_read, cancellable, error))
    return FALSE;

  do
    {
      if (!g_input_stream_read_all (decompressed, buffer, BUFFER_SIZE,
                                    &r, cancellable, error))
        return FALSE;

      if (!g_output_stream_write_all (output, buffer, r,
                                      &w, cancellable, error))
        return FALSE;

      /* We lock to protect bytes_written */
      g_mutex_lock (&self->mutex);
      self->bytes_written += w;
      g_mutex_unlock (&self->mutex);
    }
  while (r > 0);

  if (!g_input_stream_close (decompressed, cancellable, error))
    return FALSE;

  /* Wait for GPG verification to complete */
  if (!gis_scribe_write_thread_await_gpg (self, error))
    return FALSE;

  /* Check that we've written the same amount of data as we expected from the
   * GPT header. This would only fail if there's something seriously wrong with
   * the image builder, the decompressor, or the read/write loop above.
   */
  g_mutex_lock (&self->mutex);
  /* Don't forget the first <= 1 MiB we saved for later! */
  guint64 bytes_written = self->bytes_written + first_mib_bytes_read;
  g_mutex_unlock (&self->mutex);

  if (bytes_written != self->image_size_bytes)
    {
      g_autoptr(GError) local_error =
        g_error_new (GIS_INSTALL_ERROR, 0,
                     "wrote %" G_GUINT64_FORMAT " bytes, "
                     "expected to write %" G_GUINT64_FORMAT " bytes",
                     bytes_written, self->image_size_bytes);

      /* Due to an image builder bug, for a few days very large images might
       * not be a round number of sectors long. We obtain the uncompressed size
       * from the GPT header, which is expressed in sectors. The bug has been
       * fixed but we want to allow these images through for now.
       *
       * https://phabricator.endlessm.com/T20064
       */
      g_autofree gchar *image_path = g_file_get_path (self->image);
      if (bytes_written % 512 != 0
          && self->image_size_bytes % 512 == 0
          && bytes_written / 512 == self->image_size_bytes / 512
          && g_regex_match_simple ("\\.1711(1[56789]|2[0123])-\\d{6}\\.",
                                   image_path, 0, 0))
        {
          g_message ("%s; ignoring due to T20064", local_error->message);
        }
      else
        {
          g_propagate_error (error, g_steal_pointer (&local_error));
          return FALSE;
        }
    }

  /* Now write the first 1 MiB to disk. Unfortunately GUnixOutputStream does
   * not implement GSeekable.
   */
  if (lseek (fd, 0, SEEK_SET) < 0)
    return glnx_throw_errno_prefix (error, "can't seek to start of disk");

  if (!g_output_stream_write_all (output, first_mib, first_mib_bytes_read,
                                  &w, cancellable, error))
    return FALSE;

  g_mutex_lock (&self->mutex);
  self->bytes_written += w;
  g_mutex_unlock (&self->mutex);

  return TRUE;
}

static gboolean
gis_scribe_set_indeterminate_progress (gpointer data)
{
  GisScribe *self = GIS_SCRIBE (data);

  g_mutex_lock (&self->mutex);
  self->set_indeterminate_progress_id = 0;
  g_mutex_unlock (&self->mutex);

  self->step = 2;
  self->overall_progress = -1;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_STEP]);
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
gis_scribe_log_duration (GisScribe   *self,
                         const gchar *label)
{
  gint64 now_usec = g_get_monotonic_time ();
  gint64 duration = now_usec - self->start_time_usec;
  gint64 hours = (duration / G_USEC_PER_SEC) / (60 * 60);
  int minutes = ((duration / G_USEC_PER_SEC) / 60) % 60;
  int seconds = (duration / G_USEC_PER_SEC) % 60;

  g_message ("%s: %01" G_GINT64_FORMAT ":%02d:%02d",
             label, hours, minutes, seconds);
}

static void
gis_scribe_write_thread (GTask        *task,
                         gpointer      source_object,
                         gpointer      task_data,
                         GCancellable *cancellable)
{
  GisScribe *self = GIS_SCRIBE (source_object);
  GInputStream *decompressed = G_INPUT_STREAM (task_data);
  gint fd = -1;
  g_autoptr(GOutputStream) output = NULL;
  gboolean ret;
  g_autoptr(GError) error = NULL;
  guint timer_id;

  /* Transfer ownership of drive_fd; the GOutputStream will close it. */
  g_mutex_lock (&self->mutex);
  fd = self->drive_fd;
  self->drive_fd = -1;
  g_mutex_unlock (&self->mutex);
  output = g_unix_output_stream_new (fd, TRUE);

  timer_id = g_timeout_add_seconds (1, gis_scribe_update_progress, self);

  g_thread_yield ();

  if (!gis_scribe_blkdiscard (fd, &error))
    {
      /* Not fatal: the target device may not support this. */
      g_message ("%s", error->message);
      g_clear_error (&error);
    }

  ret = gis_scribe_write_thread_copy (self, decompressed, fd, output,
                                      cancellable, &error);

  g_source_remove (timer_id);

  if (!ret)
    {
      g_autoptr(GError) close_error = NULL;

      if (error == NULL)
        {
          /* This path should not be reached. To avoid translators
           * translating a technical message which should never be shown, we only
           * mark "Internal error" for translation.
           */
          g_set_error (&error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "%s: %s", _("Internal error"),
                       "gis_scribe_write_thread_copy failed with no error");
          g_critical ("%s", error->message);
        }

      g_task_return_error (task, g_steal_pointer (&error));

      /* On the happy path, gis_scribe_write_thread_copy() closes the
       * decompressed stream when it reaches EOF. If we hit a write error
       * before EOF, we need to close the decompressed stream to ensure the
       * threads upstream of us terminate.
       */
      gis_scribe_close_input_stream_or_warn (decompressed, cancellable,
                                             "decompressed stream");
      return;
    }

  gis_scribe_log_duration (self, "image fully written");

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

  if (syncfs (fd) < 0)
    {
      glnx_throw_errno_prefix (&error, "syncfs failed");
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (!g_output_stream_close (output, cancellable, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

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
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);

  gis_scribe_log_duration (self, "write complete");
}

static void
gis_scribe_begin_write (GisScribe          *self,
                        GInputStream       *decompressed,
                        GCancellable       *cancellable,
                        GAsyncReadyCallback callback,
                        gpointer            data)
{
  g_autoptr(GTask) task = g_task_new (self, cancellable, callback, data);

  g_task_set_source_tag (task, GUINT_TO_POINTER (GIS_SCRIBE_TASK_WRITE));
  g_task_set_task_data (task, g_object_ref (decompressed), g_object_unref);
  g_task_run_in_thread (task, gis_scribe_write_thread);
}

static gboolean
gis_scribe_gpg_progress (GObject *pollable_stream,
                         gpointer data)
{
  GTask *task = G_TASK (data);
  GisScribeGpgData *task_data = g_task_get_task_data (task);
  GisScribe *self = GIS_SCRIBE (g_task_get_source_object (task));
  g_autofree gchar *line = NULL;
  g_auto(GStrv) arr = NULL;
  guint64 curr, full;
  gchar *units = NULL;
  g_autoptr(GError) error = NULL;

  line = g_data_input_stream_read_line_utf8 (task_data->stdout_, NULL,
                                             g_task_get_cancellable (task),
                                             NULL);
  if (line == NULL)
    return G_SOURCE_CONTINUE;

  g_debug ("%s: %s", G_STRFUNC, line);

  if (!g_str_has_prefix (line, "[GNUPG:] PROGRESS"))
    {
      /* TODO: handle GOODSIG/EXPSIG/BADSIG/etc. to surface the exact
       * verification error. Or use GPGME?
       */
      return G_SOURCE_CONTINUE;
    }

  /* https://git.gnupg.org/cgi-bin/gitweb.cgi?p=gnupg.git;a=blob;f=doc/DETAILS;h=0be55f4d;hb=refs/heads/master#l1043
   * [GNUPG:] PROGRESS <what> <char> <cur> <total> [<units>]
   * For example:
   * [GNUPG:] PROGRESS /dev/mapper/endless- ? 676 4442 MiB
   */
  arr = g_strsplit (g_strchomp (line), " ", -1);
  if (g_strv_length (arr) < 6)
    {
      g_warning ("%s: GPG progress message has too few fields: %s",
                 G_STRFUNC, line);
      return G_SOURCE_CONTINUE;
    }

  if (!g_ascii_string_to_unsigned (arr[4], 10, 0, G_MAXUINT64, &curr, &error) ||
      !g_ascii_string_to_unsigned (arr[5], 10, 0, G_MAXUINT64, &full, &error))
    {
      g_warning ("%s: couldn't parse GPG progress message '%s': %s",
                 G_STRFUNC, line, error->message);
      return G_SOURCE_CONTINUE;
    }
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
      self->gpg_progress = -1;
    }
  else
    {
      self->gpg_progress = CLAMP ((gdouble) curr / (gdouble) full, 0, 1);
    }

  return G_SOURCE_CONTINUE;
}

static void
gis_scribe_gpg_wait_check_cb (GObject      *source,
                              GAsyncResult *result,
                              gpointer      data)
{
  GSubprocess *gpg_subprocess = G_SUBPROCESS (source);
  g_autoptr(GTask) task = G_TASK (data);
  GisScribeGpgData *task_data = g_task_get_task_data (task);
  gboolean ok;
  g_autoptr(GError) error = NULL;

  g_clear_pointer (&task_data->stdout_source, g_source_destroy);

  ok = g_subprocess_wait_check_finish (gpg_subprocess, result, &error);
  if (ok)
    {
      g_task_return_boolean (task, TRUE);
    }
  else
    {
      /* TODO: surface more details about the error */
      g_message ("GPG subprocess failed: %s", error->message);
      g_task_return_new_error (task, GIS_INSTALL_ERROR, 0,
                               _("Image verification error."));
    }
}

/*
 * If the GPG subprocess cannot be started, this function will return %NULL,
 * and @callback will fire later with the error. Otherwise, it will return
 * a #GOutputStream for the GPG subprocess' stdin, and @callback will be called
 * later with the result of verifying the image.
 *
 * Returns: (transfer full): the GPG subprocess' stdin, or %NULL on error.
 */
static GOutputStream *
gis_scribe_begin_verify (GisScribe *self,
                         GCancellable *cancellable,
                         GAsyncReadyCallback callback,
                         gpointer data)
{
  g_autoptr(GTask) task = g_task_new (self, cancellable, callback, data);
  GisScribeGpgData *task_data = g_slice_new0 (GisScribeGpgData);
  g_autofree gchar *signature_path = g_file_get_path (self->signature);
  g_autofree gchar *size_str = g_strdup_printf ("%" G_GUINT64_FORMAT,
                                                self->compressed_size_bytes);
  const gchar * const args[] = {
      GPG_PATH,
      "--enable-progress-filter", "--status-fd", "1",
      /* Trust the one key in this keyring, and no others */
      "--keyring", self->keyring_path,
      "--no-default-keyring",
      "--trust-model", "always",
      "--input-size-hint", size_str,
      "--verify", signature_path, "-", NULL
  };
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  GInputStream *gpg_stdout = NULL;
  GOutputStream *gpg_stdin = NULL;
  g_autoptr(GError) error = NULL;

  g_task_set_source_tag (task, GUINT_TO_POINTER (GIS_SCRIBE_TASK_VERIFY));
  g_task_set_task_data (task, task_data,
                        (GDestroyNotify) gis_scribe_gpg_data_free);

  if (!g_file_query_exists (self->signature, NULL))
    {
      g_task_return_new_error (
          task, GIS_INSTALL_ERROR, 0,
          _("The Endless OS signature file \"%s\" does not exist."),
          signature_path);
      return NULL;
    }

  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                        G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  task_data->subprocess = g_subprocess_launcher_spawnv (launcher, args, &error);
  if (task_data->subprocess == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return NULL;
    }

  gpg_stdin = g_subprocess_get_stdin_pipe (task_data->subprocess);

  gpg_stdout = g_subprocess_get_stdout_pipe (task_data->subprocess);
  g_assert (G_IS_POLLABLE_INPUT_STREAM (gpg_stdout));
  task_data->stdout_ = g_data_input_stream_new (gpg_stdout);
  task_data->stdout_source =
    g_pollable_input_stream_create_source (G_POLLABLE_INPUT_STREAM (gpg_stdout),
                                           cancellable);
  g_task_attach_source (task, task_data->stdout_source,
                        (GSourceFunc) gis_scribe_gpg_progress);

  g_subprocess_wait_check_async (task_data->subprocess, cancellable,
                                 gis_scribe_gpg_wait_check_cb,
                                 g_steal_pointer (&task));

  return g_object_ref (gpg_stdin);
}

static void
gis_scribe_tee_close (GisScribeTeeData *data,
                      GCancellable     *cancellable)
{
  if (data->image_input != NULL)
    gis_scribe_close_input_stream_or_warn (data->image_input, cancellable,
                                           "file input stream");

  /* Closing its stdin will ultimately cause the GPG subprocess to exit. */
  gis_scribe_close_output_stream_or_warn (data->gpg_stdin, cancellable,
                                          "GPG stdin");

  /* Similarly, closing the pipe will cause the write thread to terminate. */
  gis_scribe_close_output_stream_or_warn (data->write_pipe, cancellable,
                                          "write pipe");
}

static void
gis_scribe_tee_data_free (GisScribeTeeData *data)
{
  gis_scribe_tee_close (data, NULL);

  g_clear_object (&data->image_input);
  g_clear_object (&data->gpg_stdin);
  g_clear_object (&data->write_pipe);

  g_slice_free (GisScribeTeeData, data);
}

/* Reads the image from disk and writes it to both the GPG subprocess and the
 * writer thread.
 */
static void
gis_scribe_tee_thread (GTask            *task,
                       gpointer          source_object,
                       GisScribeTeeData *task_data,
                       GCancellable     *cancellable)
{
  g_autofree gchar *buffer = gis_scribe_malloc_aligned (BUFFER_SIZE);
  g_autoptr(GError) error = NULL;
  gssize r = -1;

  do
    {
      r = g_input_stream_read (task_data->image_input, buffer, BUFFER_SIZE,
                               cancellable, &error);

      if (r < 0)
        {
          g_prefix_error (&error, "error reading image: ");
          break;
        }

      if (!g_output_stream_write_all (task_data->gpg_stdin, buffer, r,
                                      NULL, cancellable, &error))
        {
          g_prefix_error (&error, "error writing image to GPG: ");
          break;
        }

      if (!g_output_stream_write_all (task_data->write_pipe, buffer, r,
                                      NULL, cancellable, &error))
        {
          g_prefix_error (&error, "error writing image to self: ");
          break;
        }
    }
  while (r > 0);

  if (error == NULL)
    g_task_return_boolean (task, TRUE);
  else
    g_task_return_error (task, g_steal_pointer (&error));

  gis_scribe_tee_close (task_data, cancellable);
}

static void
gis_scribe_begin_tee (GisScribe          *self,
                      GOutputStream      *gpg_stdin,
                      GOutputStream      *write_pipe,
                      GCancellable       *cancellable,
                      GAsyncReadyCallback callback,
                      gpointer            user_data)
{
  g_autoptr(GTask) task = g_task_new (self, cancellable, callback, user_data);
  GisScribeTeeData *task_data = g_slice_new0 (GisScribeTeeData);
  g_autoptr(GError) error = NULL;

  task_data->gpg_stdin = g_object_ref (gpg_stdin);
  task_data->write_pipe = g_object_ref (write_pipe);

  g_task_set_source_tag (task, GUINT_TO_POINTER (GIS_SCRIBE_TASK_TEE));
  g_task_set_task_data (task, task_data,
                        (GDestroyNotify) gis_scribe_tee_data_free);

  task_data->image_input =
    G_INPUT_STREAM (g_file_read (self->image, cancellable, &error));

  if (task_data->image_input == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      gis_scribe_tee_close (task_data, cancellable);
    }
  else
    {
      g_task_run_in_thread (task, (GTaskThreadFunc) gis_scribe_tee_thread);
    }
}

static void
gis_scribe_decompress_wait_check_cb (GObject      *source,
                                     GAsyncResult *result,
                                     gpointer      data)
{
  g_autoptr(GSubprocess) subprocess = G_SUBPROCESS (source);
  g_autoptr(GTask) task = G_TASK (data);
  g_autoptr(GError) error = NULL;

  if (g_subprocess_wait_check_finish (subprocess, result, &error))
    {
      g_task_return_boolean (task, TRUE);
    }
  else
    {
      g_prefix_error (&error, "decompressor subprocess failed: ");
      g_task_return_error (task, g_steal_pointer (&error));
    }
}


/* Spawns a subprocess to decompress the image. This function returns %TRUE
 * with @compressed and @decompressed set if spawning the subprocess succeeds;
 * and %FALSE with both unset if not. In either case, callback will fire when
 * the subprocess terminates (which may be immediately).
 *
 * The appropriate decompressor is determined from the image file name. If it's
 * uncompressed, @compressed and @decompressed will be the two ends of a
 * pipe-to-self and no subprocess will be launched. (@callback will fire with
 * success.) If the decompressor can't be determined, returns %FALSE and fires
 * @callback with error.
 *
 * @compressed: (out): socket to write compressed image data to
 * @decompressed: (out): socket to read decompressed image data from
 */
static gboolean
gis_scribe_begin_decompress (GisScribe          *self,
                             GOutputStream     **compressed,
                             GInputStream      **decompressed,
                             GCancellable       *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer            user_data)
{
  g_autoptr(GTask) task = g_task_new (self, cancellable, callback, user_data);
  g_autofree gchar *basename = NULL;
  const gchar *args[] = { NULL, "-cd", NULL };
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  GSubprocess *subprocess = NULL;
  g_autoptr(GError) error = NULL;

  g_task_set_source_tag (task, GUINT_TO_POINTER (GIS_SCRIBE_TASK_DECOMPRESS));

  basename = g_file_get_basename (self->image);
  if (basename == NULL)
    {
      /* This path should not be reached since g_file_get_basename() cannot
       * return NULL for local files. To avoid translators translating a
       * technical message which should never be shown, we only mark "Internal
       * error" for translation.
       */
      g_set_error (&error, GIS_INSTALL_ERROR, 0, "%s: %s",
                   _("Internal error"),
                   "g_file_get_basename returned NULL");
      g_critical ("%s", error->message);
      g_task_return_error (task, g_steal_pointer (&error));
      return FALSE;
    }

  /* TODO: use more magical means */
  if (g_str_has_suffix (basename, "gz"))
    {
      args[0] = "gzip";
    }
  else if (g_str_has_suffix (basename, "xz"))
    {
      args[0] = "xz";
    }
  else if (g_str_has_suffix (basename, "img")
           || g_strcmp0 (basename, "endless-image") == 0)
    {
      gint pipefd[2];

      if (!g_unix_open_pipe (pipefd, FD_CLOEXEC, &error))
        {
          g_task_return_error (task, g_steal_pointer (&error));
          return FALSE;
        }

      *compressed = g_unix_output_stream_new (pipefd[1], /* close_fd */ TRUE);
      *decompressed = g_unix_input_stream_new (pipefd[0], /* close_fd */ TRUE);
      g_task_return_boolean (task, TRUE);
      return TRUE;
    }
  else
    {
      g_task_return_new_error (task, GIS_INSTALL_ERROR, 0,
                               "%s ends in neither '.xz', '.gz' nor '.img'",
                               basename);
      return FALSE;
    }

  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                        G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  subprocess = g_subprocess_launcher_spawnv (launcher, args, &error);
  if (subprocess == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return FALSE;
    }

  *compressed = g_object_ref (g_subprocess_get_stdin_pipe (subprocess));
  *decompressed = g_object_ref (g_subprocess_get_stdout_pipe (subprocess));

  g_subprocess_wait_check_async (subprocess, cancellable,
                                 gis_scribe_decompress_wait_check_cb,
                                 g_steal_pointer (&task));

  return TRUE;
}

static void
gis_scribe_subtask_cb (GObject      *source,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  GisScribe *self = GIS_SCRIBE (source);
  g_autoptr(GTask) outer_task = G_TASK (user_data);
  GTask *inner_task = G_TASK (result);
  GisScribeTask task_flag = GPOINTER_TO_INT (g_task_get_source_tag (inner_task));
  const gchar *inner_task_name = gis_scribe_task_get_label (task_flag);
  g_autoptr(GError) error = NULL;

  /* Guard access to self->outstanding_tasks and self->error. */
  g_mutex_lock (&self->mutex);

  if (g_task_propagate_boolean (inner_task, &error))
    {
      g_debug ("%s: %s completed successfully", G_STRFUNC, inner_task_name);
    }
  else if (self->error == NULL)
    {
      g_debug ("%s: saving error from %s to report later: %s", G_STRFUNC,
               inner_task_name, error->message);
      g_propagate_error (&self->error, g_steal_pointer (&error));
    }
  else
    {
      g_debug ("%s: discarding subsequent error from %s: %s", G_STRFUNC,
               inner_task_name, error->message);
      g_clear_error (&error);
    }

  /* This task should be outstanding */
  g_assert_cmpint (self->outstanding_tasks & task_flag, ==, task_flag);
  self->outstanding_tasks &= ~task_flag;

  if (self->outstanding_tasks == 0)
    {
      if (self->error == NULL)
        g_task_return_boolean (outer_task, TRUE);
      else
        /* could steal self->error since all subtasks are now dead but it's
         * useful to know that once set, it remains set until destruction.
         */
        g_task_return_error (outer_task, g_error_copy (self->error));
    }

  /* Alert the write thread, if it's already waiting, that
   * self->outstanding_tasks and self->error have been updated.
   */
  g_cond_signal (&self->cond);
  g_mutex_unlock (&self->mutex);
}

static void
gis_scribe_setpipe_sz (const gchar          *what,
                       GFileDescriptorBased *stream)
{
  int fd = g_file_descriptor_based_get_fd (stream);

  if (fcntl (fd, F_SETPIPE_SZ, BUFFER_SIZE) < 0)
    g_warning ("failed to set %s pipe size to %d: %s",
               what, BUFFER_SIZE, g_strerror (errno));
}

/**
 * gis_scribe_write_async:
 *
 * Begins writing #GisScribe:image to #GisScribe:drive-fd. This may be called
 * at most once on any given #GisScribe object. Once called, the target drive's
 * contents should be considered lost, even if @cancellable is subsequently
 * triggered.
 */
void
gis_scribe_write_async (GisScribe          *self,
                        GCancellable       *cancellable,
                        GAsyncReadyCallback callback,
                        gpointer            user_data)
{
  g_autoptr(GTask) task = g_task_new (self, cancellable, callback, user_data);
  g_autoptr(GInputStream) decompressed = NULL;
  g_autoptr(GOutputStream) write_pipe = NULL;
  g_autoptr(GOutputStream) gpg_stdin = NULL;

  if (self->started)
    {
      g_task_return_new_error (task, GIS_INSTALL_ERROR, 0, "already started");
      return;
    }

  self->started = TRUE;
  self->start_time_usec = g_get_monotonic_time ();

  /* Guard access to self->outstanding_tasks */
  g_mutex_lock (&self->mutex);

  /* Attempt to spawn decompressor subprocess (or pipe-to-self) */
  self->outstanding_tasks |= GIS_SCRIBE_TASK_DECOMPRESS;
  if (!gis_scribe_begin_decompress (self, &write_pipe, &decompressed,
                                    cancellable, gis_scribe_subtask_cb,
                                    g_object_ref (task)))
    {
      goto out;
    }

  /* Attempt to spawn GPG subprocess */
  self->outstanding_tasks |= GIS_SCRIBE_TASK_VERIFY;
  gpg_stdin = gis_scribe_begin_verify (self, cancellable, gis_scribe_subtask_cb,
                                       g_object_ref (task));
  if (gpg_stdin == NULL)
    {
      gis_scribe_close_output_stream_or_warn (write_pipe, cancellable,
                                              "decompressor stdin");
      gis_scribe_close_input_stream_or_warn (decompressed, cancellable,
                                             "decompressor stdout");
      goto out;
    }

  gis_scribe_setpipe_sz ("gpg stdin", G_FILE_DESCRIPTOR_BASED (gpg_stdin));
  gis_scribe_setpipe_sz ("decompressor stdin", G_FILE_DESCRIPTOR_BASED (write_pipe));
  gis_scribe_setpipe_sz ("decompressor stdout", G_FILE_DESCRIPTOR_BASED (decompressed));

  /* Start feeding the image to GPG and to one end of a pipe-to-self */
  self->outstanding_tasks |= GIS_SCRIBE_TASK_TEE;
  gis_scribe_begin_tee (self, gpg_stdin, write_pipe, cancellable,
                        gis_scribe_subtask_cb, g_object_ref (task));

  /* Start reading from the other end of the pipe and writing to disk */
  self->outstanding_tasks |= GIS_SCRIBE_TASK_WRITE;
  gis_scribe_begin_write (self, decompressed, cancellable,
                          gis_scribe_subtask_cb, g_object_ref (task));

out:
  g_mutex_unlock (&self->mutex);
}

/**
 * gis_scribe_write_finish:
 *
 * Completes a call to gis_scribe_write_async().
 */
gboolean
gis_scribe_write_finish (GisScribe    *self,
                         GAsyncResult *result,
                         GError      **error)
{
  GTask *task = G_TASK (result);

  return g_task_propagate_boolean (task, error);
}

/**
 * gis_scribe_get_step:
 *
 * Returns: the #GisScribe:step property.
 */
guint
gis_scribe_get_step (GisScribe *self)
{
  g_return_val_if_fail (GIS_IS_SCRIBE (self), 0);

  return self->step;
}

/**
 * gis_scribe_get_progress:
 *
 * Returns: the #GisScribe:progress property.
 */
gdouble
gis_scribe_get_progress (GisScribe *self)
{
  g_return_val_if_fail (GIS_IS_SCRIBE (self), -1);

  return self->overall_progress;
}
