/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2012 Red Hat
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
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

/* Location page {{{1 */

#define PAGE_ID "location"

#include "config.h"
#include "cc-datetime-resources.h"
#include "date-endian.h"
#include "location-resources.h"
#include "gis-location-page.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <stdlib.h>
#include <string.h>

#define GWEATHER_I_KNOW_THIS_IS_UNSTABLE
#include <libgweather/location-entry.h>

#include "cc-timezone-map.h"
#include "cc-timezone-monitor.h"
#include "timedated.h"

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-wall-clock.h>

#define DEFAULT_TZ "Europe/London"

struct _GisLocationPagePrivate
{
  CcTimezoneMap *map;
  TzLocation *current_location;

  GDateTime *date;
  GnomeWallClock *clock_tracker;

  Timedate1 *dtm;
  GCancellable *cancellable;

  CcTimezoneMonitor *timezone_monitor;
};
typedef struct _GisLocationPagePrivate GisLocationPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisLocationPage, gis_location_page, GIS_TYPE_PAGE);

#define OBJ(type,name) ((type)gtk_builder_get_object(GIS_PAGE (page)->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)

/* Forward declarations to avoid calls before definitions */
static void
day_changed (GtkWidget *widget, GisLocationPage *page);

static void
month_year_changed (GtkWidget *widget, GisLocationPage *page);

static void
set_timezone_cb (GObject      *source,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  GError *error;

  error = NULL;
  if (!timedate1_call_set_timezone_finish (TIMEDATE1 (source),
                                           res,
                                           &error)) {
    /* TODO: display any error in a user friendly way */
    g_warning ("Could not set system timezone: %s", error->message);
    g_error_free (error);
  }
}


static void
queue_set_timezone (GisLocationPage *page)
{
  GisLocationPagePrivate *priv = gis_location_page_get_instance_private (page);

  /* for now just do it */
  if (priv->current_location) {
    timedate1_call_set_timezone (priv->dtm,
                                 priv->current_location->zone,
                                 TRUE,
                                 NULL,
                                 set_timezone_cb,
                                 page);
  }
}

static void
update_timezone (GisLocationPage *page)
{
  GisLocationPagePrivate *priv = gis_location_page_get_instance_private (page);
  GString *str;
  gchar *location;
  gchar *timezone;
  gchar *c;

  str = g_string_new ("");
  for (c = priv->current_location->zone; *c; c++) {
    switch (*c) {
    case '_':
      g_string_append_c (str, ' ');
      break;
    case '/':
      g_string_append (str, " / ");
      break;
    default:
      g_string_append_c (str, *c);
    }
  }

  c = strstr (str->str, " / ");
  location = g_strdup (c + 3);
  timezone = g_strdup (str->str);

  gtk_label_set_label (OBJ(GtkLabel*,"current-location-label"), location);
  gtk_label_set_label (OBJ(GtkLabel*,"current-timezone-label"), timezone);

  g_free (location);
  g_free (timezone);

  g_string_free (str, TRUE);
}

static void
location_changed_cb (CcTimezoneMap   *map,
                     TzLocation      *location,
                     GisLocationPage *page)
{
  GisLocationPagePrivate *priv = gis_location_page_get_instance_private (page);

  g_debug ("location changed to %s/%s", location->country, location->zone);

  priv->current_location = location;

  update_timezone (page);

  queue_set_timezone (page);
}

static void
timezone_changed_cb (CcTimezoneMonitor *timezone_monitor,
                     TzLocation        *location,
                     GisLocationPage   *page)
{
  GisLocationPagePrivate *priv = gis_location_page_get_instance_private (page);

  g_debug ("timezone changed to %s/%s", location->country, location->zone);

  priv->current_location = location;

  cc_timezone_map_set_timezone (priv->map, location->zone);

  update_timezone (page);

  queue_set_timezone (page);
}

