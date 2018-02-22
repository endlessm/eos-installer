/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2012 Red Hat
 * Copyright (C) 2016–2018 Endless Mobile, Inc.
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

/* Finished page {{{1 */

#define PAGE_ID "finished"

#include "config.h"
#include "finished-resources.h"
#include "gis-finished-page.h"
#include "gis-store.h"
#include "gis-dmi.h"

#include <gtk/gtkx.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <errno.h>
#include <attr/xattr.h>

#include <udisks/udisks.h>

struct _GisFinishedPagePrivate {
  gint led_state;

  GtkAccelGroup *accel_group;

  GtkWidget *success_box;
  GtkWidget *removelabel_usb;
  GtkWidget *removelabel_dvd;
  GtkButton *restart_button;

  GtkWidget *error_box;
  GtkLabel *error_heading_label;
  GtkLabel *error_label;
  GtkLabel *support_label;
};
typedef struct _GisFinishedPagePrivate GisFinishedPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisFinishedPage, gis_finished_page, GIS_TYPE_PAGE);

/* See endlessm/gnome-shell/js/gdm/authPrompt.js */
#define CUSTOMER_SUPPORT_FILENAME "vendor-customer-support.ini"
#define CUSTOMER_SUPPORT_GROUP_NAME "Customer Support"
#define CUSTOMER_SUPPORT_KEY_EMAIL "Email"
#define CUSTOMER_SUPPORT_EMAIL_FALLBACK "support@endlessm.com"

static const gchar * const customer_support_paths[] = {
  LOCALSTATEDIR "/lib/eos-image-defaults/" CUSTOMER_SUPPORT_FILENAME,
  DATADIR "/gnome-shell/" CUSTOMER_SUPPORT_FILENAME,
  NULL
};

static gchar *
get_customer_support_email (void)
{
  g_autoptr(GKeyFile) key_file = g_key_file_new ();
  const gchar * const *path_iter;
  g_autofree gchar *support_email = NULL;
  g_autoptr(GError) error = NULL;

  for (path_iter = customer_support_paths; *path_iter != NULL; path_iter++)
    {
      if (!g_key_file_load_from_file (key_file, *path_iter, G_KEY_FILE_NONE,
                                      &error))
        {
          if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
            g_warning ("%s: %s", *path_iter, error->message);

          g_clear_error (&error);
          continue;
        }

      support_email = g_key_file_get_locale_string (key_file,
                                                    CUSTOMER_SUPPORT_GROUP_NAME,
                                                    CUSTOMER_SUPPORT_KEY_EMAIL,
                                                    NULL, &error);
      if (support_email == NULL)
        {
          g_warning ("[%s] %s key not found in %s: %s",
                     CUSTOMER_SUPPORT_GROUP_NAME, CUSTOMER_SUPPORT_KEY_EMAIL,
                     *path_iter, error->message);
          g_clear_error (&error);
        }
      else
        {
          return g_steal_pointer (&support_email);
        }
    }

  g_warning ("[%s] %s key not found in any %s, using default",
             CUSTOMER_SUPPORT_GROUP_NAME, CUSTOMER_SUPPORT_KEY_EMAIL,
             CUSTOMER_SUPPORT_FILENAME);
  return g_strdup (CUSTOMER_SUPPORT_EMAIL_FALLBACK);
}

static void
reboot_cb (GtkButton *button, GisFinishedPage *page)
{
  g_spawn_command_line_sync ("/usr/bin/systemctl poweroff", NULL, NULL, NULL, NULL);
  g_application_quit(G_APPLICATION (GIS_PAGE (page)->driver));
}

static gboolean
toggle_leds (GisPage *page)
{
  XKeyboardControl values;
  GisFinishedPage *summary = GIS_FINISHED_PAGE (page);
  GisFinishedPagePrivate *priv = gis_finished_page_get_instance_private (summary);

  values.led_mode = priv->led_state;
  XChangeKeyboardControl(GDK_DISPLAY_XDISPLAY(gtk_widget_get_display(GTK_WIDGET(page))), KBLedMode, &values);
  priv->led_state = priv->led_state == 1 ? 0 : 1;
  return TRUE;
}

#define EOS_IMAGE_VERSION_XATTR "user.eos-image-version"
#define EOS_IMAGE_VERSION_PATH "/sysroot"
#define EOS_IMAGE_VERSION_ALT_PATH "/"

