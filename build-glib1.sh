#! /bin/sh

# Download and build glib 1.2.x statically and then compile GNU Midnight
# Commander against it.
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

: ${MC_TOPDIR=`pwd`}
: ${GLIB_VERSION=1.2.10}

GLIB_DIR="glib-$GLIB_VERSION"
GLIB_TARBALL="glib-$GLIB_VERSION.tar.gz"
GLIB_URL="ftp://ftp.gtk.org/pub/gtk/v1.2/$GLIB_TARBALL"
GLIB_INSTDIR="$MC_TOPDIR/glib-inst"

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

if gzip -vt "$GLIB_TARBALL"; then : ; else
  get_file "$GLIB_URL"
fi

rm -rf "$GLIB_DIR"
gzip -cd "$GLIB_TARBALL" | tar xf -
cd "$GLIB_DIR"
if test -f glist.c; then : ; else
  echo "glib source is incomplete" 2>&1
  exit 1
fi

rm -rf "$GLIB_INSTDIR"
./configure --disable-shared --with-glib12 --prefix="$GLIB_INSTDIR" || exit 1
make all || exit 1
make install || exit 1

cd "$MC_TOPDIR"
./configure GLIB_CONFIG="$GLIB_INSTDIR/bin/glib-config" \
	    PKG_CONFIG=/bin/false "$@" || exit 1
make clean || exit 1
make || exit 1

echo "GNU Midnight Commander has been successfully compiled"
