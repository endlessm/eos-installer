
noinst_LTLIBRARIES = libgislocation.la

BUILT_SOURCES =

# Used for backward file
AM_CPPFLAGS = \
	-DGNOMECC_DATA_DIR="\"$(datadir)/gnome-control-center\""

geoclue.c: geoclue.h
geoclue.h: $(GEOCLUE_DBUS_INTERFACE_XML)
	$(AM_V_GEN) gdbus-codegen \
               --interface-prefix org.freedesktop.GeoClue2 \
               --generate-c-code geoclue \
               --c-namespace Geoclue $<
BUILT_SOURCES += geoclue.c geoclue.h

timedated.c: timedated.h
timedated.h: $(srcdir)/timedated1-interface.xml
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix org.freedesktop. \
		--generate-c-code timedated $<
BUILT_SOURCES += timedated.c timedated.h

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/datetime.gresource.xml)
cc-datetime-resources.c: datetime.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
cc-datetime-resources.h: datetime.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += cc-datetime-resources.c cc-datetime-resources.h

resource_files_location = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/location.gresource.xml)
location-resources.c: location.gresource.xml $(resource_files_location)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
location-resources.h: location.gresource.xml $(resource_files_location)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += location-resources.c location-resources.h

libgislocation_la_SOURCES =	\
	tz.c tz.h \
	weather-tz.c weather-tz.h \
	cc-timezone-map.c cc-timezone-map.h \
	cc-timezone-monitor.c cc-timezone-monitor.h \
	gis-location-page.c gis-location-page.h \
	date-endian.c date-endian.h \
	$(BUILT_SOURCES)

libgislocation_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "$(srcdir)/../.."
libgislocation_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgislocation_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

EXTRA_DIST =				\
	timedated1-interface.xml	\
	$(resource_files)		\
	$(resource_files_location)	\
	datetime.gresource.xml		\
	location.gresource.xml
