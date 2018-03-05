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
 *
 */

/* Confirm page {{{1 */

#define PAGE_ID "confirm"

#include "config.h"
#include "confirm-resources.h"
#include "gis-confirm-page.h"
#include "gis-dmi.h"
#include "gis-errors.h"
#include "gis-store.h"

#include <udisks/udisks.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <errno.h>

struct _GisConfirmPagePrivate {
    GtkLabel *vendor_label;
    GtkLabel *product_label;

    GtkLabel *image1_label;
    GtkLabel *model1_label;
    GtkLabel *device1_label;
    GtkLabel *size1_label;

    GtkBox *warning_box;
    GtkLabel *warning_label;

    GtkBox *wrong_computer_box;

    GtkLabel *countdown_label;
    GtkButton *cancel_button;
    GtkButton *continue_button;

    guint countdown_source;
    guint countdown_remaining_seconds;
};
typedef struct _GisConfirmPagePrivate GisConfirmPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisConfirmPage, gis_confirm_page, GIS_TYPE_PAGE);

static void
gis_confirm_page_advance (GisConfirmPage *self)
{
  GisConfirmPagePrivate *priv = gis_confirm_page_get_instance_private (self);
  GisPage *page = GIS_PAGE (self);

  if (priv->countdown_source != 0)
    {
      g_source_remove (priv->countdown_source);
      priv->countdown_source = 0;
    }

  gis_assistant_next_page (gis_driver_get_assistant (page->driver));
}

static gboolean
gis_confirm_page_countdown_cb (gpointer data)
{
  GisConfirmPage *self = GIS_CONFIRM_PAGE (data);
  GisConfirmPagePrivate *priv = gis_confirm_page_get_instance_private (self);

  if (priv->countdown_remaining_seconds == 0)
    {
      gis_confirm_page_advance (self);
      return G_SOURCE_REMOVE;
    }
  else
    {
      g_autofree gchar *countdown_message =
        g_strdup_printf (g_dngettext (GETTEXT_PACKAGE,
                                      "Installation will commence automatically in %d second.",
                                      "Installation will commence automatically in %d seconds.",
                                      priv->countdown_remaining_seconds),
                         priv->countdown_remaining_seconds);
      gtk_label_set_text (priv->countdown_label, countdown_message);
      priv->countdown_remaining_seconds--;
      return G_SOURCE_CONTINUE;
    }
}

static void
gis_confirm_page_start_countdown (GisConfirmPage *self)
{
  GisConfirmPagePrivate *priv = gis_confirm_page_get_instance_private (self);

  /* This magic number was chosen to match the default timeout on our boot menu
   * on dual-boot systems, which in turn is chosen to match the Windows boot
   * menu's default timeout. Anecdotally, a 10-second pause at the boot menu
   * was short enough that people were not able to catch it in time.
   *
   * When booting from USB, our boot process takes of the order of 1 minute,
   * which is long enough for people to get bored and lose focus. 30 seconds
   * seems long enough to notice activity on the screen out of the corner of
   * your eye, turn back to the computer, read the message, and frantically
   * press the Cancel button. Reformat times for non-base images are measured
   * in low tens of minutes, so an additional 30 seconds is a small price to
   * pay; an impatient operator can always press the big red button.
   */
  priv->countdown_remaining_seconds = 30;
  gis_confirm_page_countdown_cb (self);

  priv->countdown_source = g_timeout_add_seconds (1,
                                                  gis_confirm_page_countdown_cb,
                                                  self);
}

