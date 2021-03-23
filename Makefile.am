ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

AM_CFLAGS = ${WARN_CFLAGS}

SUBDIRS = \
    ext \
    gnome-initial-setup \
    gnome-image-installer \
    po \
    eos-installer-data \
    tests \
    $(NULL)

EXTRA_DIST = \
    autogen.sh \
    buildutil/glib-tap.mk \
    buildutil/glibtests.m4 \
    buildutil/tap-test \
    COPYING \
    AUTHORS \
    eos-installer.doap

DISTCHECK_CONFIGURE_FLAGS = \
    --with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir)

# FIXME: Work around https://bugs.launchpad.net/intltool/+bug/1712194
distclean-local:
	rm -f po/.intltool-merge-cache.lock
