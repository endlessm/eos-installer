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

#include "config.h"

#include "gnome-image-installer.h"

#include <pwd.h>
#include <unistd.h>
#include <stdlib.h>
#include <glib/gi18n.h>
#include <udisks/udisks.h>

#include "pages/language/cc-common-language.h"
#include "pages/language/gis-language-page.h"
#include "pages/keyboard/gis-keyboard-page.h"
#include "pages/display/gis-display-page.h"
#include "pages/endless-eula/gis-endless-eula-page.h"
#include "pages/eulas/gis-eula-pages.h"
#include "pages/network/gis-network-page.h"
#include "pages/account/gis-account-page.h"
#include "pages/location/gis-location-page.h"
#include "pages/goa/gis-goa-page.h"
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
  PAGE (language),
  PAGE (diskimage),
  PAGE (disktarget),
  PAGE (install),
  PAGE (finished),
  { NULL },
};

#undef PAGE

#define EOS_GROUP "EndlessOS"
#define UNATTENDED_GROUP "Unattended"
#define LOCALE_KEY "locale"
#define VENDOR_KEY "vendor"
#define PRODUCT_KEY "product"

static void
destroy_pages_after (GisAssistant *assistant,
                     GisPage      *page)
{
  GList *pages, *l, *next;

  pages = gis_assistant_get_all_pages (assistant);

  for (l = pages; l != NULL; l = l->next)
    if (l->data == page)
      break;

  l = l->next;
  for (; l != NULL; l = next) {
    next = l->next;
    gtk_widget_destroy (GTK_WIDGET (l->data));
  }
}

static gchar*
sanitize_string (gchar *string)
{
  gchar *r = string;
  gchar *w = string;

  if (string == NULL)
    return NULL;

  for (;*r != '\0'; r++)
    {
      if (*r < 32 || *r > 126)
        continue;
      *w = *r;
      w++;
    }
  *w = '\0';
  return g_ascii_strdown(string, -1);
}

static gchar *
read_sanitized_string (const gchar *filename,
                       GError     **error)
{
  gchar *contents = NULL;
  gchar *sanitized = NULL;

  if (g_file_get_contents (filename, &contents, NULL, error))
    {
      sanitized = sanitize_string (contents);
      g_free (contents);
    }
  else
    {
      g_prefix_error (error, "failed to read %s", filename);
    }

  return sanitized;
}

static void
read_keys (const gchar *path)
{
  gchar *ini = g_build_path ("/", path, "install.ini", NULL);
  GKeyFile *keys = g_key_file_new();

  if (g_key_file_load_from_file (keys, ini, G_KEY_FILE_NONE, NULL))
    {
      gchar *locale = g_key_file_get_string (keys, EOS_GROUP, LOCALE_KEY, NULL);
      if (locale != NULL)
        {
          gis_language_page_preselect_language (locale);
          g_free (locale);
        }

      gis_store_set_key_file (keys);

      if (g_key_file_has_group (keys, UNATTENDED_GROUP))
        {
          gchar *vendor = NULL;
          gchar *product = NULL;
          GError *error = NULL;

          if (NULL == (vendor = read_sanitized_string ("/sys/class/dmi/id/sys_vendor", &error)) ||
              NULL == (product = read_sanitized_string ("/sys/class/dmi/id/product_name", &error)))
            {
              g_warning ("%s", error->message);
              g_clear_error (&error);
            }
          else
            {
              gchar *target_vendor = g_key_file_get_string (keys, UNATTENDED_GROUP, VENDOR_KEY, NULL);
              gchar *target_product = g_key_file_get_string (keys, UNATTENDED_GROUP, PRODUCT_KEY, NULL);

              if (g_ascii_strcasecmp (vendor, target_vendor) == 0
               && g_ascii_strcasecmp (product, target_product) == 0)
                {
                  /* We just set the flag here, rest of the magic happens as we go */
                  gis_store_enter_unattended();
                }
              else
                {
                  g_warning ("Unattended mode requested but target device is wrong: expected '%s' from '%s' but system reports '%s' from '%s'",
                           target_product, target_vendor, product, vendor);
                  /* Continue in attended mode */
                }
              g_free (target_vendor);
              g_free (target_product);
            }

            g_free (vendor);
            g_free (product);
        }
    }
}

static void
mount_and_read_keys (void)
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
      gchar *path = NULL;

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
        {
          read_keys (mounts[0]);
          return;
        }

      if (udisks_filesystem_call_mount_sync (fs, g_variant_new ("a{sv}", NULL),
                                             &path, NULL, NULL))
        {
          read_keys (path);
        }
    }
}

/* Should be kept in sync with gnome-initial-setup gis-driver.c */
static gboolean
check_for_live_boot (gchar **uuid)
{
  const gchar *force = NULL;
  GError *error = NULL;
  g_autofree gchar *cmdline = NULL;
  gboolean live_boot = FALSE;
  g_autoptr(GRegex) reg = NULL;
  g_autoptr(GMatchInfo) info = NULL;

  g_return_val_if_fail (uuid != NULL, FALSE);

  force = g_getenv ("EI_FORCE_LIVE_BOOT_UUID");
  if (force != NULL && *force != '\0')
    {
      g_print ("EI_FORCE_LIVE_BOOT_UUID set to %s\n", force);
      *uuid = g_strdup (force);
      return TRUE;
    }

  if (!g_file_get_contents ("/proc/cmdline", &cmdline, NULL, &error))
    {
      g_printerr ("unable to read /proc/cmdline: %s\n", error->message);
      g_error_free (error);
      return FALSE;
    }

  live_boot = g_regex_match_simple ("\\bendless\\.live_boot\\b", cmdline, 0, 0);

  g_print ("set live_boot to %u from /proc/cmdline: %s\n", live_boot, cmdline);

  reg = g_regex_new ("\\bendless\\.image\\.device=UUID=([^\\s]*)", 0, 0, NULL);
  g_regex_match (reg, cmdline, 0, &info);
  if (g_match_info_matches (info))
    {
      *uuid = g_match_info_fetch (info, 1);
      g_print ("set UUID to %s\n", *uuid);
    }

  return live_boot;
}

static void
rebuild_pages_cb (GisDriver *driver)
{
  PageData *page_data;
  GisAssistant *assistant;
  GisPage *current_page;

  assistant = gis_driver_get_assistant (driver);
  current_page = gis_assistant_get_current_page (assistant);

  /* Omit welcome page entirely in live mode */
  if (gis_store_is_live_install ())
    page_data = page_table + 1;
  else
    page_data = page_table;

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

  /* Skip welcome page in unattended mode */
  if (gis_store_is_unattended () && !gis_store_is_live_install ())
    gis_assistant_next_page (assistant);
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

gis_page_get_type();

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

  gis_ensure_login_keyring ("gis");

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
  mount_and_read_keys ();

  driver = gis_driver_new (get_mode ());
  g_signal_connect (driver, "rebuild-pages", G_CALLBACK (rebuild_pages_cb), NULL);
  status = g_application_run (G_APPLICATION (driver), argc, argv);

  g_object_unref (driver);
  g_option_context_free (context);

  return status;

/* Epilogue {{{1 */
/* vim: set foldmethod=marker: */
