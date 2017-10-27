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

#define IMAGE_KEYRING "/usr/share/keyrings/eos-image-keyring.gpg"

typedef struct _GisScribe {
  GObject parent;

  GFile *image;
  guint64 image_size;
  GFile *signature;
  gchar *keyring_path;

  gboolean started;
  gdouble progress;

  GPid gpg_pid;
  GIOChannel *gpg_stdout;
  guint gpg_watch;
} GisScribe;

G_DEFINE_QUARK (install-error, gis_install_error)

G_DEFINE_TYPE (GisScribe, gis_scribe, G_TYPE_OBJECT)

typedef enum {
  PROP_IMAGE = 1,
  PROP_IMAGE_SIZE,
  PROP_SIGNATURE,
  PROP_KEYRING_PATH,
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

  g_clear_pointer (&self->keyring_path, g_free);

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
}

GisScribe *
gis_scribe_new (GFile  *image,
                guint64 image_size,
                GFile  *signature)
{
  g_return_val_if_fail (G_IS_FILE (image), NULL);
  g_return_val_if_fail (image_size > 0, NULL);
  g_return_val_if_fail (G_IS_FILE (signature), NULL);

  return g_object_new (
      GIS_TYPE_SCRIBE,
      "image", image,
      "image-size", image_size,
      "signature", signature,
      NULL);
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
      g_task_return_boolean (task, TRUE);
      // TODO
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

gdouble
gis_scribe_get_progress (GisScribe *self)
{
  g_return_val_if_fail (GIS_IS_SCRIBE (self), -1);

  return self->progress;
}
