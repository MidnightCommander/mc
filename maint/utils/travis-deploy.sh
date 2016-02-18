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

mkdir HTML && cd HTML

echo "source.midnight-commander.org" > CNAME
echo "It works!" > index.html

touch .nojekyll

git init

git config user.name "Travis CI"
git config user.email "travis@midnight-commander.org"

git add .
git commit -m "Deploy to GitHub Pages"

git push --force --quiet git@github.com:MidnightCommander/source.git master:gh-pages > /dev/null 2>&1

exit 0
