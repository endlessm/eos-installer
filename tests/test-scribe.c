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

#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <glib.h>
#include <gio/gunixoutputstream.h>

#include "gis-scribe.h"

/* A 4 MiB file of "w"s (0x77) */
#define IMAGE "w.img"
#define IMAGE_BYTE 'w'

#define IMAGE_SIZE_BYTES 4 * 1024 * 1024

static gchar *keyring_path = NULL;

typedef struct {
  const gchar *image_path;
  const gchar *signature_path;
} TestData;

typedef struct {
  const TestData *data;

  GThread *main_thread;

  gchar *tmpdir;
  GFile *image;
  GFile *signature;
  gchar *target_path;
  GFile *target;

  GisScribe *scribe;
  GCancellable *cancellable;
  GMainLoop *loop;

  guint step;
  gdouble progress;

  gboolean finished;
  GError *error;
} Fixture;

static void
test_scribe_notify_step_cb (GObject    *object,
                            GParamSpec *pspec,
                            gpointer    user_data)
{
  GisScribe *scribe = GIS_SCRIBE (object);
  Fixture *fixture = user_data;
  guint step = gis_scribe_get_step (scribe);

  g_assert (fixture->main_thread == g_thread_self ());

  g_assert_cmpfloat (1, <=, step);
  g_assert_cmpfloat (step, <=, 2);
  g_assert_cmpfloat (fixture->step, <=, step);

  if (step != fixture->step)
    {
      fixture->step = step;
      fixture->progress = -1;
    }
}

static void
test_scribe_notify_progress_cb (GObject    *object,
                                GParamSpec *pspec,
                                gpointer    user_data)
{
  GisScribe *scribe = GIS_SCRIBE (object);
  Fixture *fixture = user_data;
  gdouble progress = gis_scribe_get_progress (scribe);

  g_assert (fixture->main_thread == g_thread_self ());

  if (fixture->step != 2 || progress != -1)
    {
      g_assert_cmpfloat (0, <=, progress);
      g_assert_cmpfloat (progress, <=, 1);
      g_assert_cmpfloat (fixture->progress, <=, progress);
    }

  fixture->progress = progress;
}

static void
fixture_set_up (Fixture *fixture,
                gconstpointer user_data)
{
  const TestData *data = user_data;
  g_autofree gchar *target_contents = g_malloc (IMAGE_SIZE_BYTES);
  GError *error = NULL;
  int fd;

  fixture->data = data;
  fixture->main_thread = g_thread_ref (g_thread_self ());

  fixture->tmpdir = g_dir_make_tmp ("eos-installer.XXXXXX", &error);
  g_assert_no_error (error);
  g_assert (fixture->tmpdir != NULL);

  fixture->image = g_file_new_for_path (data->image_path);
  fixture->signature = g_file_new_for_path (data->signature_path);

  fixture->target_path = g_build_filename (fixture->tmpdir, "target.img", NULL);
  fixture->target = g_file_new_for_path (fixture->target_path);

  memset (target_contents, 'D', IMAGE_SIZE_BYTES);
  g_file_set_contents (fixture->target_path, target_contents, IMAGE_SIZE_BYTES,
                       &error);
  g_assert_no_error (error);

  fd = open (fixture->target_path, O_WRONLY | O_SYNC | O_CLOEXEC | O_EXCL);
  g_assert (fd >= 0);
  fixture->scribe = g_object_new (GIS_TYPE_SCRIBE,
                                  "image", fixture->image,
                                  "image-size", IMAGE_SIZE_BYTES,
                                  "signature", fixture->signature,
                                  "keyring-path", keyring_path,
                                  "drive-path", fixture->target_path,
                                  "drive-fd", fd,
                                  NULL);
  g_signal_connect (fixture->scribe, "notify::step",
                    (GCallback) test_scribe_notify_step_cb, fixture);
  g_signal_connect (fixture->scribe, "notify::progress",
                    (GCallback) test_scribe_notify_progress_cb, fixture);

  fixture->cancellable = g_cancellable_new ();
  fixture->loop = g_main_loop_new (NULL, FALSE);
}

static void
rm_r (const gchar *path)
{
  g_autoptr(GFile) file = g_file_new_for_path (path);
  g_autoptr(GFileEnumerator) enumerator = NULL;
  GFile *child = NULL;
  GError *error = NULL;

  enumerator = g_file_enumerate_children (
      file, NULL, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, &error);
  g_assert_no_error (error);

  while (g_file_enumerator_iterate (enumerator, NULL, &child, NULL, &error) &&
         child != NULL)
    {
      if (!g_file_delete (child, NULL, &error))
        {
          g_autofree gchar *child_path = g_file_get_path (child);
          g_warning ("Failed to delete %s: %s", child_path, error->message);
          g_clear_error (&error);
        }
    }

  if (error != NULL)
    {
      g_warning ("Error while enumerating %s: %s", path, error->message);
      g_clear_error (&error);
    }

  if (!g_file_delete (file, NULL, &error))
    {
      g_warning ("Failed to delete %s: %s", path, error->message);
      g_clear_error (&error);
    }
}

static void
fixture_tear_down (Fixture *fixture,
                   gconstpointer user_data)
{
  g_signal_handlers_disconnect_by_data (fixture->scribe, fixture);
  g_cancellable_cancel (fixture->cancellable);
  g_clear_object (&fixture->cancellable);
  g_clear_object (&fixture->scribe);
  g_clear_object (&fixture->image);
  g_clear_object (&fixture->signature);
  g_clear_pointer (&fixture->target_path, g_free);
  g_clear_object (&fixture->target);
  g_clear_pointer (&fixture->loop, g_main_loop_unref);
  g_clear_pointer (&fixture->main_thread, g_thread_unref);

  rm_r (fixture->tmpdir);
  g_clear_pointer (&fixture->tmpdir, g_free);
}

