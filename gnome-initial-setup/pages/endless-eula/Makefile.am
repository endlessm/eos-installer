
noinst_LTLIBRARIES = libgisendlesseula.la

AM_CPPFLAGS = \
	$(INITIAL_SETUP_CFLAGS) \
	-DLOCALSTATEDIR="\"$(localstatedir)\"" \
	-DUIDIR="\"$(uidir)\""

BUILT_SOURCES =

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/endless-eula.gresource.xml)
endless-eula-resources.c: endless-eula.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
endless-eula-resources.h: endless-eula.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += endless-eula-resources.c endless-eula-resources.h

libgisendlesseula_la_SOURCES =			\
	gis-endless-eula-page.c gis-endless-eula-page.h	\
	$(BUILT_SOURCES)

libgisendlesseula_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "$(srcdir)/../.."
libgisendlesseula_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgisendlesseula_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

EXTRA_DIST = endless-eula.gresource.xml $(resource_files)