static void
set_location_from_gweather_location (GisLocationPage  *page,
                                     GWeatherLocation *gloc)
{
  GisLocationPagePrivate *priv = gis_location_page_get_instance_private (page);
  GWeatherTimezone *zone = gweather_location_get_timezone (gloc);
  gchar *city = gweather_location_get_city_name (gloc);

  if (zone != NULL) {
    const gchar *name;
    const gchar *id;
    GtkLabel *label;

    label = OBJ(GtkLabel*, "current-timezone-label");

    name = gweather_timezone_get_name (zone);
    id = gweather_timezone_get_tzid (zone);
    if (name == NULL) {
      /* Why does this happen ? */
      name = id;
    }
    gtk_label_set_label (label, name);
    cc_timezone_map_set_timezone (priv->map, id);
  }

  if (city != NULL) {
    GtkLabel *label;

    label = OBJ(GtkLabel*, "current-location-label");
    gtk_label_set_label (label, city);
  }

  g_free (city);
}

static void
location_changed (GObject *object, GParamSpec *param, GisLocationPage *page)
{
  GWeatherLocationEntry *entry = GWEATHER_LOCATION_ENTRY (object);
  GWeatherLocation *gloc;

  gloc = gweather_location_entry_get_location (entry);
  if (gloc == NULL)
    return;

  set_location_from_gweather_location (page, gloc);

  gweather_location_unref (gloc);
}

static void
set_using_ntp_cb (GObject *object, GAsyncResult *res, gpointer user_data)
{
  GError *error = NULL;

  if (!timedate1_call_set_ntp_finish (TIMEDATE1 (object), res, &error))
    {
      g_warning ("Could not set system to use NTP: %s", error->message);
      g_error_free (error);
    }
}

static void
queue_set_ntp (GisLocationPage *page)
{
  GisLocationPagePrivate *priv = gis_location_page_get_instance_private (page);
  gboolean using_ntp;

  using_ntp = gtk_switch_get_active (GTK_SWITCH (WID ("network_time_switch")));

  timedate1_call_set_ntp (priv->dtm,
                          using_ntp,
                          TRUE,
                          priv->cancellable,
                          set_using_ntp_cb,
                          page);
}

static void
update_widget_state_for_ntp (GisLocationPage *page, gboolean using_ntp)
{
  gtk_widget_set_sensitive (WID ("time-grid"), !using_ntp);
  gtk_widget_set_sensitive (WID ("date-box"), !using_ntp);
}

static void
change_ntp (GObject *object, GParamSpec *pspec, GisLocationPage *page)
{
  update_widget_state_for_ntp (page, gtk_switch_get_active (GTK_SWITCH (object)));
  queue_set_ntp (page);
}


static void
update_ntp_switch_from_system (GisLocationPage *page)
{
  GisLocationPagePrivate *priv = gis_location_page_get_instance_private (page);
  gboolean using_ntp;
  GtkWidget *switch_widget;

  using_ntp = timedate1_get_ntp (priv->dtm);

  switch_widget = WID ("network_time_switch");
  g_signal_handlers_block_by_func (switch_widget, change_ntp, page);
  gtk_switch_set_active (GTK_SWITCH (switch_widget), using_ntp);
  update_widget_state_for_ntp (page, using_ntp);
  g_signal_handlers_unblock_by_func (switch_widget, change_ntp, page);
}

static void
update_time (GisLocationPage *page)
{
  GisLocationPagePrivate *priv = gis_location_page_get_instance_private (page);
  GisDriver *driver = GIS_PAGE (page)->driver;
  const gchar *time_format;
  char *label;

  time_format = gis_driver_get_default_time_format (driver);

  if (!time_format || time_format[0] == '2') {
    /* Update the hours label in 24h format */
    label = g_date_time_format (priv->date, "%H");
    gtk_label_set_text (GTK_LABEL (WID ("hours_label")), label);
    gtk_widget_set_visible (WID ("ampm_label"), FALSE);
  } else {
    /* Update the hours label in AM/PM format */
    label = g_date_time_format (priv->date, "%I");
    gtk_label_set_text (GTK_LABEL (WID ("hours_label")), label);
    g_free (label);
    label = g_date_time_format (priv->date, "%p");
    gtk_label_set_text (GTK_LABEL (WID ("ampm_label")), label);
    gtk_widget_set_visible (WID ("ampm_label"), TRUE);
  }
  g_free (label);

  /* Update the minutes label */
  label = g_date_time_format (priv->date, "%M");
  gtk_label_set_text (GTK_LABEL (WID ("minutes_label")), label);
  g_free (label);
}

