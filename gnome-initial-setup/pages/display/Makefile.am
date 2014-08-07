
noinst_LTLIBRARIES = libgisdisplay.la

AM_CPPFLAGS = \
	$(INITIAL_SETUP_CFLAGS) \
	-DGNOME_DESKTOP_USE_UNSTABLE_API=1 \
	-DLOCALSTATEDIR="\"$(localstatedir)\"" \
	-DUIDIR="\"$(uidir)\""

BUILT_SOURCES =

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/display.gresource.xml)
display-resources.c: display.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
display-resources.h: display.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += display-resources.c display-resources.h

libgisdisplay_la_SOURCES =			\
	gis-display-page.c gis-display-page.h	\
	$(BUILT_SOURCES)

libgisdisplay_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "$(srcdir)/../.."
libgisdisplay_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgisdisplay_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

EXTRA_DIST = display.gresource.xml $(resource_files)
