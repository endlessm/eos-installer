/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2014 Endless Mobile, Inc.
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

#include "cc-key-row.h"

G_DEFINE_TYPE (CcKeyRow, cc_key_row, GTK_TYPE_BOX);

static void
cc_key_row_class_init (CcKeyRowClass *klass)
{
}

static void
cc_key_row_init (CcKeyRow *self)
{
  gtk_box_set_spacing (GTK_BOX (self), 24);
}

GtkWidget *
cc_key_row_new (void)
{
  return g_object_new (CC_TYPE_KEY_ROW, NULL);
}

void
cc_key_row_add_character (CcKeyRow   *self,
                          const char *key_label)
{
  char *label_string = g_strdup_printf ("<big>%s</big>", key_label);
  GtkWidget *label = gtk_label_new (label_string);
  g_free (label_string);

  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX (self), label, TRUE, TRUE, 0);
  gtk_widget_show (label);
}

void
cc_key_row_clear (CcKeyRow *self)
{
  GList *children = gtk_container_get_children (GTK_CONTAINER (self));
  g_list_foreach (children, (GFunc) gtk_widget_destroy, NULL);
  g_list_free (children);
}
