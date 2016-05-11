noinst_LTLIBRARIES = libgiiutil.la

libgiiutil_la_SOURCES = \
	gis-store.c gis-store.h \
	gduxzdecompressor.c gduxzdecompressor.h

libgiiutil_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "$(srcdir)/.." -I "$(srcdir)/../../gnome-initial-setup"
libgiiutil_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgiiutil_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined
