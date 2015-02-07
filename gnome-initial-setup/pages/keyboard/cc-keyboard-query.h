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

#ifndef CC_KEYBOARD_QUERY_H
#define CC_KEYBOARD_QUERY_H

#include <glib-object.h>
#include <gtk/gtk.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-xkb-info.h>

G_BEGIN_DECLS

#define CC_TYPE_KEYBOARD_QUERY cc_keyboard_query_get_type()
#define CC_KEYBOARD_QUERY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), CC_TYPE_KEYBOARD_QUERY, CcKeyboardQuery))
#define CC_KEYBOARD_QUERY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), CC_TYPE_KEYBOARD_QUERY, CcKeyboardQueryClass))
#define CC_IS_KEYBOARD_QUERY(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CC_TYPE_KEYBOARD_QUERY))
#define CC_IS_KEYBOARD_QUERY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CC_TYPE_KEYBOARD_QUERY))
#define CC_KEYBOARD_QUERY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), CC_TYPE_KEYBOARD_QUERY, CcKeyboardQueryClass))

typedef struct _CcKeyboardQuery CcKeyboardQuery;
typedef struct _CcKeyboardQueryClass CcKeyboardQueryClass;

struct _CcKeyboardQuery
{
  GtkDialog parent;
};

struct _CcKeyboardQueryClass
{
  GtkDialogClass parent_class;

  /* signals */
  void (*layout_result)(CcKeyboardQuery *, const char *);
};

GType      cc_keyboard_query_get_type     (void) G_GNUC_CONST;
GtkWidget *cc_keyboard_query_new          (GtkWindow       *main_window);
void       cc_keyboard_query_run          (CcKeyboardQuery *self);
gboolean   cc_keyboard_query_get_selected (CcKeyboardQuery *self,
                                           char           **id,
                                           char           **name);

G_END_DECLS

#endif /* CC_KEYBOARD_QUERY_H */
