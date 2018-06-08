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
 */
#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define TEST_TYPE_ERROR_INPUT_STREAM (test_error_input_stream_get_type ())
G_DECLARE_FINAL_TYPE (TestErrorInputStream, test_error_input_stream,
                      TEST, ERROR_INPUT_STREAM, GInputStream);

GInputStream *test_error_input_stream_new (GInputStream *child,
                                           guint64       error_offset,
                                           const GError *error);