static void
test_noop (Fixture *fixture,
           gconstpointer user_data)
{
}

static void
test_scribe_write_cb (GObject      *source,
                      GAsyncResult *result,
                      gpointer      data)
{
  GisScribe *scribe = GIS_SCRIBE (source);
  Fixture *fixture = data;
  gboolean ret;

  g_assert_false (fixture->finished);
  g_assert_no_error (fixture->error);

  fixture->finished = TRUE;

  ret = gis_scribe_write_finish (scribe, result, &fixture->error);
  if (ret)
    g_assert_no_error (fixture->error);
  else
    g_assert (fixture->error != NULL);

  g_main_loop_quit (fixture->loop);
}

static void
test_bad_signature (Fixture       *fixture,
                    gconstpointer  user_data)
{
  gis_scribe_write_async (fixture->scribe, fixture->cancellable,
                          test_scribe_write_cb, fixture);
  g_main_loop_run (fixture->loop);

  g_assert (fixture->finished);
  g_assert_error (fixture->error,
                  GIS_INSTALL_ERROR,
                  0); // TODO
}

static void
test_write_success (Fixture       *fixture,
                    gconstpointer  user_data)
{
  gboolean ret;
  g_autofree gchar *target_contents = NULL;
  gsize target_length = 0;
  g_autofree gchar *expected_contents = g_malloc (IMAGE_SIZE_BYTES);
  GError *error = NULL;

  gis_scribe_write_async (fixture->scribe, fixture->cancellable,
                          test_scribe_write_cb, fixture);
  g_main_loop_run (fixture->loop);

  g_assert (fixture->finished);
  g_assert_no_error (fixture->error);

  ret = g_file_get_contents (fixture->target_path,
                             &target_contents, &target_length,
                             &error);
  g_assert_no_error (error);
  g_assert (ret);

  memset (expected_contents, IMAGE_BYTE, IMAGE_SIZE_BYTES);
  g_assert_cmpmem (expected_contents, IMAGE_SIZE_BYTES,
                   target_contents, target_length);
}

static gchar *
test_build_filename (GTestFileType file_type,
                     const gchar  *basename)
{
  gchar *filename = g_test_build_filename (file_type, basename, NULL);

  if (!g_file_test (filename, G_FILE_TEST_EXISTS))
    g_error ("test data file %s doesn't exist", filename);

  return filename;
}

int
main (int argc, char *argv[])
{
  g_autofree gchar *image_path = NULL;
  g_autofree gchar *image_sig_path = NULL;
  g_autofree gchar *image_gz_path = NULL;
  g_autofree gchar *image_gz_sig_path = NULL;
  g_autofree gchar *image_xz_path = NULL;
  g_autofree gchar *image_xz_sig_path = NULL;
  g_autofree gchar *wjt_sig_path = NULL;
  int ret;

  setlocale (LC_ALL, "");

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("https://phabricator.endlessm.com/");

  /* Autofreed locals */
  image_path        = test_build_filename (G_TEST_BUILT, IMAGE);
  image_sig_path    = test_build_filename (G_TEST_BUILT, IMAGE ".asc");
  image_gz_path     = test_build_filename (G_TEST_BUILT, IMAGE ".gz");
  image_gz_sig_path = test_build_filename (G_TEST_BUILT, IMAGE ".gz.asc");
  image_xz_path     = test_build_filename (G_TEST_BUILT, IMAGE ".xz");
  image_xz_sig_path = test_build_filename (G_TEST_BUILT, IMAGE ".xz.asc");
  wjt_sig_path      = test_build_filename (G_TEST_DIST, "wjt.asc");

  /* Globals */
  keyring_path = test_build_filename (G_TEST_DIST, "public.asc");

  /* ======== TESTS COMMENCE ======== */

  /* Verify that the setup/teardown works */
  TestData noop_data = {
      .image_path = image_path,
      .signature_path = image_sig_path,
  };
  g_test_add ("/scribe/noop", Fixture, &noop_data,
              fixture_set_up,
              test_noop,
              fixture_tear_down);

  /* 'image_gz_sig_path' is a signature made by the trusted (test) key, but it
   * does not match the file at 'image_path'.
   */
  TestData bad_signature = {
      .image_path = image_path,
      .signature_path = image_gz_sig_path,
  };
  g_test_add ("/scribe/bad-signature",
              Fixture, &bad_signature,
              fixture_set_up,
              test_bad_signature,
              fixture_tear_down);

  /* wjt_sig_path is a valid signature made by a key that's not in the keyring.
   */
  TestData untrusted_signature = {
      .image_path = image_path,
      .signature_path = wjt_sig_path,
  };
  g_test_add ("/scribe/untrusted-signature",
              Fixture, &untrusted_signature,
              fixture_set_up,
              test_bad_signature,
              fixture_tear_down);

  /* Valid signature for an uncompressed image */
  TestData good_signature = {
      .image_path = image_path,
      .signature_path = image_sig_path,
  };
  g_test_add ("/scribe/good-signature-img", Fixture, &good_signature,
              fixture_set_up,
              test_write_success,
              fixture_tear_down);

  ret = g_test_run ();

  g_free (keyring_path);

  return ret;
}
