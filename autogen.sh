#!/bin/sh

set -e

# Use backticks to support ksh on Solaris
basedir=`dirname "$0"`
srcdir=`cd "$basedir" && pwd`

cd "$srcdir"

${AUTORECONF:-autoreconf} --verbose --install --force -I m4 ${AUTORECONF_FLAGS}

# Customize the INSTALL file
rm -f INSTALL && ln -s doc/INSTALL .

"$srcdir/version.sh" "$srcdir"

if test -x "$srcdir/configure.mc"; then
  "$srcdir/configure.mc" "$@"
fi
