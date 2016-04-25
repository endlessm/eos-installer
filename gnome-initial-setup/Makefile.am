SUBDIRS = pages

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

libexec_PROGRAMS = gnome-initial-setup gnome-initial-setup-copy-worker

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/gis-assistant.gresource.xml)
gis-assistant-resources.c: gis-assistant.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<
gis-assistant-resources.h: gis-assistant.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-header $<
BUILT_SOURCES += gis-assistant-resources.c gis-assistant-resources.h

fbe-remote-generated.c: fbe-remote-generated.h
fbe-remote-generated.h: $(srcdir)/com.endlessm.Tutorial.FBERemote.xml
	$(AM_V_GEN) gdbus-codegen		\
	--interface-prefix=com.endlessm.Tutorial \
	--generate-c-code=fbe-remote-generated $<
BUILT_SOURCES += fbe-remote-generated.c fbe-remote-generated.h

noinst_LTLIBRARIES = libgisutil.la

libgisutil_la_SOURCES = \
	$(BUILT_SOURCES) \
	gis-assistant.c gis-assistant.h \
	gis-page.c gis-page.h \
	gis-driver.c gis-driver.h \
	gis-keyring.c gis-keyring.h \
	gis-window.c gis-window.h

libgisutil_la_CFLAGS = $(INITIAL_SETUP_CFLAGS)
libgisutil_la_LIBADD = $(INITIAL_SETUP_LIBS)
libgisutil_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

gnome_initial_setup_SOURCES = \
	gnome-initial-setup.c gnome-initial-setup.h

gnome_initial_setup_LDADD =	\
	libgisutil.la \
	pages/language/libgislanguage.la \
	pages/keyboard/libgiskeyboard.la \
	pages/display/libgisdisplay.la \
	pages/endless-eula/libgisendlesseula.la \
	pages/eulas/libgiseulas.la \
	pages/network/libgisnetwork.la \
	pages/location/libgislocation.la \
	pages/account/libgisaccount.la \
	pages/goa/libgisgoa.la \
	pages/summary/libgissummary.la \
	$(INITIAL_SETUP_LIBS) \
	-lm

gnome_initial_setup_copy_worker_SOURCES =		\
	gnome-initial-setup-copy-worker.c

gnome_initial_setup_copy_worker_CFLAGS = \
	$(COPY_WORKER_CFLAGS)

gnome_initial_setup_copy_worker_LDADD = \
	$(COPY_WORKER_LIBS)

EXTRA_DIST = \
	com.endlessm.Tutorial.FBERemote.xml \
	gis-assistant.gresource.xml \
	$(resource_files)
