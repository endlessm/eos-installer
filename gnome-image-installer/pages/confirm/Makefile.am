noinst_LTLIBRARIES = libgisconfirm.la

AM_CPPFLAGS = \
	$(INITIAL_SETUP_CFLAGS) $(IMAGE_INSTALLER_CFLAGS) \
	-DLOCALSTATEDIR="\"$(localstatedir)\"" \
	-DUIDIR="\"$(uidir)\""

BUILT_SOURCES =

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/confirm.gresource.xml)
confirm-resources.c: confirm.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
confirm-resources.h: confirm.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += confirm-resources.c confirm-resources.h

libgisconfirm_la_SOURCES = \
	gis-confirm-page.c \
	gis-confirm-page.h \
	$(NULL)

nodist_libgisconfirm_la_SOURCES = $(BUILT_SOURCES)

libgisconfirm_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) $(IMAGE_INSTALLER_CFLAGS) $(WARN_CFLAGS)
libgisconfirm_la_CPPFLAGS = -I "$(srcdir)/../../../gnome-initial-setup" -I "$(srcdir)/../.." -I "$(srcdir)/../../util"
libgisconfirm_la_LIBADD = $(INITIAL_SETUP_LIBS) $(IMAGE_INSTALLER_LIBS)
libgisconfirm_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined $(WARN_LDFLAGS)

EXTRA_DIST =	\
	confirm.gresource.xml	\
	$(resource_files)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)
