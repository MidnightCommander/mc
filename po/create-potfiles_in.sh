#!/bin/sh
set -e

if [ -z "$1" ] ;then
    echo "usage: $0 <toplevel-source-dir>"
    exit 1
fi

cd "$1" && \
    "${FIND:-find}" . -type f -name '*.[ch]' -exec \
    "${GREP:-grep}" -l '_\s*(\s*"' {} + | \
    "${SORT:-sort}" | "${UNIQ:-uniq}" > po/POTFILES.in
