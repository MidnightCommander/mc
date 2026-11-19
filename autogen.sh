#!/bin/sh

set -e

# Use backticks to support ksh on Solaris
basedir=`dirname "$0"`
srcdir=`cd "$basedir" && pwd`

cd "$srcdir"

${AUTORECONF:-autoreconf} --verbose --install --force -I m4 ${AUTORECONF_FLAGS}

# Generate po/POTFILES.in from sources that call `_ ()`, `N_ ()`, `Q_ ()`
# or `ngettext ()`; false positives are harmless, `xgettext` skips them
for f in `find lib src -name '*.[ch]'`; do
  if grep '_ *(' "$f" >/dev/null || grep 'gettext *(' "$f" >/dev/null; then
    echo "$f"
  fi
done | LC_ALL=C sort >po/POTFILES.in

"$srcdir/version.sh" "$srcdir"

if test -x "$srcdir/configure.mc"; then
  "$srcdir/configure.mc" "$@"
fi
