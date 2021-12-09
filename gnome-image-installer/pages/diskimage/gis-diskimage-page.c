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

/* DiskImage page {{{1 */

#define PAGE_ID "diskimage"

#include "config.h"
#include "diskimage-resources.h"
#include "gis-diskimage-page.h"
#include "gis-errors.h"
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
    GtkListStore *image_store;
    GtkComboBox *image_combo;
};
typedef struct _GisDiskImagePagePrivate GisDiskImagePagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisDiskImagePage, gis_diskimage_page, GIS_TYPE_PAGE);

/* A device-mapped copy of endless.img used in image boots.
 * We prefer to use this to endless.img from the filesystem for two reasons:
 * - 'error' is mapped over the sectors of the drive which correspond to the
 *   image, so we can't read it from the filesystem
 * - reading from the filesystem (via fuse) comes with a big overhead
 */
static const gchar * const live_device_path = "/dev/mapper/endless-image";

/* Deliberately out-of-order so that sorting is exercised in English */
static const gchar * const sea_locales[] = {
  "th",
  "id",
  "vi",
  NULL
};
static const gsize num_sea_locales = sizeof (sea_locales) / sizeof (sea_locales[0]);

enum {
    IMAGE_NAME = 0,
    IMAGE_SIZE,
    IMAGE_SIZE_BYTES,
    IMAGE_FILE,
    IMAGE_SIGNATURE,
    IMAGE_CHECKSUM,
    ALIGN,
    IMAGE_REQUIRED_SIZE
};

