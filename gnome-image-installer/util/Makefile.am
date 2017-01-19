noinst_LTLIBRARIES = libgiiutil.la

libgiiutil_la_SOURCES = \
	gis-store.c gis-store.h \
	gduxzdecompressor.c gduxzdecompressor.h \
	gpt.c gpt.h gpt_errors.h \
	gpt_gz.c gpt_gz.h \
	gpt_lzma.c gpt_lzma.h \
	crc32.c crc32.h

libgiiutil_la_CFLAGS = \
	$(INITIAL_SETUP_CFLAGS) \
	-I "$(srcdir)/.." \
	-I "$(srcdir)/../../gnome-initial-setup" \
	$(WARN_CFLAGS)
libgiiutil_la_LIBADD = $(INITIAL_SETUP_LIBS) -lz
libgiiutil_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined $(WARN_LDFLAGS)
