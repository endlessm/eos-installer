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

/* DiskImage page {{{1 */

#define PAGE_ID "diskimage"

#include "config.h"
#include "diskimage-resources.h"
#include "gis-diskimage-page.h"
#include "gis-store.h"
#include "gpt.h"
#include "gpt_gz.h"
#include "gpt_lzma.h"

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>

#include <udisks/udisks.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <errno.h>

struct _GisDiskImagePagePrivate {
  gint dummy;
};
typedef struct _GisDiskImagePagePrivate GisDiskImagePagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisDiskImagePage, gis_diskimage_page, GIS_TYPE_PAGE);

#define OBJ(type,name) ((type)gtk_builder_get_object(GIS_PAGE(page)->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)

G_DEFINE_QUARK(image-error, gis_image_error);

/* A device-mapped copy of endless.img used in image boots from exFAT.
 * We prefer to use this to endless.img from the filesystem for two reasons:
 * - zeros are mapped over the sectors of the drive which correspond to the
 *   image, so we can't read it from the filesystem
 * - reading from the filesystem (via fuse) comes with a big overhead
 * Even if we are booted from ISO (using a regular loop device), and/or
 * from endless.squash (so the endless.img within can be mapped to a regular
 * loop device), the boot scripts always arrange for this device to exist, and
 * there should be no overhead in unnecessarily using it.
 */
static const gchar * const live_device_path = "/dev/mapper/endless-image";

/* A device-mapped copy of endless.squash used in image boots from exFAT.
 * Although in this case we will read from live_device_path when writing to
 * disk, we prefer to read the compressed file while verifying to avoid
 * incurring the decompression overhead twice, in addition to the two reasons
 * above.
 */
static const gchar * const squashfs_device_path = "/dev/mapper/endless-image-squashfs";

/* Deliberately out-of-order so that sorting is exercised in English */
static const gchar * const sea_locales[] = {
  "th",
  "id",
  "vi",
  NULL
};
static const gsize num_sea_locales = sizeof (sea_locales) / sizeof (sea_locales[0]);

enum {
    /* Human-readable image name */
    IMAGE_NAME = 0,
    /* Human-readable image size */
    IMAGE_SIZE,
    ALIGN,
    /* GisImage struct with all the gory details */
    IMAGE
};

static void
gis_diskimage_page_selection_changed(GtkWidget *combo, GisPage *page)
{
  GtkTreeIter i;
  GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
  g_autoptr(GisImage) image = NULL;

  if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &i))
    {
      gis_page_set_complete (page, FALSE);
      return;
    }

  gtk_tree_model_get (model, &i,
      IMAGE, &image,
      -1);

  gis_store_set_selected_image (image);
  gis_page_set_complete (page, TRUE);

  if (gis_store_is_unattended())
    {
      if (gtk_tree_model_iter_n_children (model, NULL) > 1)
        {
          GError *error = g_error_new (GIS_IMAGE_ERROR, 0, _("No suitable images were found."));
          gis_store_set_error (error);
          g_clear_error (&error);
        }
      gis_assistant_next_page (gis_driver_get_assistant (page->driver));
    }
}

static gchar *
get_locale_name (const gchar *locale)
{
  /* This calls gnome_parse_locale(), which warns on malformed locales.
  */
  gchar *language = gnome_get_language_from_locale (locale, NULL);

  if (language != NULL)
    {
      /* TODO: what is this stupid grumblegrumble... */
      if (g_strrstr (language, "[") != NULL)
        {
          gchar **split = g_strsplit (language, " [", 0);
          g_free (language);
          language = g_strdup (split[0]);
          g_strfreev (split);
        }
    }

  return language;
}

static gint
compare_localized_name (gconstpointer a_ptr, gconstpointer b_ptr)
{
  const gchar *a = *(const gchar * const *) a_ptr;
  const gchar *b = *(const gchar * const *) b_ptr;

  return g_utf8_collate (a, b);
}

