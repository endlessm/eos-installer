#ifndef __EOS_REFORMATTER_H__
#define __EOS_REFORMATTER_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

struct _EosReformatter;
typedef struct _EosReformatter EosReformatter;

#define EOS_TYPE_REFORMATTER   eos_reformatter_get_type()
#define EOS_REFORMATTER(o)     (G_TYPE_CHECK_INSTANCE_CAST ((o), EOS_TYPE_REFORMATTER, EosReformatter))
#define EOS_IS_REFORMATTER(o)  (G_TYPE_CHECK_INSTANCE_TYPE ((o), EOS_TYPE_REFORMATTER))
#define EOS_REFORMATTER_ERROR  eos_reformatter_error_quark()

GType            eos_reformatter_get_type (void) G_GNUC_CONST;
EosReformatter  *eos_reformatter_new (const gchar *image, const gchar *signature, const gchar *device);

typedef enum
{
  EOS_REFORMATTER_ERROR_FAILED,
  EOS_REFORMATTER_ERROR_UNKNOWN_SIZE,
  EOS_REFORMATTER_ERROR_READ_FAILED,
  EOS_REFORMATTER_ERROR_WRITE_FAILED,
  EOS_REFORMATTER_ERROR_VERIFICATION_FAILED,
} EosReformatterErrorEnum;

gboolean eos_reformatter_reformat (EosReformatter *reformatter, GCancellable *cancellable);
void eos_reformatter_cancel (EosReformatter *reformatter);

gdouble eos_reformatter_get_progress (EosReformatter *reformatter);
const GError *eos_reformatter_get_error (EosReformatter *reformatter);
guint64 eos_reformatter_get_usec_remaining (EosReformatter *reformatter);
guint64 eos_reformatter_get_bytes_per_sec (EosReformatter *reformatter);

/* DEBUG */

gint eos_reformatter_free_queue_length(EosReformatter *reformatter);
gint eos_reformatter_decomp_queue_length(EosReformatter *reformatter);
gint eos_reformatter_write_queue_length(EosReformatter *reformatter);
gint eos_reformatter_used_buffers(EosReformatter *reformatter);


G_END_DECLS

#endif /* __EOS_REFORMATTER_H__ */

