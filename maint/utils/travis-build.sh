#!/bin/bash
#
# Midnight Commander - build and test common configurations
#
# Copyright (C) 2015
# The Free Software Foundation, Inc.
#
# Written by:
#  Slava Zanko <slavazanko@gmail.com>, 2015
#  Yury V. Zaytsev <yury@shurup.com>, 2015
#
# This file is part of the Midnight Commander.
#
# The Midnight Commander is free software: you can redistribute it
# and/or modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation, either version 3 of the License,
# or (at your option) any later version.
#
# The Midnight Commander is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

set -e
set -x

function do_build() {
    make
    make -k check
    make install
}

# Build distribution archive
mkdir -p distrib && cd $_

../configure
make dist-bzip2

DISTFILE=$(ls mc-*.tar.bz2)
DISTDIR=$(basename "$DISTFILE" .tar.bz2)

tar -xjf $DISTFILE
cd $DISTDIR

# Build default configuration (S-Lang)
mkdir -p build-default && pushd $_

../configure \
    --prefix="$(pwd)/INSTALL_ROOT" \
    --with-screen=slang \
    --enable-maintainer-mode \
    --enable-mclib \
    --enable-charset \
    --enable-aspell \
    --enable-tests \
    --enable-werror

do_build

popd

# Build default configuration (ncurses)
mkdir -p build-ncurses && pushd $_

../configure \
    --prefix="$(pwd)/INSTALL_ROOT" \
    --with-screen=ncurses \
    --enable-maintainer-mode \
    --enable-mclib \
    --enable-charset \
    --enable-aspell \
    --enable-tests \
    --enable-werror

do_build

popd

# Build all disabled
mkdir -p build-all-disabled && pushd $_

../configure \
    --prefix="$(pwd)/INSTALL_ROOT" \
    --disable-maintainer-mode \
    --disable-mclib \
    --disable-charset \
    --disable-aspell \
    --disable-largefile \
    --disable-nls \
    --disable-vfs \
    --disable-background \
    --without-mmap \
    --without-x \
    --without-gpm-mouse \
    --without-internal-edit \
    --without-diff-viewer \
    --without-subshell \
    --enable-tests \
    --enable-werror

do_build

popd
