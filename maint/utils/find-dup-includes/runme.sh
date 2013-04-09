#!/bin/bash

set -e

# Midnight Commander - find an 'include' duplicates in src/ and lib/ subdirs
#
# Copyright (C) 2011, 2013
# The Free Software Foundation, Inc.
#
# Written by:
#  Ilia Maslakov <il.smind@gmail.com>, 2011
#  Yury V. Zaytsev <yury@shurup.com>, 2011
#  Slava Zanko <slavazanko@gmail.com>, 2013
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

MC_SOURCE_ROOT_DIR=${MC_SOURCE_ROOT_DIR:-$(dirname $(dirname $(dirname $(pwd))))}

#*** include section (source functions, for example) *******************

#*** file scope functions **********************************************

findIncludeDupsInDir() {
    dir_name=$1; shift

    for i in $(find "${dir_name}" -name '*.[ch]'); do
        file_name=$(echo $i | sed 's@'"${MC_SOURCE_ROOT_DIR}/"'@@g')
        [ $(grep "^\s*${file_name}$" -c "${MC_SOURCE_ROOT_DIR}/maint/utils/find-dup-includes/exclude-list.cfg") -ne 0 ] && continue
        "${MC_SOURCE_ROOT_DIR}/maint/utils/find-dup-includes/find-in-one-file.pl" "${i}"
    done
}

#*** main code *********************************************************

findIncludeDupsInDir "${MC_SOURCE_ROOT_DIR}/src"
findIncludeDupsInDir "${MC_SOURCE_ROOT_DIR}/lib"
