#! /bin/sh

# Download and build glib 2.x statically with all dependencies and then
# compile GNU Midnight Commander against it.
# Copyright (C) 2003 Pavel Roskin
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.


# This script is incomplete!  It doesn't download libiconv.  This is OK
# for glibc-based systems, but probably not for others.  This limitation
# is known.  Please don't report it.


: ${MC_TOPDIR=`pwd`}
: ${TMP_INSTDIR=$MC_TOPDIR/tmp-inst}
: ${GLIB_VERSION=2.12.6}
: ${PKGC_VERSION=0.21}
: ${GETTEXT_VERSION=0.16.1}

GLIB_DIR="glib-$GLIB_VERSION"
GLIB_TARBALL="glib-$GLIB_VERSION.tar.gz"
GLIB_URL="ftp://ftp.gtk.org/pub/glib/2.12/$GLIB_TARBALL"

PKGC_DIR="pkg-config-$PKGC_VERSION"
PKGC_TARBALL="pkg-config-$PKGC_VERSION.tar.gz"
PKGC_URL="http://pkgconfig.freedesktop.org/releases/$PKGC_TARBALL"

GETTEXT_DIR="gettext-$GETTEXT_VERSION/gettext-runtime"
GETTEXT_TARBALL="gettext-$GETTEXT_VERSION.tar.gz"
GETTEXT_URL="ftp://ftp.gnu.org/gnu/gettext/$GETTEXT_TARBALL"

get_file() {
  curl --remote-name "$1" || \
  wget --passive-ftp "$1" || \
  wget "$1" || \
  ftp "$1" </dev/null || \
  exit 1
}

if test -f src/dir.c; then : ; else
  echo "Not in the top-level directory of GNU Midnight Commander." 2>&1
  exit 1
fi

if test -f configure; then : ; else
  ./autogen.sh --help >/dev/null || exit 1
fi

rm -rf "$TMP_INSTDIR"
PATH="$TMP_INSTDIR/bin:$PATH"
export PATH

# Compile gettext
cd "$MC_TOPDIR"
if gzip -vt "$GETTEXT_TARBALL"; then : ; else
  get_file "$GETTEXT_URL"
fi

rm -rf "$GETTEXT_DIR"
gzip -cd "$GETTEXT_TARBALL" | tar xf -
cd "$GETTEXT_DIR"
if test -f src/gettext.c; then : ; else
  echo "gettext source is incomplete" 2>&1
  exit 1
fi

./configure --disable-shared --disable-nls --prefix="$TMP_INSTDIR" || exit 1
make all || exit 1
make install || exit 1

# Compile pkgconfig
cd "$MC_TOPDIR"
if gzip -vt "$PKGC_TARBALL"; then : ; else
  get_file "$PKGC_URL"
fi

rm -rf "$PKGC_DIR"
gzip -cd "$PKGC_TARBALL" | tar xf -
cd "$PKGC_DIR"
if test -f pkg.c; then : ; else
  echo "pkgconfig source is incomplete" 2>&1
  exit 1
fi

./configure --disable-shared --prefix="$TMP_INSTDIR" || exit 1
make all || exit 1
make install || exit 1

# Compile glib
cd "$MC_TOPDIR"
if gzip -vt "$GLIB_TARBALL"; then : ; else
  get_file "$GLIB_URL" || exit 1
fi

rm -rf "$GLIB_DIR"
gzip -cd "$GLIB_TARBALL" | tar xf -
cd "$GLIB_DIR"
if test -f glib/glist.c; then : ; else
  echo "glib source is incomplete" 2>&1
  exit 1
fi

./configure --disable-shared --prefix="$TMP_INSTDIR" \
	    PKG_CONFIG="$TMP_INSTDIR/bin/pkg-config" \
	    CPPFLAGS="-I$TMP_INSTDIR/include" \
	    LDFLAGS="-L$TMP_INSTDIR/lib" || exit 1
make all || exit 1
make install || exit 1

cd "$MC_TOPDIR"
./configure PKG_CONFIG="$TMP_INSTDIR/bin/pkg-config" || exit 1
make clean || exit 1
make || exit 1

echo "GNU Midnight Commander has been successfully compiled"
