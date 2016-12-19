#include "config.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <glib-object.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

#include "gduxzdecompressor.h"
#include "gduestimator.h"
#include "eos-reformatter.h"

struct _EosReformatter
{
  GObject parent;

  char *image;
  char *signature;
  char *device;

  int read_fd;
  int write_fd;
  int gpg_in;
  GPid gpg;

  gdouble progress;
  gboolean finished;

  guchar *pool;
  int total_buffers;
  int used_buffers;
  int buffer_size;
  long page_size;

  GAsyncQueue *free_queue;
  GAsyncQueue *decomp_queue;
  GAsyncQueue *write_queue;

  guint64 read_offset;
  guint64 write_completed_bytes;
  guint64 write_total_bytes;

  GThread *read_thread;
  GThread *decomp_thread;
  GThread *write_thread;

  GduEstimator *estimator;
  GConverter *decompressor;

  GError *error;
};

typedef struct _EosReformatterClass EosReformatterClass;
struct _EosReformatterClass
{
  GObjectClass parent_class;
};

typedef struct _EosAlignedBuffer EosAlignedBuffer;
struct _EosAlignedBuffer
{
  guchar *ptr;
  gsize len;
};

enum
{
  PROP_0,
  PROP_IMAGE,
  PROP_WRITE_SIZE,
  PROP_SIGNATURE,
  PROP_DEVICE,
  PROP_PROGRESS,
  N_PROPERTIES
};

static GParamSpec *_props[N_PROPERTIES] = { NULL, };

enum
{
  SIG_0,
  SIG_FINISHED,
  N_SIGNALS
};

static guint _signals[N_SIGNALS] = { 0, };

G_DEFINE_TYPE (EosReformatter, eos_reformatter, G_TYPE_OBJECT)

G_DEFINE_QUARK(install-error, eos_reformatter_error);
#define EOS_REFORMATTER_ERROR eos_reformatter_error_quark()

#define EOS_THREADS 2
#define EOS_BUFFERS 16
#define EOS_BUFFER_SIZE (1 * 1024 * 1024)

static void
eos_reformatter_init (EosReformatter *reformatter)
{
  reformatter->page_size = sysconf (_SC_PAGESIZE);
  reformatter->total_buffers = EOS_BUFFERS;
  reformatter->buffer_size = EOS_BUFFER_SIZE;
  reformatter->free_queue = g_async_queue_new ();
  reformatter->decomp_queue = g_async_queue_new ();
  reformatter->write_queue = g_async_queue_new ();
  reformatter->read_offset = 0;
}

static void
eos_reformatter_dispose (GObject *object)
{
  EosReformatter *reformatter = EOS_REFORMATTER (object);
  EosAlignedBuffer *buf = NULL;

  if (reformatter->gpg > 0)
    {
      close (reformatter->gpg_in);
      g_spawn_close_pid (reformatter->gpg);
    }

  if (reformatter->read_thread != NULL)
    {
      g_thread_join (reformatter->read_thread);
      reformatter->read_thread = NULL;
    }
  if (reformatter->decomp_thread != NULL)
    {
      g_thread_join (reformatter->decomp_thread);
      reformatter->decomp_thread = NULL;
    }
  if (reformatter->write_thread != NULL)
    {
      g_thread_join (reformatter->write_thread);
      reformatter->write_thread = NULL;
    }

  close (reformatter->read_fd);
  close (reformatter->write_fd);

  do
    {
      buf = g_async_queue_try_pop (reformatter->free_queue);
      if (buf != NULL)
        g_free(buf);
    }
  while (buf != NULL);
  g_async_queue_unref (reformatter->free_queue);

  do
    {
      buf = g_async_queue_try_pop (reformatter->decomp_queue);
      if (buf != NULL)
        g_free(buf);
    }
  while (buf != NULL);
  g_async_queue_unref (reformatter->decomp_queue);

  do
    {
      buf = g_async_queue_try_pop (reformatter->write_queue);
      if (buf != NULL)
        g_free(buf);
    }
  while (buf != NULL);
  g_async_queue_unref (reformatter->write_queue);

  if (reformatter->pool != NULL)
    g_free (reformatter->pool);

  if (reformatter->image != NULL)
    g_free (reformatter->image);
  if (reformatter->signature != NULL)
    g_free (reformatter->signature);
  if (reformatter->device != NULL)
    g_free (reformatter->device);

  if (reformatter->decompressor != NULL)
    g_object_unref (reformatter->decompressor);

  if (reformatter->error != NULL)
      g_error_free (reformatter->error);

  G_OBJECT_CLASS (eos_reformatter_parent_class)->dispose (object);
}

