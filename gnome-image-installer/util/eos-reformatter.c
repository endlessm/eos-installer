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

typedef struct _EosReformatterClass EosReformatterClass;
typedef struct _EosAlignedBuffer EosAlignedBuffer;

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
  gboolean write_fd_is_external;
  guint sample_update;

  guchar *pool;
  int total_buffers;
  int used_buffers;
  int buffer_size;
  long page_size;
  EosAlignedBuffer *boot;

  GAsyncQueue *free_queue;
  GAsyncQueue *decomp_queue;
  GAsyncQueue *write_queue;

  guint64 read_offset;
  guint64 write_completed_bytes;
  guint64 write_total_bytes;
  guint64 write_unsynced_bytes;

  GThread *read_thread;
  GThread *decomp_thread;
  GThread *write_thread;

  GduEstimator *estimator;
  GConverter *decompressor;

  GMutex thread_mutex;
  GError *error;
  GCancellable *cancellable;
};

struct _EosReformatterClass
{
  GObjectClass parent_class;
};

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
  PROP_DEVICE_FD,
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
G_DEFINE_QUARK(reformatter-error, eos_reformatter_error);

#define EOS_BUFFERS 16
#define EOS_BUFFER_SIZE (1 * 1024 * 1024)
#define EOS_SYNC_THRESHOLD (50 * EOS_BUFFER_SIZE)

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

  /* Note: never treat cancelling as an error in the threads,
   * just as an trigger to exit
   */
  g_cancellable_cancel (reformatter->cancellable);

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

  if (g_async_queue_length (reformatter->decomp_queue) < 0)
    {
      buf = g_async_queue_try_pop (reformatter->free_queue);
      if (buf != NULL)
        {
          g_async_queue_push (reformatter->decomp_queue, buf);
        }
    }

  if (reformatter->decomp_thread != NULL)
    {
      g_thread_join (reformatter->decomp_thread);
      reformatter->decomp_thread = NULL;
    }

  if (g_async_queue_length (reformatter->write_queue) < 0)
    {
      buf = g_async_queue_try_pop (reformatter->free_queue);
      if (buf != NULL)
        {
          g_async_queue_push (reformatter->write_queue, buf);
        }
    }

  if (reformatter->write_thread != NULL)
    {
      g_thread_join (reformatter->write_thread);
      reformatter->write_thread = NULL;
    }

  close (reformatter->read_fd);
  if (!reformatter->write_fd_is_external)
    close (reformatter->write_fd);

  if (reformatter->sample_update != 0)
    {
      g_source_remove (reformatter->sample_update);
      reformatter->sample_update = 0;
    }

  do
    {
      buf = g_async_queue_try_pop (reformatter->free_queue);
      g_free(buf);
    }
  while (buf != NULL);
  g_async_queue_unref (reformatter->free_queue);

  do
    {
      buf = g_async_queue_try_pop (reformatter->decomp_queue);
      g_free(buf);
    }
  while (buf != NULL);
  g_async_queue_unref (reformatter->decomp_queue);

  do
    {
      buf = g_async_queue_try_pop (reformatter->write_queue);
      g_free(buf);
    }
  while (buf != NULL);
  g_async_queue_unref (reformatter->write_queue);

  if (reformatter->decompressor != NULL)
    g_object_unref (reformatter->decompressor);

  if (reformatter->error != NULL)
      g_error_free (reformatter->error);

  if (reformatter->cancellable != NULL)
      g_object_unref (reformatter->cancellable);

  G_OBJECT_CLASS (eos_reformatter_parent_class)->dispose (object);
}