static gchar *
gis_page_util_get_image_version (const gchar *path,
                                 GError     **error)
{
  ssize_t attrsize;
  g_autofree gchar *value = NULL;

  g_return_val_if_fail (path != NULL, NULL);

  attrsize = getxattr (path, EOS_IMAGE_VERSION_XATTR, NULL, 0);
  if (attrsize >= 0)
    {
      value = g_malloc (attrsize + 1);
      value[attrsize] = 0;

      attrsize = getxattr (path, EOS_IMAGE_VERSION_XATTR, value,
                           attrsize);
    }

  if (attrsize >= 0)
    {
      return g_steal_pointer (&value);
    }
  else
    {
      int errsv = errno;
      g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errsv),
                   "Error examining " EOS_IMAGE_VERSION_XATTR " on %s: %s",
                   path, g_strerror (errsv));
      return NULL;
    }
}

static gboolean
is_eosdvd (void)
{
  g_autoptr(GError) error_sysroot = NULL;
  g_autoptr(GError) error_root = NULL;
  g_autofree char *image_version =
    gis_page_util_get_image_version (EOS_IMAGE_VERSION_PATH, &error_sysroot);

  if (image_version == NULL)
    image_version =
      gis_page_util_get_image_version (EOS_IMAGE_VERSION_ALT_PATH, &error_root);

  if (image_version == NULL)
    {
      g_warning ("%s", error_sysroot->message);
      g_warning ("%s", error_root->message);
    }

  return image_version != NULL &&
    g_str_has_prefix (image_version, "eosdvd-");
}

static void
gis_finished_page_shown (GisPage *page)
{
  GisFinishedPage *self = GIS_FINISHED_PAGE (page);
  GisFinishedPagePrivate *priv = gis_finished_page_get_instance_private (self);
  GError *error = gis_store_get_error();

  gis_driver_save_data (GIS_PAGE (page)->driver);

  if (error != NULL)
    {
      GisAssistant *assistant = gis_driver_get_assistant (page->driver);

      if (error->domain == GIS_UNATTENDED_ERROR)
        gtk_label_set_text (priv->error_heading_label,
                            _("Oops, something is wrong with your unattended installation configuration."));
      /* Otherwise, leave the default message (indicating a problem with the
       * image file). TODO: handle other domains that indicate problems
       * elsewhere.
       */

      gtk_label_set_text (priv->error_label, error->message);
      gtk_widget_show (priv->error_box);
      gtk_widget_hide (priv->success_box);
      gis_assistant_locale_changed (assistant);

      if (gis_store_is_unattended())
        {
          g_timeout_add_seconds (1, (GSourceFunc)toggle_leds, page);
        }
    }
  else
    {
      gboolean optical = is_eosdvd ();

      gtk_widget_set_visible (priv->removelabel_usb, !optical);
      gtk_widget_set_visible (priv->removelabel_dvd,  optical);
    }
}

static gboolean
write_unattended_config (gchar  **backup,
                         GError **error)
{
  GFile *image_file = NULL;
  GFile *image_dir = NULL;
  g_autoptr(GFile) unattended_ini_file = NULL;
  g_autofree gchar *unattended_ini = NULL;
  const gchar *locale = setlocale (LC_ALL, NULL);
  g_autofree gchar *image = NULL;
  UDisksBlock *block = NULL;
  g_autofree gchar *vendor = NULL;
  g_autofree gchar *product = NULL;
  const GError *existing_error = gis_store_get_error ();

  /* Refuse to write a new unattended.ini if installation failed, reminding the
   * user of the error.
   */
  if (existing_error != NULL)
    {
      g_propagate_error (error, g_error_copy (existing_error));
      return FALSE;
    }

  image_file = G_FILE (gis_store_get_object (GIS_STORE_IMAGE));
  image = g_file_get_basename (image_file);
  image_dir = G_FILE (gis_store_get_object (GIS_STORE_IMAGE_DIR));
  unattended_ini_file = g_file_get_child (image_dir, "unattended.ini");
  unattended_ini = g_file_get_path (unattended_ini_file);
  block = UDISKS_BLOCK (gis_store_get_object (GIS_STORE_BLOCK_DEVICE));

  if (!gis_dmi_read_vendor_product (&vendor, &product, error))
    return FALSE;

  return gis_unattended_config_write (unattended_ini, locale, image,
                                      udisks_block_get_device (block), vendor,
                                      product, backup, error);
}