static void
gis_diskimage_page_selection_changed(GtkWidget *combo, GisPage *page)
{
  GtkTreeIter i;
  gchar *image, *name, *signature = NULL;
  g_autofree gchar *checksum = NULL;
  GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
  GFile *file = NULL;
  guint64 size_bytes;
  guint64 required_size;

  if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &i))
    {
      gis_page_set_complete (page, FALSE);
      return;
    }

  gtk_tree_model_get(model, &i,
      IMAGE_NAME, &name,
      IMAGE_FILE, &image,
      IMAGE_SIGNATURE, &signature,
      IMAGE_CHECKSUM, &checksum,
      IMAGE_SIZE_BYTES, &size_bytes,
      IMAGE_REQUIRED_SIZE, &required_size,
      -1);

  gis_store_set_image_name (name);
  gis_store_set_image_size (size_bytes);
  gis_store_set_required_size (required_size);
  g_free (name);

  file = g_file_new_for_path (image);
  gis_store_set_object (GIS_STORE_IMAGE, G_OBJECT (file));
  g_object_unref(file);

  if (signature == NULL)
    signature = g_strjoin (NULL, image, ".asc", NULL);

  gis_store_set_image_signature (signature);
  g_free (signature);

  if (checksum == NULL)
    checksum = g_strjoin (NULL, image, ".sha256", NULL);

  gis_store_set_image_checksum (checksum);

  gis_page_set_complete (page, TRUE);

  if (gis_store_is_unattended ())
    {
      if (gtk_tree_model_iter_n_children (model, NULL) > 1)
        {
          g_autoptr(GError) error =
            g_error_new_literal (GIS_UNATTENDED_ERROR,
                                 GIS_UNATTENDED_ERROR_IMAGE_AMBIGUOUS,
                                 _("More than one image was found."));
          gis_store_set_error (error);
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

  reg = g_regex_new ("^.*/([^-]+)-([^-]+)-(?:[^-]+)-(?:[^.]+)\\.(?:[^.]+)\\.([^.]+)(?:\\.(disk\\d))?\\.img(?:\\.([gx]z|asc|sha256))?$", 0, 0, NULL);
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
    const gchar  *signature,
    const gchar  *checksum)
{
  GtkTreeIter i;
  GError *error = NULL;
  g_autoptr(GFile) f = g_file_new_for_path (image);
  g_autoptr(GFileInfo) fi = g_file_query_info (f, G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                     G_FILE_QUERY_INFO_NONE, NULL,
                                     &error);
  if (fi != NULL)
    {
      gchar *size = NULL;
      gchar *displayname = NULL;
      gboolean valid = FALSE;
      guint64 required_size = 0;

      if (g_str_has_suffix (image, ".img.gz"))
        {
          valid = get_gzip_is_valid_eos_gpt (image, &required_size);
        }
      else if (g_str_has_suffix (image, ".img.xz"))
        {
          valid = get_xz_is_valid_eos_gpt (image, &required_size);
        }
      else if (image_device != NULL)
        {
          valid = get_is_valid_eos_gpt (image_device, &required_size);
        }
      else if (g_str_has_suffix (image, ".img"))
        {
          valid = get_is_valid_eos_gpt (image, &required_size);
        }

      if (valid && required_size != 0)
        {
          displayname = get_display_name (image);

          /* if we have a signature file or checksum file passed in,
           * attempt to get the name from that too */
          if (displayname == NULL)
            {
              if (signature != NULL)
                {
                  displayname = get_display_name (signature);
                }
              if (displayname == NULL && checksum != NULL)
                {
                  displayname = get_display_name (checksum);
                }
            }
        }

      if (displayname != NULL)
        {
          goffset size_bytes = g_file_info_get_size (fi);
          g_warn_if_fail (size_bytes >= 0);
          size = g_format_size_full (size_bytes, G_FORMAT_SIZE_DEFAULT);

          gtk_list_store_append (store, &i);
          gtk_list_store_set (store, &i,
                              IMAGE_NAME, displayname,
                              IMAGE_SIZE, size,
                              IMAGE_SIZE_BYTES, (guint64) size_bytes,
                              IMAGE_FILE, image_device != NULL ? image_device : image,
                              IMAGE_SIGNATURE, signature,
                              IMAGE_CHECKSUM, checksum,
                              IMAGE_REQUIRED_SIZE, required_size,
                              -1);
          g_free (size);
          g_free (displayname);
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

  g_set_error (error, GIS_IMAGE_ERROR, GIS_IMAGE_ERROR_NOT_FOUND,
               _("Image file ‘%s’ does not exist."), path);
  return FALSE;
}

/**
 * Returns: the first of @a or @b which exists; or %NULL with a combined error
 * if neither does.
 */
static gchar *
first_existing (
    gchar *a,
    gchar *b,
    GError **error)
{
  GError *error2 = NULL;

  if (file_exists (a, &error2))
    return a;

  if (file_exists (b, error))
    {
      g_clear_error (&error2);
      return b;
    }

  g_prefix_error (error, "%s ", error2->message);
  g_clear_error (&error2);
  return NULL;
}

/* live USB sticks have an unpacked disk image named endless.img, which for
 * various reasons is invisible in directory listings. We can determine the
 * name that the image "should" have by reading /endless/live, and find the
 * corresponding signature file.
 * For ISOs the disk image is called endless.squash.
 */
static gboolean
gis_diskimage_page_add_live_image (
    GtkListStore        *store,
    const gchar         *path,
    const gchar         *ufile,
    GError             **error)
{
  g_autofree gchar *endless_img_path = g_build_path (
      "/", path, "endless", "endless.img", NULL);
  g_autofree gchar *endless_squash_path = g_build_path (
      "/", path, "endless", "endless.squash", NULL);
  g_autofree gchar *live_flag_path = g_build_path (
      "/", path, "endless", "live", NULL);
  g_autofree gchar *live_flag_contents = NULL;
  g_autofree gchar *live_sig_basename = NULL;
  g_autofree gchar *live_sig = NULL;
  g_autofree gchar *live_csum_basename = NULL;
  g_autofree gchar *live_csum = NULL;
  gchar *endless_path; /* either endless_img_path or endless_squash_path */

  endless_path = first_existing (endless_img_path, endless_squash_path, error);
  if (endless_path == NULL)
    return FALSE;

  if (!g_file_get_contents (live_flag_path, &live_flag_contents, NULL, error))
    return FALSE;

  /* live_flag_contents contains the name that 'endless.img' would have had;
   * so we should be able to find its signature at ${live_flag_contents}.asc
   */
  g_strstrip (live_flag_contents);
  live_sig_basename = g_strdup_printf ("%s.%s", live_flag_contents, "asc");
  live_sig = g_build_path ("/", path, "endless", live_sig_basename, NULL);
  live_csum_basename = g_strdup_printf ("%s.%s", live_flag_contents, "sha256");
  live_csum = g_build_path ("/", path, "endless", live_csum_basename, NULL);

  if (!first_existing (live_sig, live_csum, error))
    {
      return FALSE;
    }

  if (ufile != NULL && g_strcmp0 (ufile, live_flag_contents) != 0)
    {
      g_set_error (error, GIS_UNATTENDED_ERROR,
                   GIS_UNATTENDED_ERROR_IMAGE_NOT_FOUND,
                   _("Live image ‘%s’ does not match configured image ‘%s’."),
                   live_flag_contents, ufile);
      return FALSE;
    }

  if (file_exists (live_device_path, NULL))
    {
      add_image (store, endless_path, live_device_path, live_sig, live_csum);
    }
  else if (endless_path == endless_img_path)
    {
      g_message ("can't find image device %s; will use %s directly",
                 live_device_path, endless_img_path);
      add_image (store, endless_img_path, NULL, live_sig, live_csum);
    }
  else
    {
      // TODO: mount the squashfs image and read endless.img from within it?
      g_set_error (error, GIS_IMAGE_ERROR, GIS_IMAGE_ERROR_NOT_SUPPORTED,
                   _("Cannot find image device ‘%s’ and cannot use ‘%s’ directly."),
                   live_device_path, endless_path);
      return FALSE;
    }

  return TRUE;
}

static void
gis_diskimage_page_populate_model (GisPage     *page,
                                   const gchar *path)
{
  GisDiskImagePage *self = GIS_DISK_IMAGE_PAGE (page);
  GisDiskImagePagePrivate *priv = gis_diskimage_page_get_instance_private (self);
  g_autoptr(GFile) path_file = g_file_new_for_path (path);
  g_autoptr(GError) error = NULL;
  const gchar *file = NULL;
  GisUnattendedConfig *config = gis_store_get_unattended_config ();
  const gchar *ufile =
    (config != NULL) ? gis_unattended_config_get_image (config) : NULL;
  g_autoptr(GDir) dir = NULL;
  GtkTreeIter iter;
  gboolean is_live = gis_store_is_live_install ();

  dir = g_dir_open (path, 0, &error);
  if (dir == NULL)
    {
      gis_store_set_error (error);
      gis_assistant_next_page (gis_driver_get_assistant (page->driver));
      return;
    }

  gis_store_set_object (GIS_STORE_IMAGE_DIR, G_OBJECT (path_file));
  gtk_list_store_clear (priv->image_store);

  while ((file = g_dir_read_name (dir)))
    {
      /* ufile is only set in the unattended case */
      if (ufile == NULL || g_strcmp0 (ufile, file) == 0)
        {
          g_autofree gchar *fullpath = g_build_path ("/", path, file, NULL);
          add_image (priv->image_store, fullpath, NULL, NULL, NULL);
        }
    }

  if (is_live &&
      !gis_diskimage_page_add_live_image (priv->image_store, path, ufile, &error))
    {
      g_warning ("finding live image failed: %s", error->message);
    }

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->image_store), &iter))
    {
      gtk_combo_box_set_active_iter (priv->image_combo, &iter);
    }
  else
    {
      if (error == NULL)
        {
          if (ufile != NULL)
            g_set_error (&error, GIS_UNATTENDED_ERROR,
                         GIS_UNATTENDED_ERROR_IMAGE_NOT_FOUND,
                         /* Translators: the placeholder is a filename. */
                         _("Configured image ‘%s’ was not found."),
                         ufile);
          else
            g_set_error_literal (&error, GIS_IMAGE_ERROR,
                                 GIS_IMAGE_ERROR_NOT_FOUND,
                                 _("No suitable images were found."));
        }
      gis_store_set_error (error);
      gis_assistant_next_page (gis_driver_get_assistant (page->driver));
    }
}