static void
eos_reformatter_finalize (GObject *object)
{
  EosReformatter *reformatter = EOS_REFORMATTER (object);

  g_free (reformatter->pool);

  g_free (reformatter->image);
  g_free (reformatter->signature);
  g_free (reformatter->device);

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
    case PROP_DEVICE_FD:
      reformatter->write_fd_is_external = TRUE;
      g_value_set_int (value, reformatter->write_fd);
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
    case PROP_DEVICE_FD:
      reformatter->write_fd = g_value_get_int (value);
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

  _props[PROP_DEVICE_FD] =
    g_param_spec_int ("device-fd",
                      "Device file descriptor",
                      "The file descriptor to write the image to, can be set "
                      "before starting reformat to use open file descriptor",
                      G_MININT  /* minimum value */,
                      G_MAXINT  /* maximum value */,
                      0  /* default value */,
                      G_PARAM_READWRITE);

  _props[PROP_PROGRESS] =
    g_param_spec_double ("progress",
                         "Progress",
                         "Progress of the reformatting",
                         0.0  /* minimum value */,
                         1.0  /* maximum value */,
                         0.0  /* default value */,
                         G_PARAM_READWRITE);

  _signals[SIG_FINISHED] =
    g_signal_new ("finished",
                   G_TYPE_FROM_CLASS (gobject_class),
                   G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                   0 /* closure */,
                   NULL /* accumulator */,
                   NULL /* accumulator data */,
                   g_cclosure_marshal_VOID__BOOLEAN /* C marshaller */,
                   G_TYPE_NONE /* return_type */,
                   1     /* n_params */,
                   G_TYPE_BOOLEAN  /* param_types */,
                   NULL);

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

  /* If we are not cancelled and GPG or write is still unfinished, don't quit yet */
  if (!g_cancellable_is_cancelled (reformatter->cancellable)
   && (reformatter->gpg > 0 || reformatter->progress < 1.0))
    return;

  reformatter->finished = TRUE;

  if (reformatter->sample_update != 0)
    {
      g_source_remove (reformatter->sample_update);
      reformatter->sample_update = 0;
    }

  g_signal_emit (reformatter, _signals[SIG_FINISHED], 0, (reformatter->error == NULL));
}

static EosAlignedBuffer *
eos_reformatter_get_free_buffer (EosReformatter *reformatter, GAsyncQueue *freeq)
{
  EosAlignedBuffer *buf = g_async_queue_try_pop (freeq);

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

      do
        {
          g_thread_yield();
          buf = g_async_queue_try_pop (reformatter->free_queue);
        }
      while (buf == NULL && !g_cancellable_is_cancelled (reformatter->cancellable));

      buf->len = 0;
      return buf;
    }

  return buf;
}

static gint
eos_reformatter_prepare_read (EosReformatter *reformatter)
{
  g_autoptr(GFileInfo) info = NULL;
  g_autoptr(GFile) file = NULL;
  GError *error = NULL;

  file = g_file_new_for_path (reformatter->image);
  info = g_file_query_info (file,
                            G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
                            G_FILE_ATTRIBUTE_STANDARD_SIZE,
                            G_FILE_QUERY_INFO_NONE,
                            NULL,
                            &error);

  if (info == NULL)
    {
      g_propagate_error(&reformatter->error, error);
      g_error_free (error);
    }
  else
    {
      const gchar *type = NULL;

      type = g_file_info_get_content_type (info);
      if (g_str_has_suffix(type, "-xz-compressed"))
        {
          reformatter->decompressor = G_CONVERTER (gdu_xz_decompressor_new ());
        }
      else if (g_str_equal(type, "application/gzip"))
        {
          reformatter->decompressor = G_CONVERTER (g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP));
        }
      else if (reformatter->write_total_bytes == 0)
        {
          reformatter->write_total_bytes = g_file_info_get_size (info);
        }
      if (reformatter->write_total_bytes == 0)
        {
          reformatter->error = g_error_new (EOS_REFORMATTER_ERROR,
                                            EOS_REFORMATTER_ERROR_UNKNOWN_SIZE,
                                            _("Target write size is unknown or unobtainable"));
          return -1;
        }
  }

  return open(reformatter->image, O_RDONLY);
}

