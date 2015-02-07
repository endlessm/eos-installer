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

#include <config.h>
#include <glib/gi18n.h>

#include "cc-keyboard-detector.h"
#include "cc-keyboard-query.h"
#include "cc-key-row.h"

typedef struct
{
  KeyboardDetector *det;
  char *detected_id;
  char *detected_display_name;

  const char *press_string;
  const char *present_string;

  GtkWidget *vbox;
  GtkWidget *heading;
  GtkWidget *keyrow;
  GtkWidget *buttons;
  GtkWidget *add_button;

  GnomeXkbInfo *xkb_data;
} CcKeyboardQueryPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (CcKeyboardQuery, cc_keyboard_query, GTK_TYPE_DIALOG);

enum {
  LAYOUT_RESULT,
  N_SIGNALS,
};

static guint cc_keyboard_query_signals[N_SIGNALS] = { 0, };

static void
process (CcKeyboardQuery         *self,
         KeyboardDetectorStepType result)
{
  CcKeyboardQueryPrivate *priv = cc_keyboard_query_get_instance_private (self);
  GList *iter;

  cc_key_row_clear (CC_KEY_ROW (priv->keyrow));
  for (iter = priv->det->symbols; iter != NULL; iter = iter->next)
    cc_key_row_add_character (CC_KEY_ROW (priv->keyrow), iter->data);

  switch (result)
    {
    case PRESS_KEY:
      gtk_label_set_label (GTK_LABEL (priv->heading), priv->press_string);
      gtk_widget_hide (GTK_WIDGET (priv->buttons));
      break;
    case KEY_PRESENT:
    case KEY_PRESENT_P:
      gtk_label_set_label (GTK_LABEL (priv->heading), priv->present_string);
      gtk_widget_show (GTK_WIDGET (priv->buttons));
      break;
    case RESULT:
      g_signal_emit (self, cc_keyboard_query_signals[LAYOUT_RESULT], 0,
                     priv->det->result);
      break;
    case ERROR:
      gtk_widget_hide (GTK_WIDGET (self));
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
have_key (GtkButton       *button,
          CcKeyboardQuery *self)
{
  CcKeyboardQueryPrivate *priv = cc_keyboard_query_get_instance_private (self);
  KeyboardDetectorStepType result;

  result = keyboard_detector_read_step (priv->det, priv->det->present);
  process (self, result);
}

static void
no_have_key (GtkButton       *button,
             CcKeyboardQuery *self)
{
  CcKeyboardQueryPrivate *priv = cc_keyboard_query_get_instance_private (self);
  KeyboardDetectorStepType result;

  result = keyboard_detector_read_step (priv->det, priv->det->not_present);
  process (self, result);
}

static gboolean
key_press_event (GtkWidget       *widget,
                 GdkEventKey     *event,
                 CcKeyboardQuery *self)
{
  CcKeyboardQueryPrivate *priv = cc_keyboard_query_get_instance_private (self);
  KeyboardDetectorStepType result;
  int code, new_step;
  gpointer c;

  /* FIXME need to account for possible remapping.  Find the API to translate
   * kernel keycodes to X keycodes (xkb).
   * MIN_KEYCODE = 8
   */

  /* FIXME escape should close the window. */

  code = event->hardware_keycode - 8;
  if (code > 255)
    return GDK_EVENT_PROPAGATE;
  /* XKB doesn't support keycodes > 255. */

  c = g_hash_table_lookup (priv->det->keycodes, GINT_TO_POINTER (code));
  if (c == NULL)
    return GDK_EVENT_PROPAGATE;

  new_step = GPOINTER_TO_INT (c);
  result = keyboard_detector_read_step (priv->det, new_step);
  process (self, result);
  return GDK_EVENT_STOP;
}

static void
cc_keyboard_query_constructed (GObject *object)
{
  CcKeyboardQuery *self = CC_KEYBOARD_QUERY (object);
  CcKeyboardQueryPrivate *priv = cc_keyboard_query_get_instance_private (self);

  priv->keyrow = cc_key_row_new ();
  gtk_box_pack_start (GTK_BOX (priv->vbox), priv->keyrow, FALSE, TRUE, 0);
  gtk_box_reorder_child (GTK_BOX (priv->vbox), priv->keyrow, 1);

  gtk_widget_hide (priv->buttons);

  G_OBJECT_CLASS (cc_keyboard_query_parent_class)->constructed (object);
}

static void
cc_keyboard_query_finalize (GObject *object)
{
  CcKeyboardQuery *self = CC_KEYBOARD_QUERY (object);
  CcKeyboardQueryPrivate *priv = cc_keyboard_query_get_instance_private (self);

  g_clear_object (&priv->xkb_data);

  g_clear_pointer (&priv->det, keyboard_detector_free);
  g_clear_pointer (&priv->detected_id, g_free);
  g_clear_pointer (&priv->detected_display_name, g_free);

  G_OBJECT_CLASS (cc_keyboard_query_parent_class)->finalize (object);
}

/* Default handler for CcKeyboardQuery::layout-result */
static void
cc_keyboard_query_layout_result (CcKeyboardQuery *self,
                                 const char      *result)
{
  CcKeyboardQueryPrivate *priv = cc_keyboard_query_get_instance_private (self);
  char *result_message;
  const char *display_name = NULL;

  priv->detected_id = g_strdup (result);

  gnome_xkb_info_get_layout_info (priv->xkb_data, result, &display_name, NULL,
                                  NULL, NULL);

  priv->detected_display_name = g_strdup (display_name);
  result_message = g_strdup_printf ("%s\n%s",
                                    _("Your keyboard layout seems to be:"),
                                    display_name ? display_name : result);
  gtk_label_set_label (GTK_LABEL (priv->heading), result_message);
  gtk_widget_hide (priv->buttons);
  gtk_widget_hide (priv->keyrow);
  gtk_widget_set_sensitive (priv->add_button, TRUE);

  g_free (result_message);
}

static gboolean
is_event_on_title (CcKeyboardQuery *self,
                   GdkEventButton *event)
{
  GtkAllocation allocation;
  GtkWidget *titlebar, *src;
  gint x, y;

  titlebar = gtk_dialog_get_header_bar (GTK_DIALOG (self));

  gdk_window_get_user_data (event->window, (gpointer *)&src);
  if (src && src != GTK_WIDGET (self))
    {
      gtk_widget_translate_coordinates (src, GTK_WIDGET (self),
                                        event->x, event->y, &x, &y);
    }
  else
    {
      x = event->x;
      y = event->y;
    }

  if (titlebar != NULL &&
      gtk_widget_get_visible (titlebar) &&
      gtk_widget_get_child_visible (titlebar))
    {
      gtk_widget_get_allocation (titlebar, &allocation);
      if (allocation.x <= x && allocation.x + allocation.width > x &&
          allocation.y <= y && allocation.y + allocation.height > y)
        return TRUE;
    }

  return FALSE;
}

static gboolean
cc_keyboard_query_button_press_event (GtkWidget      *widget,
                                      GdkEventButton *event)
{
  CcKeyboardQuery *self = CC_KEYBOARD_QUERY (widget);

  /* eat all the right clicks on the titlebar, since we run in a special session */
  if (is_event_on_title (self, event) &&
      event->button == GDK_BUTTON_SECONDARY)
    return TRUE;

  return GTK_WIDGET_CLASS (cc_keyboard_query_parent_class)->button_press_event (widget, event);
}

static void
cc_keyboard_query_realize (GtkWidget *widget)
{
  GdkWindow *window;

  GTK_WIDGET_CLASS (cc_keyboard_query_parent_class)->realize (widget);

  window = gtk_widget_get_window (widget);
  /* disable all the WM functions */
  gdk_window_set_functions (window, GDK_FUNC_ALL
                            | GDK_FUNC_RESIZE
                            | GDK_FUNC_MOVE
                            | GDK_FUNC_MINIMIZE
                            | GDK_FUNC_MAXIMIZE
                            | GDK_FUNC_CLOSE);
}

static void
cc_keyboard_query_class_init (CcKeyboardQueryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  cc_keyboard_query_signals[LAYOUT_RESULT] =
    g_signal_new ("layout-result", CC_TYPE_KEYBOARD_QUERY, G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (CcKeyboardQueryClass, layout_result),
                  NULL, NULL, g_cclosure_marshal_VOID__STRING, G_TYPE_NONE,
                  1, G_TYPE_STRING);

  object_class->constructed = cc_keyboard_query_constructed;
  object_class->finalize = cc_keyboard_query_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/initial-setup/keyboard-detector.ui");
  gtk_widget_class_bind_template_child_private (widget_class, CcKeyboardQuery, vbox);
  gtk_widget_class_bind_template_child_private (widget_class, CcKeyboardQuery, heading);
  gtk_widget_class_bind_template_child_private (widget_class, CcKeyboardQuery, buttons);
  gtk_widget_class_bind_template_child_private (widget_class, CcKeyboardQuery, add_button);
  gtk_widget_class_bind_template_callback (widget_class, have_key);
  gtk_widget_class_bind_template_callback (widget_class, no_have_key);
  gtk_widget_class_bind_template_callback (widget_class, key_press_event);

  widget_class->realize = cc_keyboard_query_realize;
  widget_class->button_press_event = cc_keyboard_query_button_press_event;

  klass->layout_result = cc_keyboard_query_layout_result;
}

static void
cc_keyboard_query_init (CcKeyboardQuery *self)
{
  CcKeyboardQueryPrivate *priv = cc_keyboard_query_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));

  priv->press_string = _("Please press one of the following keys:");
  priv->present_string = _("Is the following key present on your keyboard?");

  priv->det = keyboard_detector_new ();
  priv->xkb_data = gnome_xkb_info_new ();
}

GtkWidget *
cc_keyboard_query_new (GtkWindow    *main_window)
{
  return g_object_new (CC_TYPE_KEYBOARD_QUERY,
                       "transient-for", main_window,
                       "use-header-bar", TRUE,
                       NULL);
}

void
cc_keyboard_query_run (CcKeyboardQuery *self)
{
  CcKeyboardQueryPrivate *priv = cc_keyboard_query_get_instance_private (self);
  KeyboardDetectorStepType result;

  gtk_widget_show_all (GTK_WIDGET (self));
  result = keyboard_detector_read_step (priv->det, 0);
  process (self, result);
}

gboolean
cc_keyboard_query_get_selected (CcKeyboardQuery *self,
                                gchar          **id,
                                gchar          **name)
{
  CcKeyboardQueryPrivate *priv = cc_keyboard_query_get_instance_private (self);

  if (!priv->detected_id)
    return FALSE;

  if (id != NULL)
    *id = g_strdup (priv->detected_id);
  if (name != NULL)
    *name = g_strdup (priv->detected_display_name);

  return TRUE;
}
