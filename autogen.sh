#!/bin/sh

set -e

srcdir="$(cd "$(dirname "$0")" && pwd)"

cd "$srcdir"

${AUTORECONF:-autoreconf} --verbose --install --force -I m4 ${AUTORECONF_FLAGS}

# Customize the INSTALL file
rm -f INSTALL && ln -s doc/INSTALL

# Generate po/POTFILES.in
${XGETTEXT:-xgettext} --keyword=_ --keyword=N_ --keyword=Q_ --output=- \
	`find . -name '*.[ch]'` | sed -ne '/^#:/{s/#://;s/:[0-9]*/\
/g;s/ //g;p;}' | \
	grep -v '^$' | sort | uniq >po/POTFILES.in

cd src/vfs/smbfs/helpers
date -u >include/stamp-h.in

$srcdir/maint/utils/version.sh "$srcdir"

if test -x $srcdir/configure.mc; then
  $srcdir/configure.mc "$@"
fi
