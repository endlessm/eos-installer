noinst_LTLIBRARIES = libgiiutil.la

libgiiutil_la_SOURCES = \
	gis-store.c gis-store.h \
	gduxzdecompressor.c gduxzdecompressor.h \
	gpt.c gpt.h gpt_errors.h \
	gpt_gz.c gpt_gz.h \
	gpt_lzma.c gpt_lzma.h \
	crc32.c crc32.h \
	eos-reformatter.c eos-reformatter.h \
	gduestimator.c gduestimator.h

libgiiutil_la_CFLAGS = $(INITIAL_SETUP_CFLAGS) -I "$(srcdir)/.." -I "$(srcdir)/../../gnome-initial-setup"
libgiiutil_la_LIBADD = $(INITIAL_SETUP_LIBS) -lz
libgiiutil_la_LDFLAGS = -export_dynamic -avoid-version -module -no-undefined

noinst_PROGRAMS = run-reformatter

run_reformatter_SOURCES = run-reformatter.c
run_reformatter_LDADD = libgiiutil.la -lz -llzma
run_reformatter_CFLAGS = $(INITIAL_SETUP_CFLAGS)
