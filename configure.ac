AC_INIT([eos-installer],[3.10.1.1])

AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIRS([m4 buildutil ext/libglnx])

AX_IS_RELEASE([git-directory])
AC_REQUIRE_AUX_FILE([tap-driver.sh])

AM_INIT_AUTOMAKE([1.13 dist-xz no-dist-gzip foreign tar-ustar subdir-objects])
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

GLIB_REQUIRED_VERSION=2.40.0
GTK_REQUIRED_VERSION=3.7.11
GNOME_DESKTOP_REQUIRED_VERSION=3.7.5

PKG_CHECK_MODULES(INITIAL_SETUP,
                  gnome-desktop-3.0 >= $GNOME_DESKTOP_REQUIRED_VERSION
                  gtk+-3.0 >= $GTK_REQUIRED_VERSION
                  gio-unix-2.0 >= $GLIB_REQUIRED_VERSION
                  libsecret-1
                  )

PKG_CHECK_MODULES([LIBGLNX], [gio-unix-2.0 >= $GLIB_REQUIRED_VERSION])

PKG_CHECK_MODULES(IMAGE_INSTALLER, udisks2 liblzma x11)

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
LIBGLNX_CONFIGURE

# Both the tests and the application invoke the GPG executable
AC_ARG_VAR([GPG_PATH], [GPG executable])
AC_PATH_PROG([GPG_PATH], [gpg])
AS_IF([test -z "$GPG_PATH"], [AC_MSG_ERROR([GPG not found])])
AC_DEFINE_UNQUOTED([GPG_PATH], ["$GPG_PATH"], [Path to GPG executable])

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
ext/Makefile
gnome-initial-setup/Makefile
gnome-image-installer/Makefile
gnome-image-installer/util/Makefile
gnome-image-installer/pages/Makefile
gnome-image-installer/pages/confirm/Makefile
gnome-image-installer/pages/diskimage/Makefile
gnome-image-installer/pages/disktarget/Makefile
gnome-image-installer/pages/finished/Makefile
gnome-image-installer/pages/install/Makefile
po/Makefile.in
eos-installer-data/Makefile
tests/Makefile
])
AC_OUTPUT
