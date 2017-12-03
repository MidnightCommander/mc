#!/bin/bash
#
# Midnight Commander - deployment script for Travis CI
#
# Copyright (C) 2016
# The Free Software Foundation, Inc.
#
# Written by:
#  Yury V. Zaytsev <yury@shurup.com>, 2016
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

GLOBAL_VERSION="6.5.7"
GLOBAL_URL="http://ftp.gnu.org/pub/gnu/global/global-${GLOBAL_VERSION}.tar.gz"
HTAGSFIX_URL="https://github.com/mooffie/htagsfix/raw/master/htagsfix"

mkdir .global && pushd .global # ignored by GLOBAL's indexer

    wget ${GLOBAL_URL}
    tar zxvf global-${GLOBAL_VERSION}.tar.gz > /dev/null 2>&1

    pushd global-${GLOBAL_VERSION}
        ./configure --prefix=$(pwd)/install > /dev/null 2>&1
        make > /dev/null 2>&1
        make install > /dev/null 2>&1
    popd

    export PATH="$(pwd)/global-${GLOBAL_VERSION}/install/bin:$PATH"

popd

gtags -v > /dev/null 2>&1

htags --suggest -t "Welcome to the Midnight Commander source tour!" > /dev/null 2>&1

wget --no-check-certificate ${HTAGSFIX_URL}

ruby htagsfix > /dev/null 2>&1

cd HTML

touch .nojekyll
echo "source.midnight-commander.org" > CNAME

git init

git config user.name "Travis CI"
git config user.email "travis@midnight-commander.org"

git add . > /dev/null 2>&1
git commit -m "Deploy to GitHub Pages" > /dev/null 2>&1

git push --force --quiet git@github.com:MidnightCommander/source.git master:gh-pages > /dev/null 2>&1

exit 0
