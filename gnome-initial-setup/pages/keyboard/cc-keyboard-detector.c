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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "cc-keyboard-detector.h"

KeyboardDetector *
keyboard_detector_new (void)
{
  GError *error = NULL;
  GInputStream *istream;
  KeyboardDetector *det;
  const gchar * const *language_names = g_get_language_names ();
  const gchar *language_name;
  int idx;

  /* Find the detector tree that is the best match for the user's language. */
  for (idx = 0; (language_name = language_names[idx]) != NULL; idx++)
    {
      gchar *path = g_strdup_printf (
          "/org/gnome/initial-setup/detector-trees/%s/pc105.tree",
          language_name);
      g_clear_error (&error);
      istream = g_resources_open_stream (path,
                                         G_RESOURCE_LOOKUP_FLAGS_NONE, &error);
      g_free (path);

      if (istream == NULL)
        {
          g_debug ("Unable to load keyboard detector tree for %s: %s",
                   language_name, error->message);
          /* Don't clear the error here, as we need the message
           * for the last error outside the loop.  Instead, we will
           * clear the error at the start of the next iteration. */
        }
      else
        {
          g_debug ("Successfully loaded keyboard detector tree for %s",
                   language_name);
          break;
        }
    }

  if (istream == NULL)
    g_error ("Error loading keyboard detector tree: %s", error->message);

  det = g_new0 (KeyboardDetector, 1);

  det->current_step = -1;
  det->fp = g_data_input_stream_new (istream);
  g_object_unref (istream);

  det->keycodes = g_hash_table_new (NULL, NULL);
  det->symbols = NULL;
  det->present = -1;
  det->not_present = -1;
  det->result = NULL;
  det->step_type = UNKNOWN;

  return det;
}

static void
keyboard_detector_clear (KeyboardDetector *det)
{
  g_hash_table_remove_all (det->keycodes);
  g_list_foreach (det->symbols, (GFunc) g_free, NULL);
  g_clear_pointer (&det->symbols, g_list_free);
  det->present = -1;
  det->not_present = -1;
  det->step_type = UNKNOWN;
  g_clear_pointer (&det->result, g_free);
}

void
keyboard_detector_free (KeyboardDetector *det)
{
  keyboard_detector_clear (det);
  g_object_unref (det->fp);
  g_hash_table_destroy (det->keycodes);
  g_free (det);
}

/* Return value: TRUE if should read another line */
static gboolean
process_line (KeyboardDetector         *det,
              KeyboardDetectorStepType  step,
              char                     *line,
              KeyboardDetectorStepType *result)
{
  if (g_str_has_prefix (line, "STEP "))
    {
      /* This line starts a new step. */
      int new_step = atoi (line + 5);
      if (det->current_step == step)
        {
          det->current_step = new_step;
          *result = det->step_type;
          return FALSE;
        }
      else
        {
          det->current_step = new_step;
        }
    }
  else if (det->current_step != step)
    return TRUE;
  else if (g_str_has_prefix (line, "PRESS "))
    {
      /* Ask the user to press a character on the keyboard. */
      if (det->step_type == UNKNOWN)
        det->step_type = PRESS_KEY;
      if (det->step_type != PRESS_KEY)
        {
          *result = ERROR;
          return FALSE;
        }
      char *key_symbol = g_strdup (g_strstrip (line + 6));
      det->symbols = g_list_append (det->symbols, key_symbol);
    }
  else if (g_str_has_prefix (line, "CODE "))
    {
      /* Direct the evaluating code to process step ## next if the
       * user has pressed a key which returned that keycode.
       */
      if (det->step_type != PRESS_KEY)
        {
          *result = ERROR;
          return FALSE;
        }
      char *keycode = strtok (line + 5, " ");
      char *s = strtok (NULL, " ");
      int code = atoi (keycode);
      int next_step = atoi (s);
      g_hash_table_insert (det->keycodes, GINT_TO_POINTER (code), GINT_TO_POINTER (next_step));
    }
  else if (g_str_has_prefix (line, "FIND "))
    {
      /* Ask the user whether that character is present on their
       * keyboard.
       */
      if (det->step_type == UNKNOWN)
        {
          det->step_type = KEY_PRESENT;
        }
      else
        {
          *result = ERROR;
          return FALSE;
        }
      char *key_symbol = g_strdup (g_strstrip (line + 5));
      det->symbols = g_list_prepend (det->symbols, key_symbol);
    }
  else if (g_str_has_prefix (line, "FINDP "))
    {
      /* Equivalent to FIND, except that the user is asked to consider
       * only the primary symbols (i.e. Plain and Shift).
       */
      if (det->step_type == UNKNOWN)
        {
          det->step_type = KEY_PRESENT_P;
        }
      else
        {
          *result = ERROR;
          return FALSE;
        }
      char *key_symbol = g_strdup (g_strstrip (line + 6));
      det->symbols = g_list_prepend (det->symbols, key_symbol);
    }
  else if (g_str_has_prefix (line, "YES "))
    {
      /* Direct the evaluating code to process step ## next if the
       * user does have this key.
       */
      if (det->step_type != KEY_PRESENT_P &&
          det->step_type != KEY_PRESENT)
        {
          *result = ERROR;
          return FALSE;
        }
      det->present = atoi (g_strstrip (line + 4));
    }
  else if (g_str_has_prefix (line, "NO "))
    {
      /* Direct the evaluating code to process step ## next if the
       * user does not have this key.
       */
      if (det->step_type != KEY_PRESENT_P &&
          det->step_type != KEY_PRESENT)
        {
          *result = ERROR;
          return FALSE;
        }
      det->not_present = atoi (g_strstrip (line + 3));
    }
  else if (g_str_has_prefix (line, "MAP "))
    {
      /* This step uniquely identifies a keymap. */
      if (det->step_type == UNKNOWN)
        det->step_type = RESULT;
      det->result = g_strdup (g_strstrip (line + 4));
      /* The Ubuntu file uses colons to separate country codes from layout
       * variants, and GnomeXkb requires plus signs.
       */
      char *colon_pointer = strchr (det->result, ':');
      if (colon_pointer != NULL)
        *colon_pointer = '+';
      *result = det->step_type;
      return FALSE;
    }
  else
    {
      *result = ERROR;
      return FALSE;
    }
  return TRUE;
}

KeyboardDetectorStepType
keyboard_detector_read_step (KeyboardDetector *det,
                             int               step)
{
  char *line;
  size_t len;

  if (det->current_step != -1)
    {
      GList *valid_steps = g_hash_table_get_values (det->keycodes);
      gboolean found = (g_list_find (valid_steps, GINT_TO_POINTER (step)) != NULL);
      g_list_free (valid_steps);
      if (!(found || step == det->present || step == det->not_present))
        /* Invalid argument */
        return ERROR;
      if (det->result)
        /* Already done */
        return ERROR;
    }

  keyboard_detector_clear (det);

  while ((line = g_data_input_stream_read_line_utf8 (det->fp, &len, NULL, NULL)) != NULL)
    {
      KeyboardDetectorStepType retval;
      gboolean should_continue = process_line (det, step, line, &retval);
      g_free (line);
      if (!should_continue)
        return retval;
    }

  /* The requested step was not found. */
  return ERROR;
}
