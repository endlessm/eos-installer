include $(top_srcdir)/buildutil/glib-tap.mk

# Using GFile causes GIOModules to be probed, which (if gvfs is installed)
# ultimately causes g_socket_class_init() to be called, which (as documented)
# causes SIGPIPE to be ignored for the remainder of the program. This masked a
# test failure on development systems (where gvfs is installed) which was found
# on the build server (gvfs is not listed as a dependency, so SIGPIPE was not
# implicitly ignored).
#
# Setting the search path to the empty string causes no modules to be loaded.
AM_TESTS_ENVIRONMENT += GIO_MODULE_DIR=

test_programs = \
	test-dmi \
	test-scribe \
	test-unattended-config \
	test-write-diagnostics \
	$(NULL)

dist_test_data = \
	public.asc \
	secret.asc \
	sign-file \
	unattended/blank-block-device.ini \
	unattended/empty.ini \
	unattended/full-block-device-path.ini \
	unattended/full.ini \
	unattended/missing-block-device.ini \
	unattended/missing-filename.ini \
	unattended/missing-product.ini \
	unattended/missing-vendor.ini \
	unattended/non-utf8-locale.ini \
	unattended/two-images.ini \
	wjt.asc \
	bad.sha256 \
	invalid-1.sha256 \
	invalid-2.sha256 \
	$(NULL)

test_data = \
	w.img \
	w.img.asc \
	w.img.sha256 \
	w.img.gz \
	w.img.gz.asc \
	w.img.gz.sha256 \
	w.img.xz \
	w.img.xz.asc \
	w.img.xz.sha256 \
	w.truncated.gz \
	w.truncated.gz.asc \
	w.truncated.xz \
	w.truncated.xz.asc \
	w-8193.img \
	w-8193.img.asc \
	w-8193.img.gz \
	w-8193.img.gz.asc \
	w-8193.img.xz \
	w-8193.img.xz.asc \
	$(NULL)

CLEANFILES += $(test_data)

# "OS image" consisting of the same byte repeated to 4 MiB
w.img:
	$(AM_V_GEN) python3 -c 'print("w" * (2 ** 22), end="")' > $@

# The same byte repeated to 4 MiB + 512 bytes (8193 sectors). All Endless OS
# images are a multiple of 512 bytes, but not necessarily (in practice, almost
# never) a multiple of 1 MiB, which is the buffer size we read and write.
w-8193.img:
	$(AM_V_GEN) python3 -c 'print("w" * (8193 * 512), end="")' > $@

# Truncated compressed files, with valid signatures, to test handling of
# decompression errors.
w.truncated.%z: w.img.%z
	head --bytes $$(( $$(stat --format='%s' $<) / 2 )) $< >$@

%.img.xz: %.img
	$(AM_V_GEN) rm -f $@ && xz -0 --keep $<

%.img.gz: %.img
	$(AM_V_GEN) rm -f $@ && gzip -1 --keep $<

# Test "images" are signed by a throwaway key with no passphrase.
# The public half is used as the test keyring
%.asc: % secret.asc sign-file
	$(AM_V_GEN) $(srcdir)/sign-file \
		--gpg-path $(GPG_PATH) \
		--gpgconf-path $(GPGCONF_PATH) \
		$(srcdir)/secret.asc $<

%.sha256: %
	$(AM_V_GEN) sha256sum $< >$@

test_scribe_SOURCES = \
	test-error-input-stream.c \
	test-error-input-stream.h \
	test-scribe.c \
	$(NULL)
test_scribe_CFLAGS = \
	$(INITIAL_SETUP_CFLAGS) \
	$(IMAGE_INSTALLER_CFLAGS) \
	-I $(top_srcdir)/ext/libglnx \
	-I $(top_srcdir)/gnome-image-installer/pages/install \
	-I $(top_srcdir)/gnome-image-installer/util \
	$(WARN_CFLAGS) \
	$(NULL)
test_scribe_LDADD = \
	$(INITIAL_SETUP_LIBS) \
	$(IMAGE_INSTALLER_LIBS) \
	$(top_builddir)/ext/libglnx.la \
	$(top_builddir)/gnome-image-installer/pages/install/libgisinstall.la \
	$(NULL)
test_scribe_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(NULL)

test_unattended_config_SOURCES = test-unattended-config.c
test_unattended_config_CFLAGS = \
	$(INITIAL_SETUP_CFLAGS) \
	$(IMAGE_INSTALLER_CFLAGS) \
	-I $(top_srcdir)/ext/libglnx \
	-I $(top_srcdir)/gnome-image-installer/util \
	$(WARN_CFLAGS) \
	$(NULL)
test_unattended_config_LDADD = \
	$(INITIAL_SETUP_LIBS) \
	$(IMAGE_INSTALLER_LIBS) \
	$(top_builddir)/ext/libglnx.la \
	$(top_builddir)/gnome-image-installer/util/libgiiutil.la \
	$(NULL)
test_unattended_config_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(NULL)

test_dmi_SOURCES = test-dmi.c
test_dmi_CFLAGS = \
	$(INITIAL_SETUP_CFLAGS) \
	$(IMAGE_INSTALLER_CFLAGS) \
	-I $(top_srcdir)/gnome-image-installer/util \
	$(WARN_CFLAGS) \
	$(NULL)
test_dmi_LDADD = \
	$(INITIAL_SETUP_LIBS) \
	$(IMAGE_INSTALLER_LIBS) \
	$(top_builddir)/gnome-image-installer/util/libgiiutil.la \
	$(NULL)
test_dmi_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(NULL)

test_write_diagnostics_SOURCES = test-write-diagnostics.c
test_write_diagnostics_CFLAGS = \
	$(INITIAL_SETUP_CFLAGS) \
	$(IMAGE_INSTALLER_CFLAGS) \
	-I $(top_srcdir)/ext/libglnx \
	-I $(top_srcdir)/gnome-image-installer/util \
	$(WARN_CFLAGS) \
	$(NULL)
test_write_diagnostics_LDADD = \
	$(INITIAL_SETUP_LIBS) \
	$(IMAGE_INSTALLER_LIBS) \
	$(top_builddir)/ext/libglnx.la \
	$(top_builddir)/gnome-image-installer/util/libgiiutil.la \
	$(NULL)
test_write_diagnostics_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(NULL)
