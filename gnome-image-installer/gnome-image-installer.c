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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Original code written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#include "config.h"

#include "gnome-image-installer.h"

#include <pwd.h>
#include <unistd.h>
#include <stdlib.h>
#include <glib/gi18n.h>
#include <udisks/udisks.h>

#include "pages/confirm/gis-confirm-page.h"
#include "pages/diskimage/gis-diskimage-page.h"
#include "pages/disktarget/gis-disktarget-page.h"
#include "pages/finished/gis-finished-page.h"
#include "pages/install/gis-install-page.h"

#include "util/gis-store.h"

/* main {{{1 */

static gboolean force_new_user_mode;

typedef void (*PreparePage) (GisDriver *driver);

typedef struct {
    const gchar *page_id;
    PreparePage prepare_page_func;
} PageData;

#define PAGE(name) { #name, gis_prepare_ ## name ## _page }

static PageData page_table[] = {
  PAGE (diskimage),
  PAGE (disktarget),
  PAGE (confirm),
  PAGE (install),
  PAGE (finished),
  { NULL },
};

#undef PAGE

#define EOS_GROUP "EndlessOS"
#define LOCALE_KEY "locale"

static void
destroy_pages_after (GisAssistant *assistant,
                     GisPage      *page)
{
  GList *pages, *l, *next;

  pages = gis_assistant_get_all_pages (assistant);

  for (l = pages; l != NULL; l = l->next)
    if (l->data == page)
      break;

  /* This function is called with the current page, so it should certainly be
   * found in list of pages.
   */
  g_return_if_fail (l != NULL);
  l = l->next;
  for (; l != NULL; l = next) {
    next = l->next;
    gtk_widget_destroy (GTK_WIDGET (l->data));
  }
}