static void
gis_diskimage_page_mount (GisPage *page)
{
  g_autoptr(GError) error = NULL;
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
          g_message ("skipping %s with ignore hint set", dev);
          continue;
        }

      fs = udisks_object_peek_filesystem (object);

      if (fs == NULL)
        {
          continue;
        }

      g_message ("found label or UUID partition at %s", dev);

      drive = udisks_client_get_drive_for_block (client, block);
      if (drive != NULL)
        {
          gis_store_set_object (GIS_STORE_IMAGE_SOURCE, G_OBJECT (drive));
          g_clear_object (&drive);
        }
      else
        {
          g_autoptr(UDisksLoop) loop = udisks_client_get_loop_for_block (client,
                                                                         block);
          if (loop != NULL)
            gis_store_set_object (GIS_STORE_IMAGE_SOURCE, G_OBJECT (loop));
        }
      /* If running from exFAT or NTFS, where we use device mapper rather than
       * loopback mount, the image host partition we use is on another mapped
       * device (because we can't mount the real one directly). In this case,
       * the UDisksBlock has no associated UDisksDrive or UDisksLoop.
       */

      mounts = udisks_filesystem_get_mount_points (fs);

      if (mounts != NULL && mounts[0] != NULL)
        {
          gis_diskimage_page_populate_model (page, mounts[0]);
        }
      else
        {
          g_autofree gchar *path = NULL;
          GVariant *options = g_variant_new ("a{sv}", NULL);
          gboolean ret;
          ret = udisks_filesystem_call_mount_sync (fs, options, &path,
                                                   NULL, &error);
          if (!ret) {
            g_message ("Mount failed: %s", error->message);
            g_clear_error (&error);
            continue;
          }

          gis_diskimage_page_populate_model (page, path);
        }

      return;
    }

  error = g_error_new (GIS_IMAGE_ERROR, GIS_IMAGE_ERROR_NOT_FOUND,
                       _("Could not find partition holding Endless OS files."));
  gis_store_set_error (error);
  gis_assistant_next_page (gis_driver_get_assistant (page->driver));
}

static gboolean
gis_diskimage_page_shown_idle_cb (gpointer user_data)
{
  GisPage *page = GIS_PAGE (user_data);

  if (gis_store_get_error () != NULL)
    gis_assistant_next_page (gis_driver_get_assistant (page->driver));
  else
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
  GisDiskImagePagePrivate *priv = gis_diskimage_page_get_instance_private (page);

  G_OBJECT_CLASS (gis_diskimage_page_parent_class)->constructed (object);

  gis_page_set_complete (GIS_PAGE (page), FALSE);

  g_signal_connect (priv->image_combo,
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

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-diskimage-page.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisDiskImagePage, image_store);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisDiskImagePage, image_combo);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_diskimage_page_locale_changed;
  page_class->shown = gis_diskimage_page_shown;
  object_class->constructed = gis_diskimage_page_constructed;
}

static void
gis_diskimage_page_init (GisDiskImagePage *page)
{
  g_resources_register (diskimage_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (page));
}

void
gis_prepare_diskimage_page (GisDriver *driver)
{
  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_DISK_IMAGE_PAGE,
                                     "driver", driver,
                                     NULL));
}
