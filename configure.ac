AC_INIT([eos-installer],[3.10.1.1])

AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIRS([m4 buildutil])

AX_IS_RELEASE([git-directory])
AC_REQUIRE_AUX_FILE([tap-driver.sh])

AM_INIT_AUTOMAKE([dist-xz no-dist-gzip foreign tar-ustar])
AM_SILENT_RULES([yes])

AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
AC_SYS_LARGEFILE

LT_INIT

IT_PROG_INTLTOOL([0.40])

GETTEXT_PACKAGE=eos-installer
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [gettext package])

AM_GLIB_GNU_GETTEXT

NETWORK_MANAGER_REQUIRED_VERSION=0.9.6.4
GLIB_REQUIRED_VERSION=2.40.0
GTK_REQUIRED_VERSION=3.7.11
PANGO_REQUIRED_VERSION=1.32.5
IBUS_REQUIRED_VERSION=1.4.99
GNOME_DESKTOP_REQUIRED_VERSION=3.7.5
POLKIT_REQUIRED_VERSION=0.103
GDM_REQUIRED_VERSION=3.8.3

PKG_CHECK_MODULES(INITIAL_SETUP,
                  NetworkManager >= $NETWORK_MANAGER_REQUIRED_VERSION
                  libnm-glib >= $NETWORK_MANAGER_REQUIRED_VERSION
                  libnm-util >= $NETWORK_MANAGER_REQUIRED_VERSION
                  libnm-gtk >= $NETWORK_MANAGER_REQUIRED_VERSION
                  polkit-gobject-1 >= $POLKIT_REQUIRED_VERSION
                  accountsservice
                  gnome-desktop-3.0 >= $GNOME_DESKTOP_REQUIRED_VERSION
                  gstreamer-1.0
                  fontconfig
                  geoclue-2.0
                  geocode-glib-1.0
                  gweather-3.0
                  goa-1.0
                  goa-backend-1.0
                  gtk+-3.0 >= $GTK_REQUIRED_VERSION
                  gio-unix-2.0 >= $GLIB_REQUIRED_VERSION
                  gdm >= $GDM_REQUIRED_VERSION
                  pango >= $PANGO_REQUIRED_VERSION
                  pwquality
                  libsecret-1
                  evince-view-3.0
                  evince-document-3.0
                  webkit2gtk-4.0)

PKG_CHECK_MODULES(COPY_WORKER, gio-2.0 gnome-keyring-1)

PKG_CHECK_MODULES(IMAGE_INSTALLER, udisks2 liblzma x11)

GEOCLUE_DBUS_INTERFACE_XML=`pkg-config --variable=dbus_interface geoclue-2.0`
if test "x$GEOCLUE_DBUS_INTERFACE_XML" = "x"; then
   AC_MSG_ERROR([Cannot find dbus_interface variable in geoclue-2.0.pc])
fi
AC_SUBST(GEOCLUE_DBUS_INTERFACE_XML)

# Zint barcode
AC_CHECK_LIB(zint, ZBarcode_Render,
        [have_libzint=yes], [have_libzint=no])
if test "x$have_libzint" = "xno" ; then
   AC_MSG_ERROR([*** libzint not found ***])
fi
LIBZINT_CFLAGS=""
LIBZINT_LIBS="-lzint"
AC_SUBST(LIBZINT_CFLAGS)
AC_SUBST(LIBZINT_LIBS)

INITIAL_SETUP_CFLAGS="$INITIAL_SETUP_CFLAGS $LIBZINT_CFLAGS"
INITIAL_SETUP_LIBS="$INITIAL_SETUP_LIBS $LIBZINT_LIBS"

AC_ARG_ENABLE(ibus,
        AS_HELP_STRING([--disable-ibus],
                       [Disable IBus support]),
        enable_ibus=$enableval,
        enable_ibus=yes)

AS_IF([test x$enable_ibus = xyes],
      [PKG_CHECK_MODULES(IBUS,
                        ibus-1.0 >= $IBUS_REQUIRED_VERSION,
                        have_ibus=yes, have_ibus=no)],
      [have_ibus=no])

