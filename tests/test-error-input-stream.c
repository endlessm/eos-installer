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
#include "test-error-input-stream.h"

/* We store the maximum offset as a guint64, due to it being a property and
 * there being no GType for gsize. The GInputStream API is in terms of
 * g(s)size.
 */
G_STATIC_ASSERT (G_MAXSIZE <= G_MAXUINT64);

typedef struct _TestErrorInputStream {
  GInputStream parent;

  GInputStream *child;
  guint64 error_offset;
  GError *error;

  gsize pos;
} TestErrorInputStream;

typedef enum {
  PROP_CHILD = 1,
  PROP_ERROR_OFFSET,
  PROP_ERROR,

  N_PROPERTIES
} TestErrorInputStreamPropertyId;

static GParamSpec *props[N_PROPERTIES] = { 0 };

G_DEFINE_TYPE (TestErrorInputStream, test_error_input_stream, G_TYPE_INPUT_STREAM);

static void
test_error_input_stream_init (TestErrorInputStream *self)
{
}

static void
test_error_input_stream_constructed (GObject *object)
{
  TestErrorInputStream *self = TEST_ERROR_INPUT_STREAM (object);

  G_OBJECT_CLASS (test_error_input_stream_parent_class)->constructed (object);

  g_assert (self->child != NULL);
  g_assert (self->error != NULL);
}

static void
test_error_input_stream_set_property (GObject      *object,
                                      guint         property_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  TestErrorInputStream *self = TEST_ERROR_INPUT_STREAM (object);

  switch ((TestErrorInputStreamPropertyId) property_id)
    {
    case PROP_CHILD:
      g_clear_object (&self->child);
      self->child = G_INPUT_STREAM (g_value_dup_object (value));
      break;

    case PROP_ERROR_OFFSET:
      self->error_offset = g_value_get_uint64 (value);
      break;

    case PROP_ERROR:
      g_clear_error (&self->error);
      self->error = g_value_dup_boxed (value);
      g_assert (self->error != NULL);
      break;

    case N_PROPERTIES:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
test_error_input_stream_dispose (GObject *object)
{
  TestErrorInputStream *self = TEST_ERROR_INPUT_STREAM (object);

  g_clear_object (&self->child);

  G_OBJECT_CLASS (test_error_input_stream_parent_class)->dispose (object);
}

static void
test_error_input_stream_finalize (GObject *object)
{
  TestErrorInputStream *self = TEST_ERROR_INPUT_STREAM (object);

  g_clear_error (&self->error);

  G_OBJECT_CLASS (test_error_input_stream_parent_class)->finalize (object);
}

static gssize
test_error_input_stream_read (GInputStream        *stream,
                              void                *buffer,
                              gsize                count,
                              GCancellable        *cancellable,
                              GError             **error)
{
  TestErrorInputStream *self = TEST_ERROR_INPUT_STREAM (stream);
  gssize bytes_read;

  if (self->pos + count > self->error_offset)
    {
      g_propagate_error (error, g_error_copy (self->error));
      return -1;
    }

  bytes_read = g_input_stream_read (self->child, buffer, count, cancellable,
                                    error);
  if (bytes_read >= 0)
    self->pos += bytes_read;

  return bytes_read;
}

static gboolean
test_error_input_stream_close (GInputStream        *stream,
                               GCancellable        *cancellable,
                               GError             **error)
{
  TestErrorInputStream *self = TEST_ERROR_INPUT_STREAM (stream);

  return g_input_stream_close (self->child, cancellable, error);
}

static void
test_error_input_stream_class_init (TestErrorInputStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GInputStreamClass *istream_class = G_INPUT_STREAM_CLASS (klass);

  object_class->set_property = test_error_input_stream_set_property;
  object_class->constructed = test_error_input_stream_constructed;
  object_class->dispose = test_error_input_stream_dispose;
  object_class->finalize = test_error_input_stream_finalize;

  istream_class->read_fn = test_error_input_stream_read;
  /* Allow parent class to emulate skip. We don't use it anyway. */
  istream_class->close_fn = test_error_input_stream_close;

  props[PROP_CHILD] = g_param_spec_object (
      "child",
      "Child",
      "Child stream to delegate to until :error_offset is reached.",
      G_TYPE_INPUT_STREAM,
      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_ERROR_OFFSET] = g_param_spec_uint64 (
      "error-offset",
      "Error Offset",
      "Offset at which reads will start to fail.",
      0, G_MAXUINT64, 0,
      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_ERROR] = g_param_spec_boxed (
      "error",
      "Error",
      "Error to return once :error-offset is reached.",
      G_TYPE_ERROR,
      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPERTIES, props);
}

GInputStream *
test_error_input_stream_new (GInputStream *child,
                             guint64       error_offset,
                             const GError *error)
{
  return g_object_new (TEST_TYPE_ERROR_INPUT_STREAM,
                       "child", child,
                       "error-offset", error_offset,
                       "error", error,
                       NULL);
}