static const gchar *
lookup_personality (const gchar *personality)
{
  if (g_str_equal (personality, "base"))
    return _("Basic");

  if (g_str_equal (personality, "fnde_aluno"))
    /* Translators: this is the name of a version of Educa Endless, which you
       should leave untranslated. */
    return _("Aluno");

  if (g_str_equal (personality, "fnde_escola"))
    /* Translators: this is the name of a version of Educa Endless, which you
       should leave untranslated. */
    return _("Escola");

  /* Southeast Asia */
  if (g_str_equal (personality, "sea"))
    {
      g_autoptr(GPtrArray) localized_names = g_ptr_array_new_full (
          num_sea_locales, g_free);
      const gchar * const *l;

      for (l = sea_locales; *l != NULL; l++)
        {
          gchar *localized_name = get_locale_name (*l);

          if (localized_name != NULL)
            g_ptr_array_add (localized_names, localized_name);
        }

      if (G_UNLIKELY (localized_names->len == 0))
        return NULL;

      g_ptr_array_sort (localized_names, compare_localized_name);
      g_ptr_array_add (localized_names, NULL);
      return g_strjoinv (", ", (gchar **) localized_names->pdata);
    }

  return NULL;
}

static gchar *get_display_name(const gchar *fullname)
{
  GRegex *reg;
  GMatchInfo *info;
  gchar *name = NULL;

  reg = g_regex_new ("^.*/([^-]+)-([^-]+)-(?:[^-]+)-(?:[^.]+)\\.(?:[^.]+)\\.([^.]+)(?:\\.(disk\\d))?\\.(?:img|squash)(?:\\.([gx]z|asc))$", 0, 0, NULL);
  g_regex_match (reg, fullname, 0, &info);
  if (g_match_info_matches (info))
    {
      g_autofree gchar *product = g_match_info_fetch (info, 1);
      g_autofree gchar *version = g_match_info_fetch (info, 2);
      g_autofree gchar *personality = g_match_info_fetch (info, 3);
      g_autofree gchar *type = g_match_info_fetch (info, 4);
      g_autofree gchar *language = NULL;
      const gchar *known_personality = NULL;

      /* Split images not supported yet */
      if (strlen (type) > 0)
        {
          return NULL;
        }

      if (g_str_equal (product, "eos"))
        {
          g_free (product);
          product = g_strdup (_("Endless OS"));
        }
      else if (g_str_equal (product, "eosinstaller"))
        {
          g_free (product);
          product = g_strdup (_("Endless OS Installer"));
        }
      else if (g_str_equal (product, "eosnonfree"))
        {
          g_free (product);
          product = g_strdup (_("Endless OS (non-free)"));
        }
      else if (g_str_equal (product, "eosoem"))
        {
          g_free (product);
          /* Translators: this is the edition of Endless OS pre-installed by
           * Original Equipment Manufacturers. If there is not a
           * widely-understood short translation of "OEM" in your language,
           * please do not translate this.
           */
          product = g_strdup (_("Endless OS (OEM)"));
        }
      else if (g_str_equal (product, "eosdvd"))
        {
          g_free (product);
          /* Translators: this is the DVD-sized edition of Endless OS. Please
           * only translate this if "DVD" is not widely understood in your
           * language.
           */
          product = g_strdup (_("Endless OS (DVD)"));
        }
      else if (g_str_equal (product, "fnde"))
        {
          g_free (product);
          /* Translators: this is a brand name, which you should leave
             untranslated. */
          product = g_strdup (_("Educa Endless"));
        }

      if (g_str_has_prefix (version, "eos"))
        {
          gchar *tmp = g_strdup(version+3);
          g_free (version);
          version = tmp;
        }

      /* Use a special-cased name if available; the language name if the
       * personality is a valid locale; or the raw personality otherwise.
       */
      known_personality = lookup_personality (personality);
      if (known_personality == NULL)
        {
          language = get_locale_name (personality);
          if (language == NULL)
            {
              known_personality = personality;
            }
        }

      if (known_personality != NULL)
        {
          name = g_strdup_printf ("%s %s %s", product, version, known_personality);
        }
      else
        {
          g_free (personality);
          personality = g_strdup (_("Full"));

          name = g_strdup_printf ("%s %s %s %s", product, version, language, personality);
        }
    }

  return name;
}

