include $(top_srcdir)/buildutil/glib-tap.mk

libglnx_srcpath := $(srcdir)/libglnx
libglnx_cflags := $(LIBGLNX_CFLAGS) "-I$(libglnx_srcpath)"
libglnx_libs := $(LIBGLNX_LIBS)
# See also autogen.sh and https://github.com/ostreedev/ostree/pull/1274/
libglnx/Makefile-libglnx.am.inc: libglnx/Makefile-libglnx.am
	sed -e 's,$$(libglnx_srcpath),libglnx,g' < $< > $@
include libglnx/Makefile-libglnx.am.inc
EXTRA_DIST += libglnx/Makefile-libglnx.am
noinst_LTLIBRARIES += libglnx.la
