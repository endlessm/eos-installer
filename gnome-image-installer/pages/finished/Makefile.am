
noinst_LTLIBRARIES = libgisfinished.la

AM_CPPFLAGS = \
	$(INITIAL_SETUP_CFLAGS) \
	$(IMAGE_INSTALLER_CFLAGS) \
	-DLOCALSTATEDIR="\"$(localstatedir)\"" \
	-DUIDIR="\"$(uidir)\""

BUILT_SOURCES =

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/finished.gresource.xml)
finished-resources.c: finished.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
finished-resources.h: finished.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += finished-resources.c finished-resources.h

libgisfinished_la_SOURCES =			\
	gis-finished-page.c gis-finished-page.h	\
	$(BUILT_SOURCES)

libgisfinished_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "$(srcdir)/../../../gnome-initial-setup" -I "$(srcdir)/../../util" -I "$(srcdir)/../.." $(WARN_CFLAGS)
libgisfinished_la_LIBADD = $(INITIAL_SETUP_LIBS) $(IMAGE_INSTALLER_CFLAGS)
libgisfinished_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined $(WARN_LDFLAGS)

EXTRA_DIST =	\
	finished.gresource.xml	\
	$(resource_files)
