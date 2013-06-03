#!/bin/bash

# Midnight Commander - push doc/hints/mc.hint file to Transifex
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

copyPotToWorkDir() {
    work_dir=$1; shift
    source_dir=$1; shift

    cp -f "${source_dir}/mc.pot" "${work_dir}"
}

#*** main code *********************************************************

WORK_DIR=$(initSyncDirIfNeeded "po")

copyPotToWorkDir "${WORK_DIR}" "${MC_SOURCE_ROOT_DIR}/po"

sendSourceToTransifex "${WORK_DIR}"

