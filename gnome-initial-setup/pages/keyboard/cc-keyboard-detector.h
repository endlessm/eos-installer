/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2014 Endless Mobile, Inc
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

#ifndef CC_KEYBOARD_DETECTOR_H
#define CC_KEYBOARD_DETECTOR_H

#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

typedef enum {
  UNKNOWN,
  PRESS_KEY,
  KEY_PRESENT,
  KEY_PRESENT_P,
  RESULT,
  ERROR,
} KeyboardDetectorStepType;

typedef struct {
  GHashTable *keycodes; /* GHashTable<int, int> */
  GList *symbols;       /* GList<char *>, strings owned by KeyboardDetector */
  int present;
  int not_present;
  char *result;

  /* Private */
  int current_step;
  KeyboardDetectorStepType step_type;
  GDataInputStream *fp;
} KeyboardDetector;

KeyboardDetector        *keyboard_detector_new       (void);
void                     keyboard_detector_free      (KeyboardDetector *det);
KeyboardDetectorStepType keyboard_detector_read_step (KeyboardDetector *det,
                                                      int               step);

G_END_DECLS

#endif /* CC_KEYBOARD_DETECTOR_H */