static void
update_date (GisLocationPage *page)
{
  GtkWidget *day_widget = WID ("day-spinbutton");
  GtkWidget *month_widget = WID ("month-combobox");
  GtkWidget *year_widget = WID ("year-spinbutton");

  /* Disable the system time updates while the UI is redrawn */
  g_signal_handlers_block_by_func (day_widget, day_changed, page);
  g_signal_handlers_block_by_func (month_widget, month_year_changed, page);
  g_signal_handlers_block_by_func (year_widget, month_year_changed, page);

  /* Update the UI */
  GisLocationPagePrivate *priv = gis_location_page_get_instance_private (page);

  gint day_of_month = g_date_time_get_day_of_month (priv->date);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (day_widget), day_of_month);

  gint month = g_date_time_get_month (priv->date);
  gtk_combo_box_set_active (GTK_COMBO_BOX (month_widget), month - 1);

  gint year = g_date_time_get_year (priv->date);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (year_widget), year);


  /* Re-enable the system time updates from the UI */
  g_signal_handlers_unblock_by_func (day_widget, day_changed, page);
  g_signal_handlers_unblock_by_func (month_widget, month_year_changed, page);
  g_signal_handlers_unblock_by_func (year_widget, month_year_changed, page);
}

static void
set_time_cb (GObject *source, GAsyncResult *res, gpointer user_data)
{
  GisLocationPage *page = user_data;
  GError *error = NULL;

  if(!timedate1_call_set_time_finish (TIMEDATE1 (source), res, &error))
    {
      g_warning ("Could not set system time: %s", error->message);
      g_error_free (error);
    }
  else
    {
      update_time (page);
      update_date (page);
    }
}

static void
queue_set_datetime (GisLocationPage *page)
{
  GisLocationPagePrivate *priv = gis_location_page_get_instance_private (page);
  gint64 unixtime;

  /* timedated expects number of microseconds since 1 Jan 1970 UTC */
  unixtime = g_date_time_to_unix (priv->date);

  timedate1_call_set_time (priv->dtm,
                           unixtime * G_TIME_SPAN_SECOND,
                           FALSE,
                           TRUE,
                           priv->cancellable,
                           set_time_cb,
                           page);
}

static void
change_time (GtkButton *button, GisLocationPage *page)
{
  GDateTime *old_date;
  GisLocationPagePrivate *priv = gis_location_page_get_instance_private (page);
  const gchar *widget_name;
  gint direction;

  old_date = priv->date;
  widget_name = gtk_buildable_get_name (GTK_BUILDABLE (button));

  if (strstr (widget_name, "up"))
    direction = 1;
  else
    direction = -1;

  if (widget_name[0] == 'h')
    {
      priv->date = g_date_time_add_hours (old_date, direction);
    }
  else if (widget_name[0] == 'm')
    {
      priv->date = g_date_time_add_minutes (old_date, direction);
    }
  else
    {
      int hour;
      hour = g_date_time_get_hour (old_date);
      if (hour >= 12)
        priv->date = g_date_time_add_hours (old_date, -12);
      else
        priv->date = g_date_time_add_hours (old_date, 12);
    }

  g_date_time_unref (old_date);

  update_time (page);
  queue_set_datetime (page);
}

