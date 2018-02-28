/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright © 2018 Endless Mobile, Inc.
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
#include <string.h>
#include <locale.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "gis-write-diagnostics.h"
#include "glnx-fdio.h"
#include "glnx-shutil.h"

typedef struct {
  gchar *tmpdir;
  gchar *image_dir;
  gchar *home_dir;

  GFile *output;
  GError *error;
} Fixture;

static void
fixture_set_up (Fixture *fixture,
                gconstpointer user_data)
{
  GError *error = NULL;

  fixture->tmpdir = g_dir_make_tmp ("eos-installer.XXXXXX", &error);
  g_assert_no_error (error);
  g_assert (fixture->tmpdir != NULL);
}

static void
fixture_tear_down (Fixture *fixture,
                   gconstpointer user_data)
{
  g_autoptr(GError) error = NULL;

  if (!glnx_shutil_rm_rf_at (AT_FDCWD, fixture->tmpdir, NULL, &error))
    g_warning ("Failed to remove %s: %s", fixture->tmpdir, error->message);

  g_clear_pointer (&fixture->tmpdir, g_free);
  g_clear_pointer (&fixture->image_dir, g_free);
  g_clear_pointer (&fixture->home_dir, g_free);

  g_clear_object (&fixture->output);
  g_clear_error (&fixture->error);
}

static void
write_unattended_config_cb (GObject      *source,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  GAsyncResult **result_out = user_data;

  g_assert_null (*result_out);
  *result_out = g_object_ref (result);
}

static void
call_and_wait (Fixture *fixture)
{
  g_autoptr(GFile) image_dir = NULL;
  g_autoptr(GAsyncResult) result = NULL;
  g_autofree gchar *contents = NULL;

  if (fixture->image_dir != NULL)
    image_dir = g_file_new_for_path (fixture->image_dir);

  /* We override the executable path with "echo" so we can check the arguments
   * passed to it below.
   */
  gis_write_diagnostics_async ("echo", image_dir, fixture->home_dir,
                               NULL, write_unattended_config_cb, &result);

  while (result == NULL)
    g_main_context_iteration (NULL, TRUE);

  fixture->output = gis_write_diagnostics_finish (result, &fixture->error);

  if (fixture->output != NULL)
    {
      g_assert_no_error (fixture->error);

      gboolean ret = g_file_load_contents (fixture->output, NULL, &contents,
                                           NULL, NULL, &fixture->error);
      g_assert_no_error (fixture->error);
      g_assert_true (ret);
      /* eos-diagnostics accepts an optional target filename. Rather than
       * following the normal UNIX convention of "-" meaning "write to stdout",
       * it treats the argument "stdout" that way. The trailing newline is
       * added by /bin/echo.
       */
      g_assert_cmpstr (contents, ==, "stdout\n");
    }
}

static inline void
assert_file_in_dir (GFile       *file,
                    const gchar *dir)
{
  g_autoptr(GFile) parent = g_file_get_parent (file);
  g_autofree gchar *parent_path = g_file_get_path (parent);

  g_assert_cmpstr (parent_path, ==, dir);
}

static void
test_no_dirs (Fixture       *fixture,
              gconstpointer  user_data)
{
  call_and_wait (fixture);
  g_assert_null (fixture->output);
  g_assert_no_error (fixture->error);
}

static void
test_image_dir_ok (Fixture      *fixture,
                   gconstpointer user_data)
{
  fixture->image_dir = g_build_filename (fixture->tmpdir, "imagedir", NULL);
  glnx_ensure_dir (AT_FDCWD, fixture->image_dir, 0755, &fixture->error);
  g_assert_no_error (fixture->error);

  call_and_wait (fixture);
  g_assert_nonnull (fixture->output);
  g_assert_no_error (fixture->error);
  assert_file_in_dir (fixture->output, fixture->image_dir);
}

