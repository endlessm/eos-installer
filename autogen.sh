#!/bin/sh
# Run this to generate all the initial makefiles, etc.
set -ex

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

(test -f $srcdir/configure.ac \
  && test -f $srcdir/eos-installer.doap) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level eos-installer directory"
    exit 1
}

which gnome-autogen.sh >/dev/null || {
    echo "You need to install gnome-common"
    exit 1
}

git submodule update --init --recursive

# Workaround automake bug with subdir-objects and computed paths; if
# changing this, please also change ext/Makefile.am.
sed -e 's,$(libglnx_srcpath),libglnx,g' < ext/libglnx/Makefile-libglnx.am >ext/libglnx/Makefile-libglnx.am.inc

REQUIRED_AUTOMAKE_VERSION=1.13
set +x
. gnome-autogen.sh