if test x$have_ibus = xyes; then
  INITIAL_SETUP_CFLAGS="$INITIAL_SETUP_CFLAGS $IBUS_CFLAGS"
  INITIAL_SETUP_LIBS="$INITIAL_SETUP_LIBS $IBUS_LIBS"
  AC_DEFINE(HAVE_IBUS, 1, [Build with IBus support?])
fi

# Kerberos kerberos support
AC_PATH_PROG(KRB5_CONFIG, krb5-config, no)
if test "$KRB5_CONFIG" = "no"; then
  AC_MSG_ERROR([krb5-config executable not found in your path - should be installed with the kerberos libraries])
fi

AC_MSG_CHECKING(for krb5 libraries and flags)
KRB5_CFLAGS="`$KRB5_CONFIG --cflags`"
KRB5_LIBS="`$KRB5_CONFIG --libs`"
AC_MSG_RESULT($KRB5_CFLAGS $KRB5_LIBS)

AC_SUBST(KRB5_CFLAGS)
AC_SUBST(KRB5_LIBS)

INITIAL_SETUP_CFLAGS="$INITIAL_SETUP_CFLAGS $KRB5_CFLAGS"
INITIAL_SETUP_LIBS="$INITIAL_SETUP_LIBS $KRB5_LIBS"

# This is a hard-dependency for the region and user-accounts panels
PKG_CHECK_MODULES(ISOCODES, iso-codes)

AC_DEFINE_UNQUOTED([ISO_CODES_PREFIX],["`$PKG_CONFIG --variable=prefix iso-codes`"],[ISO codes prefix])
ISO_CODES=iso-codes

# systemd
AC_ARG_WITH([systemdsystemunitdir],
        AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd service files]),
        [],
        [with_systemdsystemunitdir=yes])
if test "x$with_systemdsystemunitdir" = "xyes"; then
    with_systemdsystemunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)
fi
if test "x$with_systemdsystemunitdir" != "xno"; then
    AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])
fi

GLIB_TESTS

AC_SUBST(INITIAL_SETUP_CFLAGS)
AC_SUBST(INITIAL_SETUP_LIBS)

# -Wno-declaration-after-statement:
#   Mixed decls are used a lot and are harmless.
# -Wno-packed:
#   gpt.h structs are naturally packed, so the compiler warns that the
#   attribute is unnecessary. I am not quite comfortable removing it even
#   though I believe the compiler's argument: it has value as documentation.
AX_COMPILER_FLAGS([WARN_CFLAGS], [WARN_LDFLAGS], [], [],
                  [-Wno-declaration-after-statement -Wno-packed])

GNOME_MAINTAINER_MODE_DEFINES

AC_SUBST(CFLAGS)
AC_PATH_PROG(GLIB_COMPILE_RESOURCES, glib-compile-resources)

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
Makefile
gnome-initial-setup/Makefile
gnome-initial-setup/pages/Makefile
gnome-initial-setup/pages/language/Makefile
gnome-initial-setup/pages/display/Makefile
gnome-initial-setup/pages/endless-eula/Makefile
gnome-initial-setup/pages/eulas/Makefile
gnome-initial-setup/pages/account/Makefile
gnome-initial-setup/pages/location/Makefile
gnome-initial-setup/pages/network/Makefile
gnome-initial-setup/pages/goa/Makefile
gnome-initial-setup/pages/summary/Makefile
gnome-initial-setup/pages/keyboard/Makefile
gnome-image-installer/Makefile
gnome-image-installer/util/Makefile
gnome-image-installer/pages/Makefile
gnome-image-installer/pages/diskimage/Makefile
gnome-image-installer/pages/disktarget/Makefile
gnome-image-installer/pages/finished/Makefile
gnome-image-installer/pages/install/Makefile
po/Makefile.in
data/Makefile
eos-installer-data/Makefile
tests/Makefile
])
AC_OUTPUT
