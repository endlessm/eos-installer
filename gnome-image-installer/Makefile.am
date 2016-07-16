SUBDIRS = util pages

BUILT_SOURCES =

# TODO: ugly, but let's see if splitting makes sense
uidir = $(datadir)/gnome-initial-setup
gisdir = ./../gnome-initial-setup
gissrcdir = $(srcdir)/../gnome-initial-setup

$(info ${uidir})

AM_CPPFLAGS = \
	$(INITIAL_SETUP_CFLAGS) \
	$(IMAGE_INSTALLER_CFLAGS) \
	-DSECRET_API_SUBJECT_TO_CHANGE \
	-DGCR_API_SUBJECT_TO_CHANGE \
	-DUIDIR="\"$(uidir)\"" \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DLIBLOCALEDIR=\""$(prefix)/lib/locale"\" \
	-DDATADIR=\""$(datadir)"\" \
	-I$(gissrcdir)

libexec_PROGRAMS = gnome-image-installer liveinstaller-partition-helper

resource_files = $(shell glib-compile-resources --sourcedir=$(gissrcdir) --generate-dependencies $(gissrcdir)/gis-assistant.gresource.xml)
gis-assistant-resources.c: $(gissrcdir)/gis-assistant.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(gissrcdir) --generate-source $<
gis-assistant-resources.h: $(gissrcdir)/gis-assistant.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(gissrcdir) --generate-header $<
BUILT_SOURCES += gis-assistant-resources.c gis-assistant-resources.h

gnome_image_installer_SOURCES = \
	$(BUILT_SOURCES) \
	gnome-image-installer.c gnome-image-installer.h

gnome_image_installer_LDADD =	\
	util/libgiiutil.la \
	$(gisdir)/libgisutil.la \
	$(gisdir)/pages/language/libgislanguage.la \
	$(gisdir)/pages/keyboard/libgiskeyboard.la \
	$(gisdir)/pages/display/libgisdisplay.la \
	$(gisdir)/pages/endless-eula/libgisendlesseula.la \
	$(gisdir)/pages/eulas/libgiseulas.la \
	pages/diskimage/libgisdiskimage.la \
	pages/disktarget/libgisdisktarget.la \
	pages/install/libgisinstall.la \
	pages/finished/libgisfinished.la \
	$(INITIAL_SETUP_LIBS) \
	$(IMAGE_INSTALLER_LIBS) \
	-lm -lz

liveinstaller_partition_helper_SOURCES = \
	liveinstaller-partition-helper.c

liveinstaller_partition_helper_LDADD = \
	$(IMAGE_INSTALLER_LIBS)
