include $(top_srcdir)/buildutil/glib-tap.mk

test_programs = \
	test-scribe \
	$(NULL)

dist_test_data = \
	public.asc \
	secret.asc \
	sign-file \
	wjt.asc \
	$(NULL)

test_data = \
	w.img \
	w.img.asc \
	w.img.gz \
	w.img.gz.asc \
	w.img.xz \
	w.img.xz.asc \
	$(NULL)

CLEANFILES += $(test_data)

# "OS image" consisting of the same byte repeated to 4 MiB
w.img:
	$(AM_V_GEN) python3 -c 'print("w" * (2 ** 22), end="")' > $@

%.img.xz: %.img
	$(AM_V_GEN) rm -f $@ && xz -0 --keep $<

%.img.gz: %.img
	$(AM_V_GEN) rm -f $@ && gzip -1 --keep $<

# Test "images" are signed by a throwaway key with no passphrase.
# The public half is used as the test keyring
%.asc: % secret.asc sign-file
	$(AM_V_GEN) $(srcdir)/sign-file $(srcdir)/secret.asc $<

test_scribe_SOURCES = test-scribe.c
test_scribe_CFLAGS = \
	$(INITIAL_SETUP_CFLAGS) \
	$(IMAGE_INSTALLER_CFLAGS) \
	-I $(top_srcdir)/gnome-image-installer/pages/install \
	$(WARN_CFLAGS) \
	$(NULL)
test_scribe_LDADD = \
	$(INITIAL_SETUP_LIBS) \
	$(IMAGE_INSTALLER_LIBS) \
	$(top_builddir)/gnome-image-installer/pages/install/libgisinstall.la \
	-llzma \
	$(NULL)
test_scribe_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(NULL)
