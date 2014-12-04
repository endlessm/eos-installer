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

#ifndef CC_KEY_ROW_H
#define CC_KEY_ROW_H

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_KEY_ROW cc_key_row_get_type()
#define CC_KEY_ROW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), CC_TYPE_KEY_ROW, CcKeyRow))
#define CC_KEY_ROW_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), CC_TYPE_KEY_ROW, CcKeyRowClass))
#define CC_IS_KEY_ROW(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CC_TYPE_KEY_ROW))
#define CC_IS_KEY_ROW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CC_TYPE_KEY_ROW))
#define CC_KEY_ROW_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), CC_TYPE_KEY_ROW, CcKeyRowClass))

typedef struct _CcKeyRow CcKeyRow;
typedef struct _CcKeyRowClass CcKeyRowClass;

struct _CcKeyRow
{
  GtkBox parent;
};

struct _CcKeyRowClass
{
  GtkBoxClass parent_class;
};

GType      cc_key_row_get_type      (void) G_GNUC_CONST;
GtkWidget *cc_key_row_new           (void);
void       cc_key_row_add_character (CcKeyRow   *self,
                                     const char *label);
void       cc_key_row_clear         (CcKeyRow   *self);

G_END_DECLS

#endif /* CC_KEY_ROW_H */