static void
add_image (
    GtkListStore *store,
    const gchar  *image,
    const gchar  *image_device,
    const gchar  *verify_path,
    const gchar  *signature)
{
  GError *error = NULL;
  g_autoptr(GFile) f = g_file_new_for_path (image);
  g_autoptr(GFileInfo) fi = g_file_query_info (f, G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                     G_FILE_QUERY_INFO_NONE, NULL,
                                     &error);
  if (fi != NULL)
    {
      g_autofree gchar *displayname = NULL;
      guint64 required_size = 0;

      /* TODO: make image size an out parameter of get_*_is_valid_eos_gpt */
      if (g_str_has_suffix (image, ".img.gz") &&
          get_gzip_is_valid_eos_gpt (image))
        {
          required_size = get_gzip_disk_image_size (image);
        }
      else if (g_str_has_suffix (image, ".img.xz") &&
          get_xz_is_valid_eos_gpt (image))
        {
          required_size = get_xz_disk_image_size (image);
        }
      else if (image_device != NULL &&
          get_is_valid_eos_gpt (image_device))
        {
          required_size = get_disk_image_size (image_device);
        }
      else if (g_str_has_suffix (image, ".img") &&
          get_is_valid_eos_gpt (image))
        {
          required_size = get_disk_image_size (image);
        }

      if (required_size != 0)
        {
          displayname = get_display_name (image);

          /* this will fail if image is (eg) "endless.img"; try the signature instead.
           * from that too */
          if (displayname == NULL)
            displayname = get_display_name (signature);
        }

      if (displayname != NULL)
        {
          goffset size_bytes = g_file_info_get_size (fi);
          g_autofree gchar *size = NULL;
          g_autoptr(GFile) image_file = NULL;
          g_autoptr(GFile) signature_file = NULL;
          g_autoptr(GFile) verify_file = NULL;
          g_autoptr(GisImage) gis_image = NULL;

          size = g_format_size_full (size_bytes, G_FORMAT_SIZE_DEFAULT);

          g_print ("Found image: %s (%s, image_device=%s, verify_path=%s, signature=%s)\n",
              image, size, image_device, verify_path, signature);

          if (image_device != NULL)
            image_file = g_file_new_for_path (image_device);
          else
            image_file = g_object_ref (f);

          if (verify_path != NULL)
            verify_file = g_file_new_for_path (verify_path);
          else
            verify_file = g_object_ref (image_file);

          signature_file = g_file_new_for_path (signature);
          gis_image = gis_image_new (displayname, image_file, verify_file,
              signature_file, size_bytes, required_size);
          gtk_list_store_insert_with_values (store, NULL, -1,
                                             IMAGE_NAME, displayname,
                                             IMAGE_SIZE, size,
                                             IMAGE, gis_image,
                                             -1);
        }
    }
  else
    {
      g_warning ("Could not get file info: %s", error->message);
      g_clear_error (&error);
    }
}

static gboolean
file_exists (
    const gchar *path,
    GError     **error)
{
  g_autoptr(GFile) file = g_file_new_for_path (path);

  if (g_file_query_exists (file, NULL))
    return TRUE;

  g_set_error (error, GIS_IMAGE_ERROR, 0, "%s doesn't exist", path);
  return FALSE;
}

/* ISOs have a squashfs image named /endless/endless.squash, and live USB
 * sticks have either this or an unpacked image named /endless/endless.img. For
 * various reasons, these are sometimes invisible in exFAT directory listings.
 *
 * We can get the name that the image "should" have by reading /endless/live,
 * and use this to find the image and corresponding signature file.
 */
static gboolean
gis_diskimage_page_add_live_image (
    GtkListStore *store,
    gchar        *path,
    const gchar  *ufile,
    GError      **error)
{
  g_autofree gchar *live_flag_path = NULL;
  g_autofree gchar *live_flag_contents = NULL;
  g_autofree gchar *live_sig_basename = NULL;
  g_autofree gchar *live_sig = NULL;
  const gchar *endless_basename = NULL;
  g_autofree gchar *endless_path = NULL;
  gboolean is_squashfs = FALSE;
  const gchar *verify_path = NULL;

  live_flag_path = g_build_path ("/", path, "endless", "live", NULL);
  if (!g_file_get_contents (live_flag_path, &live_flag_contents, NULL, error))
    {
      g_prefix_error (error, "Couldn't read %s: ", live_flag_path);
      return FALSE;
    }

  /* live_flag_contents contains the name that endless.img/endless.squash would have had;
   * so we should be able to find the image at endless.<ext> and its signature
   * at ${live_flag_contents}.asc
   */
  g_strstrip (live_flag_contents);

  if (g_str_has_suffix (live_flag_contents, ".img"))
    {
      endless_basename = "endless.img";
      is_squashfs = FALSE;
    }
  else if (g_str_has_suffix (live_flag_contents, ".squash"))
    {
      endless_basename = "endless.squash";
      is_squashfs = TRUE;
    }
  else
    {
      g_set_error (error, GIS_IMAGE_ERROR, 0, "Unknown live image format '%s'",
          live_flag_contents);
      return FALSE;
    }

  endless_path = g_build_path ("/", path, "endless", endless_basename, NULL);
  if (!file_exists (endless_path, error))
    return FALSE;

  live_sig_basename = g_strdup_printf ("%s.%s", live_flag_contents, "asc");
  live_sig = g_build_path ("/", path, "endless", live_sig_basename, NULL);

  if (!file_exists (live_sig, error))
    return FALSE;

  if (ufile != NULL && g_strcmp0 (ufile, live_flag_contents) != 0)
    {
      g_set_error (error, GIS_IMAGE_ERROR, 0,
          "live image '%s' doesn't match unattended image '%s'",
          live_flag_contents, ufile);
      return FALSE;
    }

  if (is_squashfs)
    {
      if (file_exists (squashfs_device_path, NULL))
        verify_path = squashfs_device_path;
      else
        verify_path = endless_path;
    }

  if (file_exists (live_device_path, NULL))
    {
      add_image (store, endless_path, live_device_path, verify_path, live_sig);
    }
  else if (!is_squashfs)
    {
      add_image (store, endless_path, NULL, NULL, live_sig);
    }
  else
    {
      // TODO: mount the squashfs image and read endless.img from within it?
      g_set_error (error, GIS_IMAGE_ERROR, 0,
          "can't find image device %s; and can't use %s directly\n",
          live_device_path, endless_path);
      return FALSE;
    }

  return TRUE;
}

