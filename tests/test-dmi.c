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

#include "gis-dmi.h"

static const gchar * const test_sanitize_data[][2] = {
    /* Strips trailing whitespace */
    { "Foo\n", "Foo" },
    /* Strips trailing *printable* whitespace */
    { "Foo ", "Foo" },
    /* Preserves inner whitespace */
    { "Foo bar", "Foo bar" },
    /* Strips non-printable-ASCII characters */
    { "\x01\x02No \x03\n thanks\x04\x7f", "No  thanks" },
    /* Strips characters outside the ASCII range */
    { "UTF-8 Snowman Computers Ltd.\xE2\x98\x83",
      "UTF-8 Snowman Computers Ltd." },
    /* Canonicalizes empty string to NULL */
    { "", NULL },
    /* Canonicalizes empty string (after sanitization) to NULL */
    { "   ", NULL },
    { "\x01", NULL },
};

static void
test_sanitize (gconstpointer test_data_ptr)
{
  const gchar * const *test_data = test_data_ptr;
  const gchar *raw = test_data[0];
  const gchar *expected_sanitized = test_data[1];
  g_autofree gchar *sanitized = gis_dmi_sanitize_string (raw);

  g_assert_cmpstr (expected_sanitized, ==, sanitized);
}

int
main (int argc, char *argv[])
{
  size_t i;

  setlocale (LC_ALL, "");

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("https://phabricator.endlessm.com/");

  for (i = 0; i < G_N_ELEMENTS (test_sanitize_data); i++)
    {
      g_autofree gchar *testpath =
        g_strdup_printf ("/dmi/sanitize/%" G_GSIZE_FORMAT, i);
      g_test_add_data_func (testpath, test_sanitize_data[i], test_sanitize);
    }

  return g_test_run ();
}