static void
read_unattended_ini (const gchar *path)
{
  g_autofree gchar *ini = g_build_path ("/", path, "unattended.ini", NULL);
  g_autoptr(GisUnattendedConfig) config = NULL;
  g_autoptr(GError) error = NULL;

  config = gis_unattended_config_new (ini, &error);
  if (error != NULL &&
      !g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
    {
      /* Translators: the placeholder is the name of a configuration file, such
       * as "/run/mount/eosimages/unattended.ini". */
      g_prefix_error (&error, _("Error loading ‘%s’: "), ini);
      gis_store_set_error (error);
    }
  else if (config != NULL)
    {
      /* Locale is handled in gnome-initial-setup.
       * The rest of the magic happens as we go along: each page gleans the
       * relevant facts from the config.
       */
      gis_store_enter_unattended (config);
    }
}

static void
mount_and_read_unattended_ini (void)
{
  UDisksClient *client = UDISKS_CLIENT (gis_store_get_object (GIS_STORE_UDISKS_CLIENT));
  GDBusObjectManager *manager = udisks_client_get_object_manager(client);
  GList *objects = g_dbus_object_manager_get_objects(manager);
  GList *l;

  for (l = objects; l != NULL; l = l->next)
    {
      UDisksObject *object = UDISKS_OBJECT (l->data);
      UDisksBlock *block = udisks_object_peek_block (object);
      UDisksFilesystem *fs = NULL;
      const gchar *const*mounts = NULL;
      g_autofree gchar *path_to_free = NULL;
      const gchar *path = NULL;

      if (block == NULL)
        continue;

      if (!g_str_equal ("eosimages", udisks_block_get_id_label (block)))
        continue;

      fs = udisks_object_peek_filesystem (object);

      if (fs == NULL)
        {
          continue;
        }

      mounts = udisks_filesystem_get_mount_points (fs);

      if (mounts != NULL && mounts[0] != NULL)
        path = mounts[0];
      else if (udisks_filesystem_call_mount_sync (fs, g_variant_new ("a{sv}", NULL),
                                                  &path_to_free, NULL, NULL))
        path = path_to_free;

      if (path != NULL)
        {
          read_unattended_ini (path);
        }

      break;
    }
}

/* Should be kept in sync with gnome-initial-setup gis-driver.c */
static gboolean
check_for_live_boot (gchar **uuid)
{
  const gchar *force = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *cmdline = NULL;
  gboolean live_boot = FALSE;
  g_autoptr(GRegex) reg = NULL;
  g_autoptr(GMatchInfo) info = NULL;

  g_return_val_if_fail (uuid != NULL, FALSE);

  force = g_getenv ("EI_FORCE_LIVE_BOOT_UUID");
  if (force != NULL && *force != '\0')
    {
      g_message ("EI_FORCE_LIVE_BOOT_UUID set to %s", force);
      *uuid = g_strdup (force);
      return TRUE;
    }

  if (!g_file_get_contents ("/proc/cmdline", &cmdline, NULL, &error))
    {
      g_warning ("unable to read /proc/cmdline: %s", error->message);
      return FALSE;
    }

  live_boot = g_regex_match_simple ("\\bendless\\.live_boot\\b", cmdline, 0, 0);

  g_debug ("set live_boot to %u from /proc/cmdline: %s", live_boot, cmdline);

  reg = g_regex_new ("\\bendless\\.image\\.device=UUID=([^\\s]*)", 0, 0, NULL);
  g_regex_match (reg, cmdline, 0, &info);
  if (g_match_info_matches (info))
    {
      *uuid = g_match_info_fetch (info, 1);
      g_debug ("set UUID to %s", *uuid);
    }

  return live_boot;
}

static void
rebuild_pages_cb (GisDriver *driver)
{
  PageData *page_data = page_table;
  GisAssistant *assistant;
  GisPage *current_page;

  assistant = gis_driver_get_assistant (driver);
  current_page = gis_assistant_get_current_page (assistant);

  if (current_page != NULL) {
    destroy_pages_after (assistant, current_page);

    for (page_data = page_table; page_data->page_id != NULL; ++page_data)
      if (g_str_equal (page_data->page_id, GIS_PAGE_GET_CLASS (current_page)->page_id))
        break;

    ++page_data;
  }

  for (; page_data->page_id != NULL; ++page_data)
      page_data->prepare_page_func (driver);

  gis_assistant_locale_changed (assistant);
}

static gboolean
is_running_as_user (const gchar *username)
{
  struct passwd pw, *pwp;
  char buf[4096];

  getpwnam_r (username, &pw, buf, sizeof (buf), &pwp);
  if (pwp == NULL)
    return FALSE;

  return pw.pw_uid == getuid ();
}

static GisDriverMode
get_mode (void)
{
  if (force_new_user_mode)
    return GIS_DRIVER_MODE_NEW_USER;
  else if (is_running_as_user ("gnome-initial-setup"))
    return GIS_DRIVER_MODE_NEW_USER;
  else
    return GIS_DRIVER_MODE_EXISTING_USER;
}

int
main (int argc, char *argv[])
{
  GisDriver *driver;
  int status;
  GOptionContext *context;
  gchar *uuid = NULL;
  UDisksClient *udisks_client = NULL;
  GError *error = NULL;

  GOptionEntry entries[] = {
    { "force-new-user", 0, 0, G_OPTION_ARG_NONE, &force_new_user_mode,
      _("Force new user mode"), NULL },
    { NULL }
  };

  context = g_option_context_new (_("- GNOME initial setup"));
  g_option_context_add_main_entries (context, entries, NULL);

  g_option_context_parse (context, &argc, &argv, NULL);

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  gtk_init (&argc, &argv);

  if (check_for_live_boot (&uuid))
    {
      gis_store_enter_live_install ();
      gis_store_set_image_uuid (uuid);
    }

  udisks_client = udisks_client_new_sync (NULL, &error);
  if (udisks_client == NULL)
    g_error ("Failed to connect to UDisks: %s", error->message);
  gis_store_set_object (GIS_STORE_UDISKS_CLIENT, G_OBJECT (udisks_client));
  g_object_unref (udisks_client);
  mount_and_read_unattended_ini ();

  driver = gis_driver_new (get_mode ());
  g_signal_connect (driver, "rebuild-pages", G_CALLBACK (rebuild_pages_cb), NULL);
  status = g_application_run (G_APPLICATION (driver), argc, argv);

  g_object_unref (driver);
  g_option_context_free (context);

  return status;
}

/* Epilogue {{{1 */
/* vim: set foldmethod=marker: */