static void
gis_diskimage_page_populate_model(GisPage *page, gchar *path)
{
  GError *error = NULL;
  gchar *file = NULL;
  gchar *ufile = NULL;
  GtkListStore *store = OBJ(GtkListStore*, "image_store");
  GDir *dir;
  GtkTreeIter iter;
  gboolean is_live = gis_store_is_live_install ();

  dir = g_dir_open (path, 0, &error);
  if (dir == NULL)
    {
      gis_store_set_error (error);
      g_clear_error (&error);
      gis_assistant_next_page (gis_driver_get_assistant (page->driver));
      return;
    }

  gtk_list_store_clear(store);

  if (gis_store_is_unattended())
    {
      GKeyFile *keys = gis_store_get_key_file();
      if (keys != NULL)
        {
          ufile = g_key_file_get_string (keys, "Unattended", "image", NULL);
        }
    }

  for (file = (gchar*)g_dir_read_name (dir); file != NULL; file = (gchar*)g_dir_read_name (dir))
    {
      g_autofree gchar *fullpath = g_build_path ("/", path, file, NULL);
      g_autofree gchar *signature = g_strdup_printf ("%s.asc", fullpath);

      /* ufile is only set in the unattended case */
      if (ufile == NULL || g_str_equal (ufile, file))
        {
          add_image (store, fullpath, NULL, NULL, signature);
        }
    }

  if (is_live &&
      !gis_diskimage_page_add_live_image (store, path, ufile, &error))
    {
      g_print ("finding live image failed: %s\n", error->message);
    }

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter))
    {
      GtkComboBox *combo = OBJ (GtkComboBox*, "imagecombo");
      gtk_combo_box_set_active_iter (combo, &iter);
    }
  else
    {
      if (error == NULL)
        error = g_error_new (GIS_IMAGE_ERROR, 0, _("No suitable images were found."));
      gis_store_set_error (error);
      g_clear_error (&error);
      gis_assistant_next_page (gis_driver_get_assistant (page->driver));
    }

  g_dir_close (dir);
}

static void
gis_diskimage_page_mount_ready (GObject *source, GAsyncResult *res, GisPage *page)
{
  UDisksFilesystem *fs = UDISKS_FILESYSTEM (source);
  GError *error = NULL;
  gchar *path = NULL;

  if (!udisks_filesystem_call_mount_finish (fs, &path, res, &error))
    {
      gis_store_set_error (error);
      g_error_free (error);
      gis_assistant_next_page (gis_driver_get_assistant (page->driver));
      return;
    }

  gis_diskimage_page_populate_model(page, path);
}

