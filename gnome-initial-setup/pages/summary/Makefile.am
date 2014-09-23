
noinst_LTLIBRARIES = libgissummary.la

AM_CPPFLAGS = \
	$(INITIAL_SETUP_CFLAGS) \
	-DLOCALSTATEDIR="\"$(localstatedir)\"" \
	-DUIDIR="\"$(uidir)\""

BUILT_SOURCES =

fbe-generated.c: fbe-generated.h
fbe-generated.h: $(srcdir)/com.endlessm.FBE.xml
	$(AM_V_GEN) gdbus-codegen		\
	--interface-prefix=com.endlessm		\
	--generate-c-code=fbe-generated $<
BUILT_SOURCES += fbe-generated.c fbe-generated.h

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/summary.gresource.xml)
summary-resources.c: summary.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
summary-resources.h: summary.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += summary-resources.c summary-resources.h

libgissummary_la_SOURCES =			\
	gis-summary-page.c gis-summary-page.h	\
	$(BUILT_SOURCES)

libgissummary_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "$(srcdir)/../.."
libgissummary_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgissummary_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

EXTRA_DIST =	\
	com.endlessm.FBE.xml	\
	summary.gresource.xml	\
	$(resource_files)
