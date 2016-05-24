
noinst_LTLIBRARIES = libgisdiskimage.la

AM_CPPFLAGS = \
	$(INITIAL_SETUP_CFLAGS) \
	$(IMAGE_INSTALLER_CFLAGS) \
	-DLOCALSTATEDIR="\"$(localstatedir)\"" \
	-DUIDIR="\"$(uidir)\""

BUILT_SOURCES =

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/diskimage.gresource.xml)
diskimage-resources.c: diskimage.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
diskimage-resources.h: diskimage.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += diskimage-resources.c diskimage-resources.h

libgisdiskimage_la_SOURCES =			\
	gis-diskimage-page.c gis-diskimage-page.h	\
	$(BUILT_SOURCES)

libgisdiskimage_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) $(IMAGE_INSTALLER_CFLAGS) -I "$(srcdir)/../../../gnome-initial-setup" -I "$(srcdir)/../.." -I "$(srcdir)/../../util"
libgisdiskimage_la_LIBADD = $(INITIAL_SETUP_LIBS) $(IMAGE_INSTALLER_LIBS) ./../../util/libgiiutil.la
libgisdiskimage_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

EXTRA_DIST =	\
	diskimage.gresource.xml	\
	$(resource_files)
