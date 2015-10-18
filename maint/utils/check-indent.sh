#!/bin/bash
#
# Midnight Commander - check source code indentation
#
# Copyright (C) 2015
# The Free Software Foundation, Inc.
#
# Written by:
#  Slava Zanko <slavazanko@gmail.com>, 2015
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

findUnindentedFiles() {
  local directory=$1
  find "${directory}" -name '*.[ch]' -print0 | \
    xargs -0 indent \
      --gnu-style \
      --format-first-column-comments \
      --indent-level4 \
      --brace-indent0 \
      --line-length100 \
      --no-tabs \
      --blank-lines-after-procedures
  return $(git ls-files --modified | wc -l)
}

( findUnindentedFiles "lib" && findUnindentedFiles "src" && findUnindentedFiles "tests" ) || {
  echo "Sources not indented"
  git ls-files --modified
  exit 1
}