static void
gis_confirm_page_shown (GisPage *page)
{
  GisConfirmPage *self = GIS_CONFIRM_PAGE (page);
  GisConfirmPagePrivate *priv = gis_confirm_page_get_instance_private (self);
  GFile *image_file;
  g_autofree gchar *image_basename = NULL;
  UDisksClient *udisks_client;
  UDisksBlock *target_block;
  /* TODO: given support in libudisks2, use g_autoptr(UDisksDrive) here and
   * remove g_clear_object() below.
   */
  UDisksDrive *target_drive = NULL;
  g_autofree gchar *target_model = NULL;
  g_autofree gchar *target_size = NULL;
  g_autofree gchar *vendor = NULL;
  g_autofree gchar *product = NULL;
  g_autofree gchar *unknown_markup = g_markup_printf_escaped ("<i>%s</i>",
                                                              _("Unknown"));
  GisUnattendedConfig *config;
  GisUnattendedComputerMatch match;
  g_autoptr(GError) error = NULL;

  if (gis_store_get_error() != NULL)
    {
      gis_confirm_page_advance (self);
      return;
    }

  if (!gis_dmi_read_vendor_product (&vendor, &product, &error))
    {
      g_warning ("Failed to read DMI data: %s", error->message);
      g_clear_error (&error);
    }

  if (vendor != NULL)
    gtk_label_set_text (priv->vendor_label, vendor);
  else
    gtk_label_set_markup (priv->vendor_label, unknown_markup);

  if (product != NULL)
    gtk_label_set_text (priv->product_label, product);
  else
    gtk_label_set_markup (priv->product_label, unknown_markup);

  image_file = G_FILE (gis_store_get_object (GIS_STORE_IMAGE));
  image_basename = g_file_get_basename (image_file);
  gtk_label_set_text (priv->image1_label, image_basename);
  udisks_client = UDISKS_CLIENT (gis_store_get_object (GIS_STORE_UDISKS_CLIENT));
  target_block = UDISKS_BLOCK (gis_store_get_object (GIS_STORE_BLOCK_DEVICE));
  target_drive = udisks_client_get_drive_for_block (udisks_client, target_block);

  g_assert (target_drive != NULL);

  target_model = g_strdup_printf ("%s %s",
                                  udisks_drive_get_vendor (target_drive),
                                  udisks_drive_get_model  (target_drive));
  g_strstrip (target_model);
  gtk_label_set_text (priv->model1_label, target_model);
  gtk_label_set_text (priv->device1_label, udisks_block_get_device (target_block));
  target_size = g_format_size (udisks_drive_get_size (target_drive));
  gtk_label_set_text (priv->size1_label, target_size);

  g_clear_object (&target_drive);

  config = gis_store_get_unattended_config ();
  g_assert (config != NULL);
  match = gis_unattended_config_match_computer (config, vendor, product);
  gtk_widget_set_visible (GTK_WIDGET (priv->countdown_label),
                          match == GIS_UNATTENDED_COMPUTER_MATCHES);
  gtk_widget_set_visible (GTK_WIDGET (priv->wrong_computer_box),
                          match == GIS_UNATTENDED_COMPUTER_DOES_NOT_MATCH);
  switch (match)
    {
    case GIS_UNATTENDED_COMPUTER_MATCHES:
      gis_confirm_page_start_countdown (self);
      break;
    case GIS_UNATTENDED_COMPUTER_DOES_NOT_MATCH:
      gtk_button_set_label (priv->continue_button,
                            _("C_ontinue Anyway and Destroy Data"));
      break;
    case GIS_UNATTENDED_COMPUTER_NOT_SPECIFIED:
    default:
      /* no action needed */
      break;
    }

  gtk_widget_grab_default (GTK_WIDGET (priv->cancel_button));
  gtk_widget_grab_focus (GTK_WIDGET (priv->cancel_button));
}

static void
gis_confirm_page_cancel_clicked (GtkButton *button,
                                 GisConfirmPage *self)
{
  g_autoptr(GError) error =
    g_error_new_literal (G_IO_ERROR,
                         G_IO_ERROR_CANCELLED,
                         _("Unattended installation was cancelled."));

  gis_store_set_error (error);
  gis_confirm_page_advance (self);
}

static void
gis_confirm_page_continue_clicked (GtkButton *button,
                                   GisConfirmPage *self)
{
  gis_confirm_page_advance (self);
}

static void
gis_confirm_page_constructed (GObject *object)
{
  GisConfirmPage *self = GIS_CONFIRM_PAGE (object);
  GisConfirmPagePrivate *priv = gis_confirm_page_get_instance_private (self);

  G_OBJECT_CLASS (gis_confirm_page_parent_class)->constructed (object);

  g_signal_connect (priv->cancel_button,
                    "clicked",
                    G_CALLBACK (gis_confirm_page_cancel_clicked),
                    self);
  g_signal_connect (priv->continue_button,
                    "clicked",
                    G_CALLBACK (gis_confirm_page_continue_clicked),
                    self);

  gtk_widget_show (GTK_WIDGET (self));
}

static void
gis_confirm_page_locale_changed (GisPage *page)
{
  gis_page_set_title (page, _("Endless OS Unattended Installation"));
}

static void
gis_confirm_page_class_init (GisConfirmPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-confirm-page.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisConfirmPage, vendor_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisConfirmPage, product_label);

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisConfirmPage, image1_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisConfirmPage, model1_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisConfirmPage, device1_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisConfirmPage, size1_label);

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisConfirmPage, warning_box);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisConfirmPage, warning_label);

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisConfirmPage, wrong_computer_box);

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisConfirmPage, countdown_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisConfirmPage, cancel_button);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisConfirmPage, continue_button);

  page_class->hide_forward_button = TRUE;
  page_class->hide_backward_button = TRUE;
  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_confirm_page_locale_changed;
  page_class->shown = gis_confirm_page_shown;
  object_class->constructed = gis_confirm_page_constructed;
}

static void
gis_confirm_page_init (GisConfirmPage *self)
{
  g_resources_register (confirm_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));
}

void
gis_prepare_confirm_page (GisDriver *driver)
{
  if (gis_store_is_unattended ())
    gis_driver_add_page (driver,
                         g_object_new (GIS_TYPE_CONFIRM_PAGE,
                                       "driver", driver,
                                       NULL));
}
