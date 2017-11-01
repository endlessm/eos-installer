
noinst_LTLIBRARIES = libgisinstall.la

AM_CPPFLAGS = \
	$(INITIAL_SETUP_CFLAGS) \
	$(IMAGE_INSTALLER_CFLAGS) \
	-DLOCALSTATEDIR="\"$(localstatedir)\"" \
	-DUIDIR="\"$(uidir)\""

BUILT_SOURCES =

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/install.gresource.xml)
install-resources.c: install.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
install-resources.h: install.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += install-resources.c install-resources.h

libgisinstall_la_SOURCES =			\
	gis-install-page.c gis-install-page.h	\
	gis-scribe.c gis-scribe.h \
	$(BUILT_SOURCES)

libgisinstall_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) $(IMAGE_INSTALLER_CFLAGS) -I "$(srcdir)/../../../gnome-initial-setup" -I "$(srcdir)/../.." -I "$(srcdir)/../../util" -I $(top_srcdir)/ext/libglnx $(WARN_CFLAGS)
libgisinstall_la_LIBADD = $(INITIAL_SETUP_LIBS) $(IMAGE_INSTALLER_CFLAGS) ./../../util/libgiiutil.la $(top_builddir)/ext/libglnx.la
libgisinstall_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined $(WARN_LDFLAGS)

EXTRA_DIST =	\
	install.gresource.xml	\
	$(resource_files)