static void
reorder_date_widget (DateEndianess endianess, GisLocationPage *page)
{
  GtkBox *box;
  GtkWidget *month, *day, *year;

  if (endianess == DATE_ENDIANESS_MIDDLE)
    return;

  month = WID ("month-combobox");
  day = WID ("day-spinbutton");
  year = WID ("year-spinbutton");

  box = GTK_BOX (WID ("date-box"));

  switch (endianess)
    {
    case DATE_ENDIANESS_LITTLE:
      gtk_box_reorder_child (box, month, 0);
      gtk_box_reorder_child (box, day, 0);
      gtk_box_reorder_child (box, year, -1);
      break;

    case DATE_ENDIANESS_BIG:
      gtk_box_reorder_child (box, month, 0);
      gtk_box_reorder_child (box, year, 0);
      gtk_box_reorder_child (box, day, -1);
      break;

    case DATE_ENDIANESS_MIDDLE:
      /* We already handle this case, but cover to avoid warnings in the compiler */
      g_assert_not_reached();
      break;
    }
}

static void
change_date (GisLocationPage *page)
{
  GisLocationPagePrivate *priv = gis_location_page_get_instance_private (page);
  GDateTime *old_date = priv->date;
  guint month, year, day;

  month = 1 + gtk_combo_box_get_active (GTK_COMBO_BOX (WID ("month-combobox")));
  year = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (WID ("year-spinbutton")));
  day = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (WID ("day-spinbutton")));

  priv->date = g_date_time_new_local (year, month, day,
                                      g_date_time_get_hour (old_date),
                                      g_date_time_get_minute (old_date),
                                      g_date_time_get_second (old_date));
  g_date_time_unref (old_date);
  queue_set_datetime (page);
}

static void
month_year_changed (GtkWidget *widget, GisLocationPage *page)
{
  GtkAdjustment *adj;
  GtkSpinButton *day_spin;
  guint month;
  guint num_days;
  guint year;

  month = 1 + gtk_combo_box_get_active (GTK_COMBO_BOX (WID ("month-combobox")));
  year = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (WID ("year-spinbutton")));

  num_days = g_date_get_days_in_month (month, year);

  day_spin = GTK_SPIN_BUTTON (WID ("day-spinbutton"));
  adj = GTK_ADJUSTMENT (gtk_spin_button_get_adjustment (day_spin));
  gtk_adjustment_set_upper (adj, num_days + 1);

  if (gtk_spin_button_get_value_as_int (day_spin) > num_days)
    gtk_spin_button_set_value (day_spin, num_days);

  change_date (page);
}

static void
day_changed (GtkWidget *widget, GisLocationPage *page)
{
  change_date (page);
}

static void
clock_changed (GnomeWallClock *clock, GParamSpec *pspec, GisLocationPage *page)
{
  GisLocationPagePrivate *priv = gis_location_page_get_instance_private (page);

  g_date_time_unref (priv->date);
  priv->date = g_date_time_new_now_local ();
  update_time (page);
  update_date (page);
}

#define WANT_GEOCLUE 0

#if WANT_GEOCLUE
static void
position_callback (GeocluePosition      *pos,
		   GeocluePositionFields fields,
		   int                   timestamp,
		   double                latitude,
		   double                longitude,
		   double                altitude,
		   GeoclueAccuracy      *accuracy,
		   GError               *error,
		   GisLocationPage      *page)
{
  if (error) {
    g_printerr ("Error getting position: %s\n", error->message);
    g_error_free (error);
  } else {
    if (fields & GEOCLUE_POSITION_FIELDS_LATITUDE &&
        fields & GEOCLUE_POSITION_FIELDS_LONGITUDE) {
      GWeatherLocation *city = gweather_location_find_nearest_city (latitude, longitude);
      set_location_from_gweather_location (page, city);
    } else {
      g_print ("Position not available.\n");
    }
  }
}

static void
determine_location (GtkWidget       *widget,
                    GisLocationPage *page)
{
  GeoclueMaster *master;
  GeoclueMasterClient *client;
  GeocluePosition *position = NULL;
  GError *error = NULL;

  master = geoclue_master_get_default ();
  client = geoclue_master_create_client (master, NULL, NULL);
  g_object_unref (master);

  if (!geoclue_master_client_set_requirements (client, 
                                               GEOCLUE_ACCURACY_LEVEL_LOCALITY,
                                               0, TRUE,
                                               GEOCLUE_RESOURCE_ALL,
                                               NULL)){
    g_printerr ("Setting requirements failed");
    goto out;
  }

  position = geoclue_master_client_create_position (client, &error);
  if (position == NULL) {
    g_warning ("Creating GeocluePosition failed: %s", error->message);
    goto out;
  }

  geoclue_position_get_position_async (position,
                                       (GeocluePositionCallback) position_callback,
                                       page);

 out:
  g_clear_error (&error);
  g_object_unref (client);
  g_object_unref (position);
}
#endif

