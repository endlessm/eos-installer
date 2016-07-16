/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2016 Endless Mobile, Inc.
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
 */

#include "config.h"

#include <glib.h>
#include <string.h>

int main (int argc, char *argv[])
{
  GPid sfpid = 0;
  gint infd = -1;
  gchar *args[] = { "sfdisk", "--force", "--label", "gpt", "/dev/null", NULL};
  gchar *layout = g_strdup_printf(
    /* Space for GRUB and whatnot MBR stuff */
    "start=2048, "
#ifdef __arm__
    /* RootFS (32bit ARM) */
    "type=69DAD710-2CE4-4E3C-B16C-21A1D49ABED3, attrs=GUID:55\n"
#else
    /* EFI */
    "size=62MiB, type=C12A7328-F81F-11D2-BA4B-00A0C93EC93B\n"
    /* BIOS Boot */
    "size=1MiB, type=21686148-6449-6E6F-744E-656564454649\n"
    /* RootFS (x86-64) */
    "type=4F68BCE3-E8CD-4DB1-96E7-FBCAF984B709, attrs=GUID:55\n"
#endif
    );

  if (argc != 2)
    {
      g_warning("Exactly one argument is required");
      return -1;
    }

  args[4] = argv[1];

  if (!g_spawn_async_with_pipes (NULL, args, NULL,
                                 G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                                 NULL, NULL, &sfpid,
                                 &infd, NULL, NULL, NULL))
    return -1;

  if (write (infd, layout, strlen(layout)) <= 0)
    return -1;

  close (infd);

  g_spawn_command_line_sync ("partprobe", NULL, NULL, NULL, NULL);

  return 0;
}
