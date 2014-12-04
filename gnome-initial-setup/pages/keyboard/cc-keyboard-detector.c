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

#include "cc-keyboard-detector.h"

KeyboardDetector *
keyboard_detector_new (void)
{
  GError *error = NULL;
  GInputStream *istream;
  KeyboardDetector *det = g_new0 (KeyboardDetector, 1);

  det->current_step = -1;
  istream = g_resources_open_stream ("/org/gnome/initial-setup/pc105.tree",
                                     G_RESOURCE_LOOKUP_FLAGS_NONE, &error);
  if (istream == NULL)
    g_error ("Error loading keyboard detector tree: %s", error->message);
  det->fp = g_data_input_stream_new (istream);
  g_object_unref (istream);

  det->keycodes = g_hash_table_new (NULL, NULL);
  det->symbols = NULL;
  det->present = -1;
  det->not_present = -1;
  det->result = NULL;

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

  KeyboardDetectorStepType step_type = UNKNOWN;
  keyboard_detector_clear (det);

  while ((line = g_data_input_stream_read_line_utf8 (det->fp, &len, NULL, NULL)) != NULL)
    {
      if (g_str_has_prefix (line, "STEP "))
        {
          /* This line starts a new step. */
          int new_step = atoi (line + 5);
          g_free (line);
          if (det->current_step == step)
            {
              det->current_step = new_step;
              return step_type;
            }
          else
            {
              det->current_step = new_step;
            }
        }
      else if (det->current_step != step)
        {
          g_free (line);
          continue;
        }
      else if (g_str_has_prefix (line, "PRESS "))
        {
          /* Ask the user to press a character on the keyboard. */
          if (step_type == UNKNOWN)
            step_type = PRESS_KEY;
          if (step_type != PRESS_KEY)
            {
              g_free (line);
              return ERROR;
            }
          char *key_symbol = g_strdup (g_strstrip (line + 6));
          g_free (line);
          det->symbols = g_list_append (det->symbols, key_symbol);
        }
      else if (g_str_has_prefix (line, "CODE "))
        {
          /* Direct the evaluating code to process step ## next if the
           * user has pressed a key which returned that keycode.
           */
          if (step_type != PRESS_KEY)
            {
              g_free (line);
              return ERROR;
            }
          char *keycode = strtok (line + 5, " ");
          char *s = strtok (NULL, " ");
          int code = atoi (keycode);
          int next_step = atoi (s);
          g_free (line);
          g_hash_table_insert (det->keycodes, GINT_TO_POINTER (code), GINT_TO_POINTER (next_step));
        }
      else if (g_str_has_prefix (line, "FIND "))
        {
          /* Ask the user whether that character is present on their
           * keyboard.
           */
          if (step_type == UNKNOWN)
            {
              step_type = KEY_PRESENT;
            }
          else
            {
              g_free (line);
              return ERROR;
            }
          char *key_symbol = g_strdup (g_strstrip (line + 5));
          g_free (line);
          det->symbols = g_list_prepend (det->symbols, key_symbol);
        }
      else if (g_str_has_prefix (line, "FINDP "))
        {
          /* Equivalent to FIND, except that the user is asked to consider
           * only the primary symbols (i.e. Plain and Shift).
           */
          if (step_type == UNKNOWN)
            {
              step_type = KEY_PRESENT_P;
            }
          else
            {
              g_free (line);
              return ERROR;
            }
          char *key_symbol = g_strdup (g_strstrip (line + 6));
          g_free (line);
          det->symbols = g_list_prepend (det->symbols, key_symbol);
        }
      else if (g_str_has_prefix (line, "YES "))
        {
          /* Direct the evaluating code to process step ## next if the
           * user does have this key.
           */
          int next_step = atoi (g_strstrip (line + 4));
          g_free (line);
          if (step_type != KEY_PRESENT_P &&
              step_type != KEY_PRESENT)
            return ERROR;
          det->present = next_step;
        }
      else if (g_str_has_prefix (line, "NO "))
        {
          /* Direct the evaluating code to process step ## next if the
           * user does not have this key.
           */
          int next_step = atoi (g_strstrip (line + 3));
          g_free (line);
          if (step_type != KEY_PRESENT_P &&
              step_type != KEY_PRESENT)
            return ERROR;
          det->not_present = next_step;
        }
      else if (g_str_has_prefix (line, "MAP "))
        {
          /* This step uniquely identifies a keymap. */
          if (step_type == UNKNOWN)
            step_type = RESULT;
          det->result = g_strdup (g_strstrip (line + 4));
          g_free (line);
          /* The Ubuntu file uses colons to separate country codes from layout
           * variants, and GnomeXkb requires plus signs.
           */
          char *colon_pointer = strchr (det->result, ':');
          if (colon_pointer != NULL)
            *colon_pointer = '+';
          return step_type;
        }
      else {
        g_free (line);
        return ERROR;
      }
    }

  /* The requested step was not found. */
  return ERROR;
}
