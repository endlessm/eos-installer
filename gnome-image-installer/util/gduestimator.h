/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2008-2013 Red Hat, Inc.
 *
 * Licensed under GPL version 2 or later.
 *
 * Author: David Zeuthen <zeuthen@gmail.com>
 */

#ifndef __GDU_ESTIMATOR_H__
#define __GDU_ESTIMATOR_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

struct _GduEstimator;
typedef struct _GduEstimator GduEstimator;

#define GDU_TYPE_ESTIMATOR   gdu_estimator_get_type()
#define GDU_ESTIMATOR(o)     (G_TYPE_CHECK_INSTANCE_CAST ((o), GDU_TYPE_ESTIMATOR, GduEstimator))
#define GDU_IS_ESTIMATOR(o)  (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDU_TYPE_ESTIMATOR))

GType          gdu_estimator_get_type            (void) G_GNUC_CONST;
GduEstimator  *gdu_estimator_new                 (guint64         target_bytes);
void           gdu_estimator_add_sample          (GduEstimator    *estimator,
                                                  guint64          completed_bytes);
guint64        gdu_estimator_get_target_bytes    (GduEstimator    *estimator);
guint64        gdu_estimator_get_completed_bytes (GduEstimator    *estimator);

guint64        gdu_estimator_get_bytes_per_sec   (GduEstimator    *estimator);
guint64        gdu_estimator_get_usec_remaining  (GduEstimator    *estimator);

G_END_DECLS

#endif /* __GDU_ESTIMATOR_H__ */