static gboolean
eos_reformatter_read (EosReformatter *reformatter, GAsyncQueue *freeq, GAsyncQueue *outq)
{
  gboolean ret = TRUE;
  guint64 len = 0;
  EosAlignedBuffer *buf = NULL;

  if (g_cancellable_is_cancelled (reformatter->cancellable))
    {
      return FALSE;
    }

  buf = eos_reformatter_get_free_buffer (reformatter, freeq);

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
      g_mutex_lock (&reformatter->thread_mutex);
      reformatter->error = g_error_new (EOS_REFORMATTER_ERROR,
                                        EOS_REFORMATTER_ERROR_READ_FAILED,
                                        _("Reading the image failed: %s"),
                                        g_strerror (errno));
      g_mutex_unlock (&reformatter->thread_mutex);
      g_cancellable_cancel (reformatter->cancellable);
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
eos_reformatter_decompress (EosReformatter *reformatter, GAsyncQueue *freeq, GAsyncQueue *inq, GAsyncQueue *outq)
{
  GConverterResult res;
  GConverterFlags flags = G_CONVERTER_NO_FLAGS;
  GError *error = NULL;
  gsize input_read = 0;
  EosAlignedBuffer *buf;
  EosAlignedBuffer *outbuf;

  if (g_cancellable_is_cancelled (reformatter->cancellable))
    {
      return FALSE;
    }

  buf = g_async_queue_pop (inq);
  outbuf = eos_reformatter_get_free_buffer (reformatter, freeq);

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
          outbuf = eos_reformatter_get_free_buffer (reformatter, freeq);
          if (g_cancellable_is_cancelled (reformatter->cancellable))
            {
              res = G_CONVERTER_ERROR;
            }
        }
    }
  while (res == G_CONVERTER_CONVERTED && input_read < buf->len);

  /* Return the outbuf to free queue if we didn't use it */
  if (outbuf->len == 0)
    {
      g_async_queue_push (freeq, outbuf);
    }

  /* This is fine, we just need to cycle to get more input buffers */
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_PARTIAL_INPUT))
    {
      res = G_CONVERTER_CONVERTED;
      g_error_free (error);
      error = NULL;
    }
  else if (error != NULL)
    {
      g_mutex_lock (&reformatter->thread_mutex);
      reformatter->error = error;
      g_mutex_unlock (&reformatter->thread_mutex);
      g_cancellable_cancel (reformatter->cancellable);
    }

  if (res == G_CONVERTER_FINISHED)
    {
      /* If conversion is finished, we send an empty buffer to indicate EOS */
      buf->len = 0;
      g_async_queue_push (outq, buf);
    }
  else
    {
      g_async_queue_push (freeq, buf);
    }

  return (res == G_CONVERTER_CONVERTED);
}

static gint
eos_reformatter_prepare_write (EosReformatter *reformatter)
{
  if (reformatter->write_fd > 0)
    return reformatter->write_fd;

  return open(reformatter->device, O_WRONLY | O_SYNC | O_CREAT);
}

static gboolean
eos_reformatter_add_sample (gpointer data)
{
  EosReformatter *reformatter = EOS_REFORMATTER(data);

  gdu_estimator_add_sample (reformatter->estimator, reformatter->write_completed_bytes);
  g_mutex_lock (&reformatter->thread_mutex);
  reformatter->sample_update = 0;
  g_mutex_unlock (&reformatter->thread_mutex);

  return FALSE;
}

