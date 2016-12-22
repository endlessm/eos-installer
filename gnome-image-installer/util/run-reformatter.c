#include <stdio.h>

#include "eos-reformatter.h"

static GMainLoop *loop = NULL;
static guint64 initial_estimate = 0;
static gint64 start_usec = 0;
static int retval = 0;
static GCancellable *cancellable = NULL;

static gboolean _quit (gpointer data)
{
  g_main_loop_quit(loop);

  return FALSE;
}

static gboolean _cancel (gpointer data)
{
  EosReformatter *reformatter = EOS_REFORMATTER(data);

  g_warning ("cancelling");

  g_cancellable_cancel (cancellable);

  return FALSE;
}

static void _reformat_finished (GObject *object, gboolean success, gpointer data)
{
  EosReformatter *reformatter = EOS_REFORMATTER(data);

  if (success)
    {
      printf ("\n\nReformat finished in %.02f seconds, buffers: %d / %d / %d / %d\n",
        (g_get_real_time () - start_usec) / 1000.0 / 1000.0,
        eos_reformatter_free_queue_length(reformatter),
        eos_reformatter_decomp_queue_length(reformatter),
        eos_reformatter_write_queue_length(reformatter),
        eos_reformatter_used_buffers(reformatter));
    }
  else
    {
      GError *error = eos_reformatter_get_error (reformatter);
      printf ("\n\nFinished with error: %s\n", error->message);
      retval = 1;
    }

  g_timeout_add_seconds(1, _quit, NULL);
}

static void _reformat_progress (GObject *object, GParamSpec *pspec, gpointer data)
{
  EosReformatter *reformatter = EOS_REFORMATTER(data);

  if (eos_reformatter_get_usec_remaining (reformatter) == 0)
    return;

  if (initial_estimate == 0) {
    initial_estimate = eos_reformatter_get_usec_remaining (reformatter);
    printf ("\nInitial estimate is %.02f seconds\n\n", initial_estimate / 1000.0 / 1000.0);
  }

  printf ("\r%03.03f%% / %0.02f seconds left (%0.02f Mb/s), buffer balance: %d / %d / %d / %d  ",
    eos_reformatter_get_progress (reformatter) * 100.0,
    eos_reformatter_get_usec_remaining (reformatter) / 1000.0 / 1000.0,
    eos_reformatter_get_bytes_per_sec (reformatter) / 1024.0 / 1024.0,
    eos_reformatter_free_queue_length(reformatter),
    eos_reformatter_decomp_queue_length(reformatter),
    eos_reformatter_write_queue_length(reformatter),
    eos_reformatter_used_buffers(reformatter));
  fflush(stdin);
}

static gboolean _start_reformat (gpointer data)
{
  EosReformatter *reformatter = EOS_REFORMATTER(data);

  start_usec = g_get_real_time ();
  cancellable = g_cancellable_new ();
  if (!eos_reformatter_reformat (reformatter, cancellable))
    {
      g_timeout_add_seconds(1, _quit, NULL);
    }

  return FALSE;
}

int main(int argc, char** argv)
{
  EosReformatter *reformatter;
  guint64 write_size = 0;
  char *signature = NULL;

  if (argc < 3)
    {
      printf ("\nUsage: %s <target> <image> [write size] [signature]\n", argv[0]);
      return 1;
    }

  if (argc >= 4)
    {
      write_size = g_ascii_strtoull (argv[3], NULL, 0);
    }

  if (argc == 5)
    {
      signature = argv[4];
    }

  reformatter = eos_reformatter_new (argv[2], signature, argv[1]);
  g_object_set (reformatter,
    "write-size", write_size,
    NULL);

  g_signal_connect (reformatter, "notify::progress",
                    G_CALLBACK (_reformat_progress),
                    reformatter);

  g_signal_connect (reformatter, "finished",
                    G_CALLBACK (_reformat_finished),
                    reformatter);

  g_idle_add(_start_reformat, reformatter);
//  g_timeout_add_seconds(10, _cancel, reformatter);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_object_unref (reformatter);
  g_main_loop_unref (loop);

  return retval;
}