static void
gis_diskimage_page_mount (GisPage *page)
{
  GError *error = NULL;
  gboolean is_live = gis_store_is_live_install ();
  const gchar *uuid = gis_store_get_image_uuid ();
  UDisksClient *client = UDISKS_CLIENT (gis_store_get_object (GIS_STORE_UDISKS_CLIENT));
  GDBusObjectManager *manager = udisks_client_get_object_manager(client);
  GList *objects = g_dbus_object_manager_get_objects(manager);
  GList *l;
  const gchar *label;

  if (gis_store_is_live_install ())
    label = "eoslive";
  else
    label = "eosimages";

  for (l = objects; l != NULL; l = l->next)
    {
      UDisksObject *object = UDISKS_OBJECT (l->data);
      UDisksBlock *block = udisks_object_peek_block (object);
      UDisksFilesystem *fs = NULL;
      const gchar *const*mounts = NULL;
      const gchar *dev = NULL;
      UDisksDrive *drive = NULL;

      if (block == NULL)
        continue;

      dev = udisks_block_get_preferred_device (block);

      if (!((is_live && g_str_equal (uuid, udisks_block_get_id_uuid (block))) ||
          g_str_equal (label, udisks_block_get_id_label (block))))
        continue;

      if (udisks_block_get_hint_ignore (block))
        {
          g_print ("skipping %s with ignore hint set\n", dev);
          continue;
        }

      fs = udisks_object_peek_filesystem (object);

      if (fs == NULL)
        {
          continue;
        }

      g_print ("found label or UUID partition at %s\n", dev);

      mounts = udisks_filesystem_get_mount_points (fs);

      if (mounts != NULL && mounts[0] != NULL)
        {
          gis_diskimage_page_populate_model(page, (gchar*)mounts[0]);
        }
      else
        {
          udisks_filesystem_call_mount (fs, g_variant_new ("a{sv}", NULL), NULL,
                                        (GAsyncReadyCallback)gis_diskimage_page_mount_ready, page);
        }

      drive = udisks_client_get_drive_for_block (client, block);
      if (drive != NULL)
        {
          gis_store_set_object (GIS_STORE_IMAGE_DRIVE, G_OBJECT (drive));
          g_clear_object (&drive);
        }
      /* If running from exFAT or NTFS, where we use device mapper rather than
       * loopback mount, the image host partition we use is on another mapped
       * device (because we can't mount the real one directly). In this case,
       * the UDisksBlock has no associated UDisksDrive.
       */

      return;
    }

  error = g_error_new (GIS_IMAGE_ERROR, 0, _("No suitable images were found."));
  gis_store_set_error (error);
  g_clear_error (&error);
  gis_assistant_next_page (gis_driver_get_assistant (page->driver));
}

static gboolean
gis_diskimage_page_shown_idle_cb (gpointer user_data)
{
  GisPage *page = GIS_PAGE (user_data);

  gis_diskimage_page_mount (page);

  return G_SOURCE_REMOVE;
}

static void
gis_diskimage_page_shown (GisPage *page)
{
  /* In most cases, this page is the first page shown.
   * gis_diskimage_page_mount() can, in several situations, call
   * gis_assistant_next_page() synchronously. But when the page is first shown,
   * the list of pages is not fully initialized in the driver.
   *
   * So, defer initializing the page.
   */
  g_idle_add_full (G_PRIORITY_DEFAULT, gis_diskimage_page_shown_idle_cb,
                   g_object_ref (page), g_object_unref);
}

static void
gis_diskimage_page_constructed (GObject *object)
{
  GisDiskImagePage *page = GIS_DISK_IMAGE_PAGE (object);

  G_OBJECT_CLASS (gis_diskimage_page_parent_class)->constructed (object);

  gtk_container_add (GTK_CONTAINER (page), WID ("diskimage-page"));

  gis_page_set_complete (GIS_PAGE (page), FALSE);

  g_signal_connect(OBJ(GObject*, "imagecombo"),
                       "changed", G_CALLBACK(gis_diskimage_page_selection_changed),
                       page);

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_diskimage_page_locale_changed (GisPage *page)
{
  gis_page_set_title (page, _("Reformat with Endless OS"));
}

static void
gis_diskimage_page_class_init (GisDiskImagePageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* GisImage is referenced by name in gis-diskimage-page.ui */
  g_type_ensure (gis_image_get_type ());

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_diskimage_page_locale_changed;
  page_class->shown = gis_diskimage_page_shown;
  object_class->constructed = gis_diskimage_page_constructed;
}

static void
gis_diskimage_page_init (GisDiskImagePage *page)
{
  g_resources_register (diskimage_get_resource ());
}

void
gis_prepare_diskimage_page (GisDriver *driver)
{
  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_DISK_IMAGE_PAGE,
                                     "driver", driver,
                                     NULL));
}