static void
write_unattended_config_cb (GisFinishedPage *self)
{
  g_autofree gchar *backup = NULL;
  g_autoptr(GError) error = NULL;
  GtkMessageType message_type = GTK_MESSAGE_INFO;
  const gchar *message = NULL;
  g_autofree gchar *secondary = NULL;
  GtkWidget *dialog = NULL;

  if (write_unattended_config (&backup, &error))
    {
      message = _("Unattended installation configuration file created");

      if (backup != NULL)
        secondary = g_strdup_printf (_("The previous file was renamed to ‘%s’."),
                                     backup);
    }
  else
    {
      message_type = GTK_MESSAGE_ERROR;
      message = _("Unattended installation configuration file could not be created.");
      secondary = g_strdup (error->message);
    }

  dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))),
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   message_type,
                                   GTK_BUTTONS_OK,
                                   "%s", message);
  if (secondary != NULL)
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                              "%s", secondary);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

static void
gis_finished_page_constructed (GObject *object)
{
  GisFinishedPage *page = GIS_FINISHED_PAGE (object);
  GisFinishedPagePrivate *priv = gis_finished_page_get_instance_private (page);
  g_autoptr(GClosure) closure = NULL;

  G_OBJECT_CLASS (gis_finished_page_parent_class)->constructed (object);

  gis_page_set_complete (GIS_PAGE (page), TRUE);

  g_signal_connect (priv->restart_button, "clicked", G_CALLBACK (reboot_cb), page);

  /* Use Ctrl+U to write unattended config */
  priv->accel_group = gtk_accel_group_new ();
  closure = g_cclosure_new_swap (G_CALLBACK (write_unattended_config_cb), page, NULL);
  gtk_accel_group_connect (priv->accel_group, GDK_KEY_u, GDK_CONTROL_MASK, 0, closure);

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_finished_page_dispose (GObject *object)
{
  GisFinishedPage *self = GIS_FINISHED_PAGE (object);
  GisFinishedPagePrivate *priv = gis_finished_page_get_instance_private (self);

  g_clear_object (&priv->accel_group);

  G_OBJECT_CLASS (gis_finished_page_parent_class)->dispose (object);
}

static GtkBuilder *
gis_finished_page_get_builder (GisPage *page)
{
  return NULL;
}

static void
gis_finished_page_locale_changed (GisPage *page)
{
  GisFinishedPage *self = GIS_FINISHED_PAGE (page);
  GisFinishedPagePrivate *priv = gis_finished_page_get_instance_private (self);
  g_autofree gchar *support_email = NULL;
  g_autofree gchar *support_email_markup = NULL;
  g_autofree gchar *support_markup = NULL;

  if (gis_store_get_error() == NULL)
    {
      gis_page_set_title (page, _("Reformatting Finished"));
    }
  else
    {
      gis_page_set_title (page, "");
    }

  support_email = get_customer_support_email ();
  support_email_markup = g_strdup_printf ("<a href=\"mailto:%1$s\">%1$s</a>",
                                          support_email);
  /* Translators: the %s is the customer support email address */
  support_markup = g_strdup_printf (_("Please contact %s or join the <a href=\"https://community.endlessos.com/\">Endless Community</a> to troubleshoot."),
                                    support_email_markup);
  gtk_label_set_markup (priv->support_label, support_markup);
}

static GtkAccelGroup *
gis_finished_page_get_accel_group (GisPage *page)
{
  GisFinishedPage *self = GIS_FINISHED_PAGE (page);
  GisFinishedPagePrivate *priv = gis_finished_page_get_instance_private (self);

  return priv->accel_group;
}

static void
gis_finished_page_class_init (GisFinishedPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-finished-page.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisFinishedPage, success_box);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisFinishedPage, removelabel_usb);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisFinishedPage, removelabel_dvd);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisFinishedPage, restart_button);

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisFinishedPage, error_box);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisFinishedPage, error_heading_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisFinishedPage, error_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisFinishedPage, support_label);

  page_class->page_id = PAGE_ID;
  page_class->get_builder = gis_finished_page_get_builder;
  page_class->locale_changed = gis_finished_page_locale_changed;
  page_class->get_accel_group = gis_finished_page_get_accel_group;
  page_class->shown = gis_finished_page_shown;
  object_class->constructed = gis_finished_page_constructed;
  object_class->dispose = gis_finished_page_dispose;
}

static void
gis_finished_page_init (GisFinishedPage *self)
{
  g_resources_register (finished_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));
}

void
gis_prepare_finished_page (GisDriver *driver)
{
  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_FINISHED_PAGE,
                                     "driver", driver,
                                     NULL));
}