static gboolean
eos_reformatter_write (EosReformatter *reformatter, GAsyncQueue *freeq, GAsyncQueue *inq)
{
  gboolean ret = TRUE;
  EosAlignedBuffer *buf;
  gsize wb = 0;

  if (g_cancellable_is_cancelled (reformatter->cancellable))
    {
      return FALSE;
    }

  buf = g_async_queue_pop (inq);

  /* EOS */
  if (buf->len == 0)
    {

      /* Write the first buffer last */
      if (reformatter->boot != NULL)
        {
          wb = lseek (reformatter->write_fd, 0, SEEK_SET);

          if (wb == 0)
            wb = write (reformatter->write_fd, reformatter->boot->ptr, reformatter->boot->len);

          if (wb <= 0)
            {
              g_mutex_lock (&reformatter->thread_mutex);
              reformatter->error = g_error_new (EOS_REFORMATTER_ERROR,
                                                EOS_REFORMATTER_ERROR_WRITE_FAILED,
                                                _("Writing the image failed: %s"),
                                                g_strerror (errno));
              g_mutex_unlock (&reformatter->thread_mutex);
            }

          g_async_queue_push (freeq, reformatter->boot);
          reformatter->boot = NULL;
        }

      syncfs (reformatter->write_fd);
      g_async_queue_push (freeq, buf);

      /* Always add sample in the end so no risk of missing the update */
      g_mutex_lock (&reformatter->thread_mutex);
      if (reformatter->sample_update == 0)
        reformatter->sample_update = g_idle_add (eos_reformatter_add_sample, reformatter);
      g_mutex_unlock (&reformatter->thread_mutex);

      return FALSE;
    }

  /* Grab the first buffer and write zeroes instead so the result does not
   * look like a bootable system if we abort in the middle
   */
  if (reformatter->boot == NULL)
    {
      reformatter->boot = buf;
      buf = eos_reformatter_get_free_buffer (reformatter, freeq);
      memset (buf->ptr, 0, reformatter->boot->len);
      buf->len = reformatter->boot->len;
    }

  wb = write (reformatter->write_fd, buf->ptr, buf->len);

  if (wb < 0)
    {
      g_mutex_lock (&reformatter->thread_mutex);
      reformatter->error = g_error_new (EOS_REFORMATTER_ERROR,
                                        EOS_REFORMATTER_ERROR_WRITE_FAILED,
                                        _("Writing the image failed: %s"),
                                        g_strerror (errno));
      g_mutex_unlock (&reformatter->thread_mutex);
      ret = FALSE;
    }
  else
    {
      g_mutex_lock (&reformatter->thread_mutex);
      reformatter->write_completed_bytes += wb;
      reformatter->write_unsynced_bytes += wb;

      /* Overwriting the estimate is an error */
      if (reformatter->write_completed_bytes > reformatter->write_total_bytes)
        {
          reformatter->error = g_error_new (EOS_REFORMATTER_ERROR,
                                            EOS_REFORMATTER_ERROR_WRITE_FAILED,
                                            _("Tried to write more than the target size "
                                              "(%lu bytes > %lu bytes)"),
                                            reformatter->write_completed_bytes,
                                            reformatter->write_total_bytes);
          reformatter->write_completed_bytes = reformatter->write_total_bytes;
          g_cancellable_cancel (reformatter->cancellable);
          ret = FALSE;
        }

      if (reformatter->sample_update == 0 && reformatter->write_unsynced_bytes > EOS_SYNC_THRESHOLD)
          reformatter->sample_update = g_idle_add (eos_reformatter_add_sample, reformatter);

      g_mutex_unlock (&reformatter->thread_mutex);

      if (reformatter->write_unsynced_bytes > EOS_SYNC_THRESHOLD)
        {
          syncfs (reformatter->write_fd);
          reformatter->write_unsynced_bytes = 0;
        }

    }

  g_async_queue_push (freeq, buf);
  return ret;
}

static void
eos_reformatter_update_progress (GObject *object, GParamSpec *pspec, gpointer data)
{
  EosReformatter *reformatter = EOS_REFORMATTER(data);
  guint64 completed = gdu_estimator_get_completed_bytes (reformatter->estimator);
  guint64 target = gdu_estimator_get_target_bytes (reformatter->estimator);
  gdouble progress = 0.0;

  if (target > 0.0)
    progress = (gdouble)completed / (gdouble)target;

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
      g_mutex_lock (&reformatter->thread_mutex);
      reformatter->error = g_error_new (EOS_REFORMATTER_ERROR,
                                        EOS_REFORMATTER_ERROR_VERIFICATION_FAILED,
                                        _("Image verification error."));
      g_mutex_unlock (&reformatter->thread_mutex);
      g_cancellable_cancel (reformatter->cancellable);
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
      g_mutex_lock (&reformatter->thread_mutex);
      reformatter->error = g_error_new (EOS_REFORMATTER_ERROR,
                                        EOS_REFORMATTER_ERROR_VERIFICATION_FAILED,
                                        _("Image verification error."));
      g_mutex_unlock (&reformatter->thread_mutex);
      g_cancellable_cancel (reformatter->cancellable);
      return FALSE;
    }
  else
    {
      g_child_watch_add (reformatter->gpg, (GChildWatchFunc)eos_reformatter_gpg_watch, reformatter);
    }

  return TRUE;
}

void
eos_reformatter_cancelled (GCancellable *cancellable, gpointer data)
{
  EosReformatter *reformatter = EOS_REFORMATTER (data);

  eos_reformatter_maybe_finish (reformatter);
}

/* Thread runners */