static void
eos_reformatter_finalize (GObject *object)
{
  G_OBJECT_CLASS (eos_reformatter_parent_class)->finalize (object);
}

static void
eos_reformatter_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  EosReformatter *reformatter = EOS_REFORMATTER (object);

  switch (property_id)
    {
    case PROP_IMAGE:
      g_value_set_string (value, reformatter->image);
      break;
    case PROP_WRITE_SIZE:
      g_value_set_uint64 (value, reformatter->write_total_bytes);
      break;
    case PROP_SIGNATURE:
      g_value_set_string (value, reformatter->signature);
      break;
    case PROP_DEVICE:
      g_value_set_string (value, reformatter->device);
      break;
    case PROP_PROGRESS:
      g_value_set_double (value, reformatter->progress);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
eos_reformatter_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  EosReformatter *reformatter = EOS_REFORMATTER (object);

  switch (property_id)
    {
    case PROP_IMAGE:
      g_free (reformatter->image);
      reformatter->image = g_value_dup_string (value);
      break;
    case PROP_WRITE_SIZE:
      reformatter->write_total_bytes = g_value_get_uint64 (value);
      break;
    case PROP_SIGNATURE:
      g_free (reformatter->signature);
      reformatter->signature = g_value_dup_string (value);
      break;
    case PROP_DEVICE:
      g_free (reformatter->device);
      reformatter->device = g_value_dup_string (value);
      break;
    case PROP_PROGRESS:
      reformatter->progress = g_value_get_double (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
eos_reformatter_class_init (EosReformatterClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->get_property = eos_reformatter_get_property;
  gobject_class->set_property = eos_reformatter_set_property;
  gobject_class->dispose     = eos_reformatter_dispose;
  gobject_class->finalize     = eos_reformatter_finalize;

  _props[PROP_IMAGE] =
    g_param_spec_string ("image",
                         "Image",
                         "Disk image to verify and write",
                         NULL  /* default value */,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  _props[PROP_WRITE_SIZE] =
    g_param_spec_uint64 ("write-size",
                         "Write size in bytes",
                         "The size of the write operation in bytes for "
                         "progress estimation (defaults to image file size)",
                         0  /* minimum value */,
                         G_MAXUINT64  /* maximum value */,
                         0  /* default value */,
                         G_PARAM_READWRITE);

  _props[PROP_SIGNATURE] =
    g_param_spec_string ("signature",
                         "Signature",
                         "Signature to verify the disk image with",
                         NULL  /* default value */,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  _props[PROP_DEVICE] =
    g_param_spec_string ("device",
                         "Device",
                         "Disk device to write the image to",
                         NULL  /* default value */,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  _props[PROP_PROGRESS] =
    g_param_spec_double ("progress",
                         "Progress",
                         "Progress of the reformatting",
                         0.0  /* minimum value */,
                         1.0  /* maximum value */,
                         0.0  /* default value */,
                         G_PARAM_READWRITE);

  _signals[SIG_FINISHED] =
    g_signal_newv ("finished",
                   G_TYPE_FROM_CLASS (gobject_class),
                   G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                   NULL /* closure */,
                   NULL /* accumulator */,
                   NULL /* accumulator data */,
                   NULL /* C marshaller */,
                   G_TYPE_NONE /* return_type */,
                   0     /* n_params */,
                   NULL  /* param_types */);

  g_object_class_install_properties (gobject_class,
                                     N_PROPERTIES,
                                     _props);
}

EosReformatter *
eos_reformatter_new (const gchar *image, const gchar *signature, const gchar *device)
{
  return EOS_REFORMATTER (g_object_new (EOS_TYPE_REFORMATTER,
                                      "image", image,
                                      "device", device,
                                      "signature", signature,
                                      NULL));
}

/* Private API */

static void
eos_reformatter_maybe_finish (EosReformatter *reformatter)
{
  if (reformatter->finished)
    return;

  if (reformatter->error == NULL && reformatter->gpg > 0)
    return;

  if (reformatter->error == NULL && reformatter->progress < 1.0)
    return;

  reformatter->finished = TRUE;

  g_signal_emit (reformatter, _signals[SIG_FINISHED], 0);
}

static EosAlignedBuffer *
eos_reformatter_get_free_buffer (EosReformatter *reformatter, gboolean exhaust)
{
  /* If the exhaust flag is not set, we only allow half of the buffers to be
   * reserved. This is mainly to avoid reading queue to hog free buffers.
   */
  int half_buffers = reformatter->total_buffers / 2;
  EosAlignedBuffer *buf = NULL;

  if (exhaust || g_async_queue_length (reformatter->free_queue) < half_buffers)
    {
      buf = g_async_queue_try_pop (reformatter->free_queue);
    }

  /* A free buffer was available, return it */
  if (buf != NULL)
    {
      buf->len = 0;
      return buf;
    }

  /* Initialize our pool, if it's not created yet */
  if (reformatter->pool == NULL)
    {
      reformatter->pool = g_new0(guchar, reformatter->total_buffers * (reformatter->buffer_size + reformatter->page_size));
    }

  /* A bit awkwardly, exhaust doesn't apply when we create new buffers */
  if (reformatter->used_buffers < reformatter->total_buffers)
    {
      /* No free buffers, but we can create one */
      gsize offset = reformatter->used_buffers * (reformatter->buffer_size + reformatter->page_size);
      guchar *unaligned = reformatter->pool + offset;
      gsize page_size = reformatter->page_size;

      buf = g_new0(EosAlignedBuffer, 1);
      buf->ptr = (guchar*) (((gintptr) (unaligned + page_size)) & (~(page_size - 1)));
      buf->len = 0;
      reformatter->used_buffers++;
    }
  else
    {
      /* If we can't create a new buffer, we need to wait for one */
      EosAlignedBuffer *buf;

      while (!exhaust && g_async_queue_length (reformatter->free_queue) < half_buffers);
        {
          g_thread_yield();
        }

      do
        {
          g_thread_yield();
          buf = g_async_queue_try_pop (reformatter->free_queue);
        }
      while (buf == NULL && reformatter->error == NULL);
      buf->len = 0;
      return buf;
    }

  return buf;
}

static gint
eos_reformatter_prepare_read (EosReformatter *reformatter)
{
  GFileInfo *info;
  GFile *file = g_file_new_for_path (reformatter->image);
  GError *error = NULL;

  info = g_file_query_info (file,
                            G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
                            G_FILE_ATTRIBUTE_STANDARD_SIZE,
                            G_FILE_QUERY_INFO_NONE,
                            NULL,
                            &error);

  if (info == NULL)
    {
      reformatter->error = g_error_new (EOS_REFORMATTER_ERROR, 0, _("Internal error"));
      g_error_free (error);
    }
  else
    {
      const gchar *type = NULL;

      /* If we weren't seeded with the target size, use the file size.
       * This is of course wrong for compressed files */
      if (reformatter->write_total_bytes == 0)
        {
          reformatter->write_total_bytes = g_file_info_get_size (info);
        }

      type = g_file_info_get_content_type (info);
      if (g_str_has_suffix(type, "-xz-compressed"))
        {
          reformatter->decompressor = G_CONVERTER (gdu_xz_decompressor_new ());
        }
      else if (g_str_equal(type, "application/gzip"))
        {
          reformatter->decompressor = G_CONVERTER (g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP));
        }
  }

  g_object_unref (file);

  if (info != NULL)
    g_object_unref (info);

  return open(reformatter->image, O_RDONLY);
}

static gboolean
eos_reformatter_read (EosReformatter *reformatter, GAsyncQueue *outq)
{
  gboolean ret = TRUE;
  guint64 len = 0;
  EosAlignedBuffer *buf = eos_reformatter_get_free_buffer (reformatter, FALSE);

  if (reformatter->error != NULL)
    {
      /* Error is set so send EOS and abort */
      if (buf != NULL)
        {
          g_async_queue_push (outq, buf);
        }
      else
        {
          buf = g_new0(EosAlignedBuffer, 1);
          g_async_queue_push (outq, buf);
        }
      return FALSE;
    }

  if (reformatter->gpg_in > 0)
    {
#ifdef USE_SPLICE
      /* Splice to GPG stdin pipe, offering the offset so the read_fd position isn't changed */
      len = splice (reformatter->read_fd, &reformatter->read_offset, reformatter->gpg_in, NULL, reformatter->buffer_size, 0);

      if (len > 0)
        {
          /* Read to a buffer the same amount as splice */
          buf->len = read (reformatter->read_fd, buf->ptr, len);
          /* Update the current file position for next splice */
          reformatter->read_offset = lseek (reformatter->read_fd, 0, SEEK_CUR);
        }
#else
      buf->len = read (reformatter->read_fd, buf->ptr, reformatter->buffer_size);
      if (buf->len > 0)
        {
          len = write (reformatter->gpg_in, buf->ptr, buf->len);
        }
#endif
    }
  else
    {
      /* Plain read, no GPG verification */
      buf->len = read (reformatter->read_fd, buf->ptr, reformatter->buffer_size);
    }

  if (len < 0 || buf->len < 0)
    {
      reformatter->error = g_error_new (EOS_REFORMATTER_ERROR, 0, _("Internal error"));
      ret = FALSE;
      buf->len = 0;
      g_async_queue_push (outq, buf);
    }
  else
    {
      /* Empty buffer indicates EOS */
      if (buf->len == 0)
        {
          ret = FALSE;
          if (reformatter->gpg_in > 0)
            {
              /* Close up GPG pipe to let it finish up */
              close (reformatter->gpg_in);
            }
        }

      g_async_queue_push (outq, buf);
    }

  return ret;
}

static gboolean
eos_reformatter_decompress (EosReformatter *reformatter, GAsyncQueue *inq, GAsyncQueue *outq)
{
  GConverterResult res;
  GConverterFlags flags = G_CONVERTER_NO_FLAGS;
  GError *error = NULL;
  gsize input_read = 0;
  EosAlignedBuffer *buf;
  EosAlignedBuffer *outbuf;

  if (reformatter->error != NULL)
    {
      /* Error is set so send EOS and abort */
      buf = g_new0(EosAlignedBuffer, 1);
      g_async_queue_push (outq, buf);
      return FALSE;
    }

  buf = g_async_queue_pop (inq);
  outbuf = eos_reformatter_get_free_buffer (reformatter, TRUE);

  if (buf->len == 0)
    flags |= G_CONVERTER_INPUT_AT_END;

  outbuf->len = 0;

  /* Loop over input until consumed, sending possible output immediately */
  do
    {
      gsize in = 0;
      res = g_converter_convert (reformatter->decompressor,
                                 buf->ptr + input_read, buf->len - input_read,
                                 outbuf->ptr, reformatter->buffer_size,
                                 flags, &in, &outbuf->len,
                                 &error);
      input_read += in;

      if (outbuf->len > 0)
        {
          g_async_queue_push (outq, outbuf);
          /* Note that this competes with read buffers if in different threads */
          outbuf = eos_reformatter_get_free_buffer (reformatter, TRUE);
          if (reformatter->error != NULL)
            {
              res = G_CONVERTER_ERROR;
            }
        }
    }
  while (res == G_CONVERTER_CONVERTED && input_read < buf->len);

  /* Return the outbuf to free queue if we didn't use it */
  if (outbuf->len == 0)
    {
      g_async_queue_push (reformatter->free_queue, outbuf);
    }

  /* This is fine, we just need to cycle to get more input buffers */
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_PARTIAL_INPUT))
    {
      res = G_CONVERTER_CONVERTED;
      g_error_free (error);
      error = NULL;
    }

  if (error != NULL)
    {
      reformatter->error = g_error_new (EOS_REFORMATTER_ERROR, 0, _("Internal error"));
      g_error_free (error);
    }

  if (res == G_CONVERTER_FINISHED)
    {
      /* If conversion is finished, we send an empty buffer to indicate EOS */
      buf->len = 0;
      g_async_queue_push (outq, buf);
    }
  else
    {
      g_async_queue_push (reformatter->free_queue, buf);
    }

  return (res == G_CONVERTER_CONVERTED);
}

static gint
eos_reformatter_prepare_write (EosReformatter *reformatter)
{
  return open(reformatter->device, O_WRONLY | O_SYNC | O_CREAT);
}

static gboolean
eos_reformatter_add_sample (gpointer data)
{
  EosReformatter *reformatter = EOS_REFORMATTER(data);

  gdu_estimator_add_sample (reformatter->estimator, reformatter->write_completed_bytes);

  return FALSE;
}

static gboolean
eos_reformatter_write (EosReformatter *reformatter, GAsyncQueue *inq)
{
  gboolean ret = TRUE;
  EosAlignedBuffer *buf;
  gsize wb = 0;

  if (reformatter->error != NULL)
    {
      /* Error is set so abort */
      return FALSE;
    }

  buf = g_async_queue_pop (inq);

  /* EOS */
  if (buf->len == 0)
    {
      syncfs (reformatter->write_fd);
      g_async_queue_push (reformatter->free_queue, buf);

      /* Always add sample in the end so no risk of missing an update */
      g_idle_add (eos_reformatter_add_sample, reformatter);

      return FALSE;
    }

  wb = write (reformatter->write_fd, buf->ptr, buf->len);

  if (wb < 0)
    {
      reformatter->error = g_error_new (EOS_REFORMATTER_ERROR, 0, _("Internal error"));
      ret = FALSE;
    }
  else
    {
      /* We don't lock here to not create a dependency between writing and
       * main thread. This is the only place we change write_completed_bytes
       * and since it's not critical if we miss updates, it should be fine
       * to avoid the lock.
       */
      reformatter->write_completed_bytes += wb;

      /* Overwriting the estimate is an error */
      if (reformatter->write_completed_bytes > reformatter->write_total_bytes)
        {
          reformatter->error = g_error_new (EOS_REFORMATTER_ERROR, 0, _("Internal error"));
          ret = FALSE;
          reformatter->write_completed_bytes = reformatter->write_total_bytes;
        }

      g_idle_add (eos_reformatter_add_sample, reformatter);
    }

  g_async_queue_push (reformatter->free_queue, buf);
  return ret;
}

static void
eos_reformatter_update_progress (GObject *object, GParamSpec *pspec, gpointer data)
{
  EosReformatter *reformatter = EOS_REFORMATTER(data);
  guint64 completed = gdu_estimator_get_completed_bytes (reformatter->estimator);
  guint64 target = gdu_estimator_get_target_bytes (reformatter->estimator);
  gdouble progress = (gdouble)completed / (gdouble)target;

  if (progress > 1.0)
    progress = 1.0;

  g_object_set (reformatter, "progress", progress, NULL);

  if (completed == target)
    {
      eos_reformatter_maybe_finish (reformatter);
    }
}

static void
eos_reformatter_gpg_watch (GPid pid, gint status, gpointer data)
{
  EosReformatter *reformatter = EOS_REFORMATTER (data);

  if (!g_spawn_check_exit_status (status, NULL))
    {
      reformatter->error = g_error_new (EOS_REFORMATTER_ERROR, 0, _("Image verification error."));
      g_warning ("Verification failed!");
    }
  else
    {
      g_debug ("Verification ok");
    }

  g_spawn_close_pid (reformatter->gpg);
  reformatter->gpg = 0;

  eos_reformatter_maybe_finish (reformatter);
}

#define IMAGE_KEYRING "/usr/share/keyrings/eos-image-keyring.gpg"
static gboolean
eos_reformatter_prepare_gpg_verify (EosReformatter *reformatter)
{
  GError *error = NULL;
  gchar *args[] = { "gpg",
                    /* Trust the one key in this keyring, and no others */
                    "--keyring", IMAGE_KEYRING,
                    "--no-default-keyring",
                    "--trust-model", "always",
                    "--verify", (gchar *) reformatter->signature, "-", NULL };

  if (!g_spawn_async_with_pipes (NULL, args, NULL,
                                 G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                                 NULL, NULL, &reformatter->gpg,
                                 &reformatter->gpg_in, NULL, NULL, &error))
    {
      reformatter->error = g_error_new (EOS_REFORMATTER_ERROR, 0, _("Image verification error."));
      return FALSE;
    }
  else
    {
      g_child_watch_add (reformatter->gpg, (GChildWatchFunc)eos_reformatter_gpg_watch, reformatter);
    }

  return TRUE;
}

/* Thread runners */

static gpointer do_reformat (gpointer data)
{
  EosReformatter *reformatter = EOS_REFORMATTER (data);
  gboolean read_ok = TRUE;
  gboolean decomp_ok = TRUE;
  gboolean write_ok = TRUE;

  do
    {
      if (read_ok)
        read_ok = eos_reformatter_read (reformatter, reformatter->write_queue);
      if (reformatter->decompressor != NULL && decomp_ok)
        decomp_ok = eos_reformatter_decompress (reformatter, reformatter->decomp_queue, reformatter->write_queue);
      write_ok = eos_reformatter_write (reformatter, reformatter->write_queue);
    }
  while (read_ok || write_ok || decomp_ok);

  return NULL;
}

static gpointer do_read (gpointer data)
{
  EosReformatter *reformatter = EOS_REFORMATTER (data);
  gboolean ok = TRUE;

  do
    {
      if (reformatter->decomp_thread == NULL)
        {
          ok = eos_reformatter_read (reformatter, reformatter->write_queue);
        }
      else
        {
          ok = eos_reformatter_read (reformatter, reformatter->decomp_queue);
        }
    }
  while (ok);

  return NULL;
}

static gpointer do_decompress (gpointer data)
{
  EosReformatter *reformatter = EOS_REFORMATTER (data);
  gboolean ok = TRUE;

  do
    {
      ok = eos_reformatter_decompress (reformatter, reformatter->decomp_queue, reformatter->write_queue);
    }
  while (ok);

  return NULL;
}

static gpointer do_read_and_decompress (gpointer data)
{
  EosReformatter *reformatter = EOS_REFORMATTER (data);
  gboolean read_ok = TRUE;
  gboolean decomp_ok = TRUE;

  do
    {
      if (read_ok)
        read_ok = eos_reformatter_read (reformatter, reformatter->decomp_queue);

      decomp_ok = eos_reformatter_decompress (reformatter, reformatter->decomp_queue, reformatter->write_queue);
    } while (read_ok || decomp_ok);

  return NULL;
}

static gpointer do_write (gpointer data)
{
  EosReformatter *reformatter = EOS_REFORMATTER (data);
  gboolean ok = TRUE;

  do
    {
      ok = eos_reformatter_write (reformatter, reformatter->write_queue);
    } while (ok);

  return NULL;
}


/* Public API */

gdouble
eos_reformatter_get_progress (EosReformatter *reformatter)
{
  gdouble progress = 0.0;
  g_object_get (reformatter, "progress", &progress, NULL);
  return progress;
}

GError *
eos_reformatter_get_error (EosReformatter *reformatter)
{
  return reformatter->error;
}

guint64
eos_reformatter_get_usec_remaining (EosReformatter *reformatter)
{
  if (reformatter->estimator == NULL)
    return 0;

  return gdu_estimator_get_usec_remaining (reformatter->estimator);
}

guint64
eos_reformatter_get_bytes_per_sec (EosReformatter *reformatter)
{
  if (reformatter->estimator == NULL)
    return 0;

  return gdu_estimator_get_bytes_per_sec (reformatter->estimator);
}

gboolean
eos_reformatter_reformat (EosReformatter *reformatter)
{
  g_return_val_if_fail (reformatter != NULL, FALSE);
  g_return_val_if_fail (reformatter->image != NULL, FALSE);
  g_return_val_if_fail (reformatter->device != NULL, FALSE);

  g_object_set (reformatter, "progress", 0.0, NULL);

  reformatter->read_fd = eos_reformatter_prepare_read (reformatter);
  if (reformatter->read_fd <= 0)
    {
      reformatter->error = g_error_new (EOS_REFORMATTER_ERROR, 0, _("Internal error"));
      return FALSE;
    }

  if (reformatter->signature != NULL && !eos_reformatter_prepare_gpg_verify (reformatter))
    {
      return FALSE;
    }

  reformatter->write_fd = eos_reformatter_prepare_write (reformatter);
  if (reformatter->write_fd <= 0)
    {
      reformatter->error = g_error_new (EOS_REFORMATTER_ERROR, 0, _("Internal error"));
      return FALSE;
    }

  reformatter->estimator = gdu_estimator_new (reformatter->write_total_bytes);
  g_signal_connect (reformatter->estimator, "notify::usec-remaining",
                    G_CALLBACK (eos_reformatter_update_progress),
                    reformatter);

  g_debug ("%s: using %d threads", __FUNCTION__, EOS_THREADS);

#if EOS_THREADS == 1
  reformatter->write_thread = g_thread_new ("read-write", do_reformat, reformatter);
#elif EOS_THREADS == 2
  reformatter->write_thread = g_thread_new ("write", do_write, reformatter);
  if (reformatter->decompressor == NULL)
    {
      reformatter->read_thread = g_thread_new ("read", do_read, reformatter);
    }
  else
    {
      reformatter->read_thread = g_thread_new ("read-compress", do_read_and_decompress, reformatter);
    }
#elif EOS_THREADS == 3
  reformatter->write_thread = g_thread_new ("write", do_write, reformatter);
  reformatter->decomp_thread = g_thread_new ("decompress", do_decompress, reformatter);
  reformatter->read_thread = g_thread_new ("read", do_read, reformatter);
#else
#error "Don't be silly about threads."
#endif

  return TRUE;
}

void
eos_reformatter_cancel (EosReformatter *reformatter)
{
  reformatter->error = g_error_new (EOS_REFORMATTER_ERROR, 0, _("Cancel"));
  eos_reformatter_maybe_finish (reformatter);
}

/* DEBUG */

gint eos_reformatter_free_queue_length(EosReformatter *reformatter)
{
  return g_async_queue_length(reformatter->free_queue);
}

gint eos_reformatter_decomp_queue_length(EosReformatter *reformatter)
{
  return g_async_queue_length(reformatter->decomp_queue);
}

gint eos_reformatter_write_queue_length(EosReformatter *reformatter)
{
  return g_async_queue_length(reformatter->write_queue);
}

gint eos_reformatter_used_buffers(EosReformatter *reformatter)
{
  return reformatter->used_buffers;
}
