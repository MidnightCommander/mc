#!/bin/sh

set -e

# Use backticks to support ksh on Solaris
basedir=`dirname "$0"`
srcdir=`cd "$basedir" && pwd`

cd "$srcdir"

${AUTORECONF:-autoreconf} --verbose --install --force -I m4 ${AUTORECONF_FLAGS}

# Customize the INSTALL file
rm -f INSTALL && ln -s doc/INSTALL .

# Generate po/POTFILES.in
if ! xgettext -h 2>&1 | grep -- '--keyword' >/dev/null ; then
  echo "gettext is unable to extract translations, set XGETTEXT to GNU gettext!" >&2
  touch po/POTFILES.in
else
  ${XGETTEXT:-xgettext} --keyword=_ --keyword=N_ --keyword=Q_ --output=- \
	`find . -name '*.[ch]'` | ${SED-sed} -ne '/^#:/{s/#://;s/:[0-9]*/\
/g;s/ //g;p;}' | \
	grep -v '^$' | sort | uniq >po/POTFILES.in
fi

"$srcdir/version.sh" "$srcdir"

if test -x "$srcdir/configure.mc"; then
  "$srcdir/configure.mc" "$@"
fi