static gpointer do_read (gpointer data)
{
  EosReformatter *reformatter = EOS_REFORMATTER (data);
  gboolean ok = TRUE;

  do
    {
      if (reformatter->decomp_thread == NULL)
        {
          ok = eos_reformatter_read (reformatter, reformatter->free_queue, reformatter->write_queue);
        }
      else
        {
          ok = eos_reformatter_read (reformatter, reformatter->free_queue, reformatter->decomp_queue);
        }
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
      read_ok = eos_reformatter_read (reformatter, reformatter->free_queue, reformatter->decomp_queue);
      decomp_ok = eos_reformatter_decompress (reformatter, reformatter->free_queue, reformatter->decomp_queue, reformatter->write_queue);
    } while (read_ok || decomp_ok);

  return NULL;
}

static gpointer do_write (gpointer data)
{
  EosReformatter *reformatter = EOS_REFORMATTER (data);
  gboolean ok = TRUE;

  do
    {
      ok = eos_reformatter_write (reformatter, reformatter->free_queue, reformatter->write_queue);
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

const GError *
eos_reformatter_get_error (EosReformatter *reformatter)
{
  return (const GError *)reformatter->error;
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
eos_reformatter_reformat (EosReformatter *reformatter, GCancellable *cancellable)
{
  g_return_val_if_fail (reformatter != NULL, FALSE);
  g_return_val_if_fail (reformatter->image != NULL, FALSE);
  g_return_val_if_fail (reformatter->device != NULL, FALSE);

  g_object_set (reformatter, "progress", 0.0, NULL);

  reformatter->read_fd = eos_reformatter_prepare_read (reformatter);
  if (reformatter->read_fd <= 0)
    {
      if (errno == 0 && reformatter->error == NULL)
        {
          reformatter->error = g_error_new (EOS_REFORMATTER_ERROR,
                                            EOS_REFORMATTER_ERROR_READ_FAILED,
                                            _("Reading the image failed"));
        }
      else if (reformatter->error == NULL)
        {
          reformatter->error = g_error_new (EOS_REFORMATTER_ERROR,
                                            EOS_REFORMATTER_ERROR_READ_FAILED,
                                            _("Reading the image failed: %s"),
                                            g_strerror (errno));
        }
      g_cancellable_cancel(cancellable);
      return FALSE;
    }

  if (reformatter->signature != NULL && !eos_reformatter_prepare_gpg_verify (reformatter))
    {
      g_cancellable_cancel(cancellable);
      return FALSE;
    }

  reformatter->write_fd = eos_reformatter_prepare_write (reformatter);
  if (reformatter->write_fd <= 0)
    {
      reformatter->error = g_error_new (EOS_REFORMATTER_ERROR,
                                        EOS_REFORMATTER_ERROR_WRITE_FAILED,
                                        _("Writing the image failed: %s"),
                                        g_strerror (errno));
      g_cancellable_cancel(cancellable);
      return FALSE;
    }

  reformatter->estimator = gdu_estimator_new (reformatter->write_total_bytes);
  g_signal_connect (reformatter->estimator, "notify::usec-remaining",
                    G_CALLBACK (eos_reformatter_update_progress),
                    reformatter);

  /* If the user provides a GCancellable, use that with a ref. Otherwise our own */
  if (cancellable == NULL)
    {
      reformatter->cancellable = g_cancellable_new ();
    }
  else
    {
      reformatter->cancellable = cancellable;
      g_object_ref (reformatter->cancellable);
    }

    g_cancellable_connect (reformatter->cancellable,
                           G_CALLBACK (eos_reformatter_cancelled),
                           reformatter, NULL);

  reformatter->write_thread = g_thread_new ("write", do_write, reformatter);

  if (reformatter->decompressor == NULL)
    {
      reformatter->read_thread = g_thread_new ("read", do_read, reformatter);
    }
  else
    {
      reformatter->read_thread = g_thread_new ("read-compress", do_read_and_decompress, reformatter);
    }

  return TRUE;
}

void
eos_reformatter_cancel (EosReformatter *reformatter)
{
  g_return_if_fail (reformatter != NULL);

  if (reformatter->cancellable == NULL)
    return;

  g_cancellable_cancel (reformatter->cancellable);
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
