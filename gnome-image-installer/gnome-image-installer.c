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

#ifdef HAVE_CLUTTER
#include <clutter-gtk/clutter-gtk.h>
#endif

#ifdef HAVE_CHEESE
#include <cheese-gtk.h>
#endif

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
static const gchar *system_setup_pages[] = {
    "account",
    "display",
    "endless_eula",
    "location"
};

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
#define IMAGE_KEY "filename"
#define DRIVE_KEY "blockdevice"

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

static gboolean
target_computer (GKeyFile *keys, gchar *vendor, gchar *product)
{
  gboolean found = FALSE;
  gsize i = 0;
  gchar **groups = g_key_file_get_groups (keys, NULL);

  while (groups[i] != NULL && found == FALSE)
    {
      if (g_str_has_prefix (groups[i], "Computer"))
        {
          g_autofree gchar *target_vendor = g_key_file_get_string (keys, groups[i], VENDOR_KEY, NULL);
          g_autofree gchar *target_product = g_key_file_get_string (keys, groups[i], PRODUCT_KEY, NULL);

          found = (g_ascii_strcasecmp (vendor, target_vendor) == 0
                && g_ascii_strcasecmp (product, target_product) == 0);
        }
      i++;
    }

  g_strfreev (groups);
  return found;
}

static void
collect_images (GKeyFile *keys, const gchar *path)
{
  GisStoreTargetName target = GIS_STORE_TARGET_PRIMARY;
  gsize i = 0;
  gchar **groups = g_key_file_get_groups (keys, NULL);

  while (groups[i] != NULL && target < GIS_STORE_N_TARGETS)
    {
      if (g_str_has_prefix (groups[i], "Image"))
        {
          g_autofree gchar *image = g_key_file_get_string (keys, groups[i], IMAGE_KEY, NULL);
          g_autofree gchar *drive = g_key_file_get_string (keys, groups[i], DRIVE_KEY, NULL);

          if (image != NULL)
            {
              g_autofree gchar *img = g_build_path ("/", path, image, NULL);
              g_autofree gchar *signature = g_strjoin (NULL, img, ".asc", NULL);
              if (g_file_test(img, G_FILE_TEST_EXISTS)
               && g_file_test(signature, G_FILE_TEST_EXISTS))
                {
                  gis_store_set_target (target, img, signature, drive);
                }
              target++;
            }
        }
      i++;
    }

  g_strfreev (groups);
}

static void
read_keys (const gchar *path)
{
  gchar *ini = g_build_path ("/", path, "unattended.ini", NULL);
  GKeyFile *keys = g_key_file_new();

  if (g_key_file_load_from_file (keys, ini, G_KEY_FILE_NONE, NULL))
    {
      g_autofree gchar *vendor = NULL;
      g_autofree gchar *product = NULL;
      GError *error = NULL;
      g_autofree gchar *locale = g_key_file_get_string (keys, EOS_GROUP, LOCALE_KEY, NULL);

      if (locale != NULL)
        {
          gis_language_page_preselect_language (locale);
        }

      gis_store_set_key_file (keys);

      if (NULL == (vendor = read_sanitized_string ("/sys/class/dmi/id/sys_vendor", &error)) ||
          NULL == (product = read_sanitized_string ("/sys/class/dmi/id/product_name", &error)))
        {
          g_warning ("%s", error->message);
          g_clear_error (&error);
        }
      else
        {
          if (target_computer (keys, vendor, product))
            {
              gis_store_enter_unattended();
            }
        }

      collect_images (keys, path);
    }
}

static void
mount_and_read_keys ()
{
  GError *error = NULL;
  UDisksClient *client = udisks_client_new_sync(NULL, &error);
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

static gboolean
check_for_live_boot ()
{
  const gchar *force = g_getenv ("EI_FORCE_LIVE_BOOT");
  GError *error = NULL;
  g_autofree gchar *cmdline = NULL;
  gboolean live_boot = FALSE;

  if (force != NULL && *force != '\0')
    {
      g_print ("EI_FORCE_LIVE_BOOT='%s', set live_boot to TRUE\n", force);
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

  /* Skip welcome page in unattended and live install mode */
  if (gis_store_is_unattended () || gis_store_is_live_install ())
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

#ifdef HAVE_CHEESE
  cheese_gtk_init (NULL, NULL);
#endif

  gtk_init (&argc, &argv);
  ev_init ();

#if HAVE_CLUTTER
  if (gtk_clutter_init (NULL, NULL) != CLUTTER_INIT_SUCCESS) {
    g_critical ("Clutter-GTK init failed");
    exit (1);
  }
#endif

  gis_ensure_login_keyring ("gis");

  if (check_for_live_boot ())
    gis_store_enter_live_install ();

  mount_and_read_keys ();

  driver = gis_driver_new (get_mode ());
  g_signal_connect (driver, "rebuild-pages", G_CALLBACK (rebuild_pages_cb), NULL);
  status = g_application_run (G_APPLICATION (driver), argc, argv);

  g_object_unref (driver);
  g_option_context_free (context);
  ev_shutdown ();

  return status;
}

/* Epilogue {{{1 */
/* vim: set foldmethod=marker: */
