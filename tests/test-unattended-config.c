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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */
#include "config.h"
#include <locale.h>
#include <string.h>
#include <glib/gstdio.h>

#include "gis-unattended-config.h"

typedef struct {
    gchar *tmpdir;
} Fixture;

static void
fixture_set_up (Fixture *fixture,
                gconstpointer user_data)
{
  g_autoptr(GError) error = NULL;

  fixture->tmpdir = g_dir_make_tmp ("eos-installer.XXXXXX", &error);
  g_assert_no_error (error);
  g_assert_nonnull (fixture->tmpdir);
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
  rm_r (fixture->tmpdir);
  g_clear_pointer (&fixture->tmpdir, g_free);
}

static void
test_parse_empty (void)
{
  g_autofree gchar *empty_ini =
    g_test_build_filename (G_TEST_DIST, "unattended/empty.ini", NULL);
  g_autoptr(GisUnattendedConfig) config = NULL;
  g_autoptr(GError) error = NULL;

  config = gis_unattended_config_new (empty_ini, &error);
  g_assert_no_error (error);
  g_assert_nonnull (config);

  g_assert_cmpstr (gis_unattended_config_get_locale (config), ==, NULL);
}

static void
test_parse_full (void)
{
  g_autofree gchar *full_ini =
    g_test_build_filename (G_TEST_DIST, "unattended/full.ini", NULL);
  g_autoptr(GisUnattendedConfig) config = NULL;
  g_autoptr(GError) error = NULL;

  config = gis_unattended_config_new (full_ini, &error);
  g_assert_no_error (error);
  g_assert_nonnull (config);

  g_assert_cmpstr (gis_unattended_config_get_locale (config), ==, "pt_BR.utf8");
}

static void
test_parse_malformed (void)
{
  /* This source file is a perfectly good non-keyfile! */
  g_autofree gchar *not_ini =
    g_test_build_filename (G_TEST_DIST, "test-unattended-config.c", NULL);
  g_autoptr(GisUnattendedConfig) config = NULL;
  g_autoptr(GError) error = NULL;

  config = gis_unattended_config_new (not_ini, &error);
  g_assert_error (error,
                  GIS_UNATTENDED_ERROR,
                  GIS_UNATTENDED_ERROR_READ);
  g_assert_null (config);
}

static void
test_parse_noent (void)
{
  g_autofree gchar *ini =
    g_test_build_filename (G_TEST_DIST, "does-not-exist", NULL);
  g_autoptr(GisUnattendedConfig) config = NULL;
  g_autoptr(GError) error = NULL;

  config = gis_unattended_config_new (ini, &error);
  /* We wrap most errors in our own GIS_UNATTENDED_ERROR domain to simplify
   * error reporting on the final page, but leave NOENT untouched since it is
   * not an error for the application as a whole.
   */
  g_assert_error (error,
                  G_FILE_ERROR,
                  G_FILE_ERROR_NOENT);
  g_assert_null (config);
}

static void
test_parse_unreadable (Fixture *fixture,
                       gconstpointer data)
{
  g_autoptr(GisUnattendedConfig) config = NULL;
  g_autoptr(GError) error = NULL;

  /* The test tmpdir is just a convenient path that exists, but isn't a file */
  config = gis_unattended_config_new (fixture->tmpdir, &error);
  g_assert_error (error,
                  GIS_UNATTENDED_ERROR,
                  GIS_UNATTENDED_ERROR_READ);
  g_assert_null (config);
}

static void
test_parse_non_utf8_locale (void)
{
  g_autofree gchar *non_utf8_locale =
    g_test_build_filename (G_TEST_DIST, "unattended/non-utf8-locale.ini", NULL);
  g_autoptr(GisUnattendedConfig) config = NULL;
  g_autoptr(GError) error = NULL;

  config = gis_unattended_config_new (non_utf8_locale, &error);
  g_assert_error (error,
                  GIS_UNATTENDED_ERROR,
                  GIS_UNATTENDED_ERROR_READ);
  g_assert_null (config);
}

int
main (int argc, char *argv[])
{
  setlocale (LC_ALL, "");

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("https://phabricator.endlessm.com/");

  g_test_add_func ("/unattended-config/empty", test_parse_empty);
  g_test_add_func ("/unattended-config/full", test_parse_full);
  g_test_add_func ("/unattended-config/malformed", test_parse_malformed);
  g_test_add_func ("/unattended-config/noent", test_parse_noent);
  g_test_add ("/unattended-config/unreadable", Fixture, NULL, fixture_set_up,
              test_parse_unreadable, fixture_tear_down);
  g_test_add_func ("/unattended-config/non-utf8-locale", test_parse_non_utf8_locale);

  return g_test_run ();
}
