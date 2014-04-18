/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013 Kalev Lember <kalevlember@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __CC_TIMEZONE_MONITOR_H
#define __CC_TIMEZONE_MONITOR_H

#include <glib-object.h>
#include "tz.h"

G_BEGIN_DECLS

#define CC_TYPE_TIMEZONE_MONITOR                  (cc_timezone_monitor_get_type ())
#define CC_TIMEZONE_MONITOR(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CC_TYPE_TIMEZONE_MONITOR, CcTimezoneMonitor))
#define CC_IS_TIMEZONE_MONITOR(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CC_TYPE_TIMEZONE_MONITOR))
#define CC_TIMEZONE_MONITOR_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), CC_TYPE_TIMEZONE_MONITOR, CcTimezoneMonitorClass))
#define CC_IS_TIMEZONE_MONITOR_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), CC_TYPE_TIMEZONE_MONITOR))
#define CC_TIMEZONE_MONITOR_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), CC_TYPE_TIMEZONE_MONITOR, CcTimezoneMonitorClass))

typedef struct _CcTimezoneMonitor        CcTimezoneMonitor;
typedef struct _CcTimezoneMonitorClass   CcTimezoneMonitorClass;

struct _CcTimezoneMonitor
{
	GObject parent_instance;
};

struct _CcTimezoneMonitorClass
{
	GObjectClass parent_class;

	void (*timezone_changed) (CcTimezoneMonitor *monitor, TzLocation *new_tzlocation);
};

GType cc_timezone_monitor_get_type (void) G_GNUC_CONST;

CcTimezoneMonitor *cc_timezone_monitor_new (void);

G_END_DECLS

#endif /* __CC_TIMEZONE_MONITOR_H */