static void
gis_location_page_constructed (GObject *object)
{
  GisLocationPage *page = GIS_LOCATION_PAGE (object);
  GisLocationPagePrivate *priv = gis_location_page_get_instance_private (page);
  GtkWidget *frame, *map, *entry;
  GWeatherLocation *world;
  GSettings *clock_settings;
  GError *error;
  const gchar *clock_format;
  const gchar *timezone;
  DateEndianess endianess;
  GtkWidget *widget;
  gint i;
  guint num_days;
  GtkAdjustment *adjustment;
  gchar *time_buttons[] = { "hour_up_button", "hour_down_button",
                            "min_up_button", "min_down_button" };

  G_OBJECT_CLASS (gis_location_page_parent_class)->constructed (object);

  gtk_container_add (GTK_CONTAINER (page), WID ("location-page"));

  frame = WID("location-map-frame");

  priv->cancellable = g_cancellable_new ();
  error = NULL;
  priv->dtm = timedate1_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                G_DBUS_PROXY_FLAGS_NONE,
                                                "org.freedesktop.timedate1",
                                                "/org/freedesktop/timedate1",
                                                NULL,
                                                &error);
  if (priv->dtm == NULL) {
    g_error ("Failed to create proxy for timedated: %s", error->message);
    exit (1);
  }

  priv->map = cc_timezone_map_new ();
  map = GTK_WIDGET (priv->map);
  gtk_widget_set_hexpand (map, TRUE);
  gtk_widget_set_vexpand (map, TRUE);
  gtk_widget_set_halign (map, GTK_ALIGN_FILL);
  gtk_widget_set_valign (map, GTK_ALIGN_FILL);
  gtk_widget_show (map);

  gtk_container_add (GTK_CONTAINER (frame), map);

  world = gweather_location_new_world (TRUE);
  entry = gweather_location_entry_new (world);
  gtk_entry_set_placeholder_text (GTK_ENTRY (entry), _("Search for a location"));
  gtk_widget_set_halign (entry, GTK_ALIGN_FILL);
  gtk_widget_show (entry);

  frame = WID("location-page");
#if WANT_GEOCLUE
  gtk_grid_attach (GTK_GRID (frame), entry, 1, 1, 1, 1);
#else
  gtk_grid_attach (GTK_GRID (frame), entry, 0, 1, 2, 1);
#endif

  timezone = timedate1_get_timezone (priv->dtm);

  if (!cc_timezone_map_set_timezone (priv->map, timezone)) {
    GisDriver *driver = GIS_PAGE (page)->driver;
    const gchar *default_timezone = gis_driver_get_default_timezone (driver);

    if (default_timezone == NULL || default_timezone[0] == '\0') {
      default_timezone = DEFAULT_TZ;
    }

    g_warning ("Timezone '%s' is unhandled, setting %s as default",
               timezone, default_timezone);
    cc_timezone_map_set_timezone (priv->map, default_timezone);

    priv->current_location = cc_timezone_map_get_location (priv->map);
    queue_set_timezone (page);
  }
  else {
    g_debug ("System timezone is '%s'", timezone);
    priv->current_location = cc_timezone_map_get_location (priv->map);
  }

  update_timezone (page);

  g_signal_connect (G_OBJECT (entry), "notify::location",
                    G_CALLBACK (location_changed), page);

  g_signal_connect (map, "location-changed",
                    G_CALLBACK (location_changed_cb), page);

#if WANT_GEOCLUE
  g_signal_connect (WID ("location-auto-button"), "clicked",
                    G_CALLBACK (determine_location), page);
#else
  gtk_widget_hide (WID ("location-auto-button"));
