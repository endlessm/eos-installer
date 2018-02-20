BUILT_SOURCES =

uidir = $(datadir)/gnome-initial-setup

AM_CPPFLAGS = \
	$(INITIAL_SETUP_CFLAGS) \
	-DSECRET_API_SUBJECT_TO_CHANGE \
	-DGCR_API_SUBJECT_TO_CHANGE \
	-DUIDIR="\"$(uidir)\"" \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DLIBLOCALEDIR=\""$(prefix)/lib/locale"\" \
	-DDATADIR=\""$(datadir)"\"

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/gis-assistant.gresource.xml)
gis-assistant-resources.c: gis-assistant.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
gis-assistant-resources.h: gis-assistant.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += gis-assistant-resources.c gis-assistant-resources.h

noinst_LTLIBRARIES = libgisutil.la

libgisutil_la_SOURCES = \
	$(BUILT_SOURCES) \
	gis-assistant.c gis-assistant.h \
	gnome-initial-setup.h \
	gis-page.c gis-page.h \
	gis-driver.c gis-driver.h \
	gis-driver-mode.c gis-driver-mode.h \
	gis-window.c gis-window.h

libgisutil_la_CFLAGS = $(INITIAL_SETUP_CFLAGS)
libgisutil_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgisutil_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

EXTRA_DIST = \
	gis-assistant.gresource.xml \
	$(resource_files)