static void
test_image_dir_error (Fixture      *fixture,
                      gconstpointer user_data)
{
  fixture->image_dir = g_build_filename (fixture->tmpdir, "imagedir", NULL);

  call_and_wait (fixture);
  g_assert_null (fixture->output);
  g_assert_error (fixture->error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
}

static void
test_home_dir_ok (Fixture      *fixture,
                   gconstpointer user_data)
{
  fixture->home_dir = g_build_filename (fixture->tmpdir, "homedir", NULL);
  glnx_ensure_dir (AT_FDCWD, fixture->home_dir, 0755, &fixture->error);
  g_assert_no_error (fixture->error);

  call_and_wait (fixture);
  g_assert_nonnull (fixture->output);
  g_assert_no_error (fixture->error);
  assert_file_in_dir (fixture->output, fixture->home_dir);
}

static void
test_home_dir_error (Fixture      *fixture,
                     gconstpointer user_data)
{
  fixture->home_dir = g_build_filename (fixture->tmpdir, "homedir", NULL);

  call_and_wait (fixture);
  g_assert_null (fixture->output);
  g_assert_error (fixture->error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
}

static void
test_image_dir_preferred (Fixture      *fixture,
                          gconstpointer user_data)
{
  fixture->image_dir = g_build_filename (fixture->tmpdir, "imagedir", NULL);
  glnx_ensure_dir (AT_FDCWD, fixture->image_dir, 0755, &fixture->error);
  g_assert_no_error (fixture->error);

  fixture->home_dir = g_build_filename (fixture->tmpdir, "homedir", NULL);
  glnx_ensure_dir (AT_FDCWD, fixture->home_dir, 0755, &fixture->error);
  g_assert_no_error (fixture->error);

  call_and_wait (fixture);
  g_assert_nonnull (fixture->output);
  g_assert_no_error (fixture->error);
  assert_file_in_dir (fixture->output, fixture->image_dir);
}

static void
test_fall_back_to_image_dir (Fixture      *fixture,
                             gconstpointer user_data)
{
  fixture->image_dir = g_build_filename (fixture->tmpdir, "imagedir", NULL);
  /* don't create it */

  fixture->home_dir = g_build_filename (fixture->tmpdir, "homedir", NULL);
  glnx_ensure_dir (AT_FDCWD, fixture->home_dir, 0755, &fixture->error);
  g_assert_no_error (fixture->error);

  call_and_wait (fixture);
  g_assert_nonnull (fixture->output);
  g_assert_no_error (fixture->error);
  assert_file_in_dir (fixture->output, fixture->home_dir);
}

static void
test_both_error (Fixture      *fixture,
                 gconstpointer user_data)
{
  fixture->image_dir = g_build_filename (fixture->tmpdir, "imagedir", NULL);
  /* don't create it */

  fixture->home_dir = g_build_filename (fixture->tmpdir, "homedir", NULL);
  /* create it as a non-directory. This allows us to make assertions about
   * whether the first or second error is returned.
   */
  g_file_set_contents (fixture->home_dir, "ceci n'est pas un directoire", -1,
                       &fixture->error);
  g_assert_no_error (fixture->error);

  call_and_wait (fixture);
  g_assert_null (fixture->output);
  g_assert_error (fixture->error, G_IO_ERROR, G_IO_ERROR_NOT_DIRECTORY);
}

static void
test_command_fails (Fixture      *fixture,
                    gconstpointer user_data)
{
  g_autoptr(GAsyncResult) result = NULL;

  gis_write_diagnostics_async ("false", NULL, NULL, NULL,
                               write_unattended_config_cb, &result);

  while (result == NULL)
    g_main_context_iteration (NULL, TRUE);

  fixture->output = gis_write_diagnostics_finish (result, &fixture->error);
  g_assert_error (fixture->error, G_SPAWN_EXIT_ERROR, 1);
  g_assert_null (fixture->output);
}

int
main (int argc, char *argv[])
{
  setlocale (LC_ALL, "");

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("https://phabricator.endlessm.com/");

#define TEST(name, func) \
  g_test_add ("/write-diagnostics/" name, Fixture, NULL, \
              fixture_set_up, func, fixture_tear_down)

  TEST ("no-dirs", test_no_dirs);
  TEST ("image-dir-ok", test_image_dir_ok);
  TEST ("image-dir-error", test_image_dir_error);
  TEST ("home-dir-ok", test_home_dir_ok);
  TEST ("home-dir-error", test_home_dir_error);
  TEST ("image-dir-preferred", test_image_dir_preferred);
  TEST ("fall-back-to-image-dir", test_fall_back_to_image_dir);
  TEST ("both-error", test_both_error);
  TEST ("command-fails", test_command_fails);

#undef TEST

  return g_test_run ();
}