#endif

  /* set up network time button */
  update_ntp_switch_from_system (page);
  g_signal_connect(WID ("network_time_switch"), "notify::active",
                   G_CALLBACK (change_ntp), page);

  /* set up time editing widgets */
  for (i = 0; i < G_N_ELEMENTS (time_buttons); i++)
    {
      g_signal_connect (WID (time_buttons[i]), "clicked",
                        G_CALLBACK (change_time), page);
    }

  /* set up date editing widgets */
  priv->date = g_date_time_new_now_local ();
  endianess = date_endian_get_default (FALSE);
  reorder_date_widget (endianess, page);

  /* Force the direction for the time, so that the time is presented
     correctly for RTL languages */
  gtk_widget_set_direction (WID ("time-grid"), GTK_TEXT_DIR_LTR);

  widget = WID ("month-combobox");
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget),
                            g_date_time_get_month (priv->date) - 1);
  g_signal_connect (widget, "changed",
                    G_CALLBACK (month_year_changed), page);

  num_days = g_date_get_days_in_month (g_date_time_get_month (priv->date),
                                       g_date_time_get_year (priv->date));
  adjustment = (GtkAdjustment *) gtk_adjustment_new (g_date_time_get_day_of_month (priv->date),
                                                     1, num_days, 1, 10, 1);
  widget = WID ("day-spinbutton");
  gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (widget), adjustment);

  g_signal_connect (widget, "value-changed",
                    G_CALLBACK (day_changed), page);

  adjustment = (GtkAdjustment *) gtk_adjustment_new (g_date_time_get_year (priv->date),
                                                     1900, 9999, 1, 10, 1);
  widget = WID ("year-spinbutton");
  gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (widget), adjustment);
  g_signal_connect (widget, "value-changed",
                    G_CALLBACK (month_year_changed), page);

  month_year_changed(widget, page);

  /* set up the time itself */
  priv->clock_tracker = g_object_new (GNOME_TYPE_WALL_CLOCK, NULL);
  g_signal_connect (priv->clock_tracker, "notify::clock",
                    G_CALLBACK (clock_changed), page);

  /* add automatic timezone */
  priv->timezone_monitor = cc_timezone_monitor_new ();
  g_signal_connect (priv->timezone_monitor, "timezone-changed",
                    G_CALLBACK (timezone_changed_cb), page);

  update_time (page);

  clock_settings = g_settings_new ("org.gnome.desktop.interface");
  clock_format = gis_driver_get_default_time_format (GIS_PAGE (page)->driver);
  g_settings_set_string (clock_settings, "clock-format", clock_format? clock_format: "24h");
  g_object_unref (clock_settings);

  gis_page_set_complete (GIS_PAGE (page), TRUE);

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_location_page_dispose (GObject *object)
{
  GisLocationPage *page = GIS_LOCATION_PAGE (object);
  GisLocationPagePrivate *priv = gis_location_page_get_instance_private (page);

  g_clear_object (&priv->dtm);
  g_clear_object (&priv->clock_tracker);
  g_clear_object (&priv->timezone_monitor);
  g_clear_pointer (&priv->date, g_date_time_unref);
  if (priv->cancellable) {
    g_cancellable_cancel (priv->cancellable);
    g_clear_object (&priv->cancellable);
  }

  G_OBJECT_CLASS (gis_location_page_parent_class)->dispose (object);
}

static void
gis_location_page_locale_changed (GisPage *page)
{
  gis_page_set_title (GIS_PAGE (page), _("Location"));
}

static void
gis_location_page_class_init (GisLocationPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_location_page_locale_changed;
  object_class->constructed = gis_location_page_constructed;
  object_class->dispose = gis_location_page_dispose;
}

static void
gis_location_page_init (GisLocationPage *page)
{
  g_resources_register (location_get_resource ());
  g_resources_register (datetime_get_resource ());
}

void
gis_prepare_location_page (GisDriver *driver)
{
  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_LOCATION_PAGE,
                                     "driver", driver,
                                     NULL));
}
