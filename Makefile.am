ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

AM_CFLAGS = ${WARN_CFLAGS}

SUBDIRS = gnome-initial-setup gnome-image-installer po data eos-installer-data

EXTRA_DIST = \
    autogen.sh \
    COPYING \
    AUTHORS \
    NEWS \
    eos-installer.doap

DISTCHECK_CONFIGURE_FLAGS = \
    --with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir)
