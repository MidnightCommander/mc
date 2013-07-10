#!/bin/bash

# Midnight Commander - fetch doc/hints/mc.hint translations from Transifex
#
# Copyright (C) 2013
# The Free Software Foundation, Inc.
#
# Written by:
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

set -e

MC_SOURCE_ROOT_DIR=${MC_SOURCE_ROOT_DIR:-$(dirname $(dirname $(dirname $(pwd))))}

#*** include section (source functions, for example) *******************

source "${MC_SOURCE_ROOT_DIR}/maint/utils/sync-transifex/functions"

#*** file scope functions **********************************************

stripLocation() {
    work_dir=$1; shift

    for i in $(find "${work_dir}" -name '*.po' -print); do
        sed -i '/^#:/d' "${i}"
    done
}

# ----------------------------------------------------------------------

copyFilesToSourceDir() {
    work_dir=$1; shift
    source_dir=$1; shift

    exclude_list_file=$(getConfigFile "po" "po-ignore.list")

    for i in $(find "${work_dir}" -name '*.po' -print | sort); do
        [ $(grep -c "^\s*$(basename ${i})" "${exclude_list_file}") -ne 1 ] && {
            cp -f "${i}" "${source_dir}"
        }
    done
}

#*** main code *********************************************************

WORK_DIR=$(initSyncDirIfNeeded "po")

receiveTranslationsFromTransifex "${WORK_DIR}"

stripLocation "${WORK_DIR}"

copyFilesToSourceDir "${WORK_DIR}" "${MC_SOURCE_ROOT_DIR}/po"
