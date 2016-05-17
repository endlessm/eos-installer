
noinst_LTLIBRARIES = libgisdisktarget.la

AM_CPPFLAGS = \
	$(INITIAL_SETUP_CFLAGS) $(IMAGE_INSTALLER_CFLAGS) \
	-DLOCALSTATEDIR="\"$(localstatedir)\"" \
	-DUIDIR="\"$(uidir)\""

BUILT_SOURCES =

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/disktarget.gresource.xml)
disktarget-resources.c: disktarget.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
disktarget-resources.h: disktarget.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += disktarget-resources.c disktarget-resources.h

libgisdisktarget_la_SOURCES =			\
	gis-disktarget-page.c gis-disktarget-page.h	\
	$(BUILT_SOURCES)

libgisdisktarget_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) $(IMAGE_INSTALLER_CFLAGS) -I "$(srcdir)/../../../gnome-initial-setup" -I "$(srcdir)/../.." -I "$(srcdir)/../../util"
libgisdisktarget_la_LIBADD = $(INITIAL_SETUP_LIBS) $(IMAGE_INSTALLER_LIBS)
libgisdisktarget_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

EXTRA_DIST =	\
	disktarget.gresource.xml	\
	$(resource_files)
