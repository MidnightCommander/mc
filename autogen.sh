#! /bin/sh
# Run this to generate all the initial makefiles, etc.

# Don't ignore errors.
set -e

# Make it possible to specify path in the environment
: ${AUTOCONF=autoconf}
: ${AUTOHEADER=autoheader}
: ${AUTOMAKE=automake}
: ${ACLOCAL=aclocal}
: ${AUTOPOINT=autopoint}
: ${XGETTEXT=xgettext}

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

(
# Some shells don't propagate "set -e" to subshells.
set -e

cd "$srcdir"

# The autoconf cache (version after 2.52) is not reliable yet.
rm -rf autom4te.cache vfs/samba/autom4te.cache

if test ! -d config; then
  mkdir config
fi

# Ensure that gettext is reasonably new.

# remove all but the first line
sed_cmd="sed -e '2,\$d'"
# take text after the last space
sed_cmd="${sed_cmd} -e 's/.* //'"
# strip "-pre" or "-rc" at the end
sed_cmd="${sed_cmd} -e 's/-.*//'"
# prepend 0 to every token
sed_cmd="${sed_cmd} -e 's/\\([^.][^.]*\\)/0\\1/g'"
# strip leading 0 from long tokens
sed_cmd="${sed_cmd} -e 's/0\\([^.][^.]\\)/\\1/g'"
# add .00.00 for short version strings
sed_cmd="${sed_cmd} -e 's/\$/.00.00/'"
# remove dots
sed_cmd="${sed_cmd} -e 's/\\.//g'"
# leave only 6 leading digits
sed_cmd="${sed_cmd} -e 's/\\(......\\).*/\\1/'"

gettext_ver=`$AUTOPOINT --version | eval ${sed_cmd}`
if test -z "$gettext_ver"; then
  echo "Cannot determine version of gettext" >&2
  exit 1
fi

if test "$gettext_ver" -lt 01105; then
  echo "gettext 0.11.5 or newer is required" >&2
  exit 1
fi

rm -rf intl
$AUTOPOINT --force || exit 1

# Generate po/POTFILES.in
$XGETTEXT --keyword=_ --keyword=N_ --keyword=gettext_ui --output=- \
	`find . -name '*.[ch]'` | sed -ne '/^#:/{s/#://;s/:[0-9]*/\
/g;s/ //g;p;}' | \
	grep -v '^$' | sort | uniq | grep -v 'regex.c' >po/POTFILES.in

ACLOCAL_INCLUDES="-I m4"

# Some old version of GNU build tools fail to set error codes.
# Check that they generate some of the files they should.

$ACLOCAL $ACLOCAL_INCLUDES $ACLOCAL_FLAGS
test -f aclocal.m4 || \
  { echo "aclocal failed to generate aclocal.m4" >&2; exit 1; }

$AUTOHEADER || exit 1
test -f config.h.in || \
  { echo "autoheader failed to generate config.h.in" >&2; exit 1; }

$AUTOCONF || exit 1
test -f configure || \
  { echo "autoconf failed to generate configure" >&2; exit 1; }

# Workaround for Automake 1.5 to ensure that depcomp is distributed.
if test "`$AUTOMAKE --version|awk '{print $NF;exit}'`" = '1.5' ; then
    $AUTOMAKE -a src/Makefile
fi
$AUTOMAKE -a
test -f Makefile.in || \
  { echo "automake failed to generate Makefile.in" >&2; exit 1; }

cd vfs/samba
date -u >include/stamp-h.in

$AUTOHEADER
test -f include/config.h.in || \
  { echo "autoheader failed to generate vfs/samba/include/config.h.in" >&2; exit 1; }

$AUTOCONF
test -f configure || \
  { echo "autoconf failed to generate vfs/samba/configure" >&2; exit 1; }
) || exit 1

if test -x $srcdir/configure.mc; then
  $srcdir/configure.mc "$@"
else
  $srcdir/configure --cache-file=config.cache --enable-maintainer-mode "$@"
fi
