#! /bin/sh
# Run this to generate all the initial makefiles, etc.

# Don't ignore errors.
set -e

# Make it possible to specify path in the environment
: ${AUTOCONF=autoconf}
: ${AUTOHEADER=autoheader}
: ${AUTOMAKE=automake}
: ${ACLOCAL=aclocal}
: ${GETTEXTIZE=gettextize}
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
gettext_ver=`$GETTEXTIZE --version | \
  sed '2,$d;			# remove all but the first line
       s/.* //;			# take text after the last space
       s/-.*//;			# strip "-pre" or "-rc" at the end
       s/\([^.][^.]*\)/0\1/g;	# prepend 0 to every token
       s/0\([^.][^.]\)/\1/g;	# strip leading 0 from long tokens
       s/$/.00.00/;		# add .00.00 for short version strings
       s/\.//g;			# remove dots
       s/\(......\).*/\1/;	# leave only 6 leading digits
       '`

if test -z "$gettext_ver"; then
  echo "Cannot determine version of gettext" 2>&1
  exit 1
fi

if test "$gettext_ver" -lt 01038; then
  echo "Don't use gettext older than 0.10.38" 2>&1
  exit 1
fi

rm -rf intl
if test "$gettext_ver" -ge 01100; then
  if test "$gettext_ver" -lt 01105; then
    echo "Upgrade gettext to at least 0.11.5 or downgrade to 0.10.40" 2>&1
    exit 1
  fi
  $AUTOPOINT --force || exit 1
else
  $GETTEXTIZE --copy --force || exit 1
  if test -e po/ChangeLog~; then
    rm -f po/ChangeLog
    mv po/ChangeLog~ po/ChangeLog
  fi
fi

# Generate po/POTFILES.in
$XGETTEXT --keyword=_ --keyword=N_ --output=- `find . -name '*.[ch]'` | \
	sed -ne '/^#:/{s/#://;s/:[0-9]*/\
/g;s/ //g;p;}' | \
	grep -v '^$' | sort | uniq | grep -v 'regex.c' >po/POTFILES.in

ACLOCAL_INCLUDES="-I m4"

# Some old version of GNU build tools fail to set error codes.
# Check that they generate some of the files they should.

$ACLOCAL $ACLOCAL_INCLUDES $ACLOCAL_FLAGS
test -f aclocal.m4 || \
  { echo "aclocal failed to generate aclocal.m4" 2>&1; exit 1; }

$AUTOHEADER || exit 1
test -f config.h.in || \
  { echo "autoheader failed to generate config.h.in" 2>&1; exit 1; }

$AUTOCONF || exit 1
test -f configure || \
  { echo "autoconf failed to generate configure" 2>&1; exit 1; }

# Workaround for Automake 1.5 to ensure that depcomp is distributed.
if test "`$AUTOMAKE --version|awk '{print $NF;exit}'`" = '1.5' ; then
    $AUTOMAKE -a src/Makefile
fi
$AUTOMAKE -a
test -f Makefile.in || \
  { echo "automake failed to generate Makefile.in" 2>&1; exit 1; }

cd vfs/samba
date -u >include/stamp-h.in

$AUTOHEADER
test -f include/config.h.in || \
  { echo "autoheader failed to generate vfs/samba/include/config.h.in" 2>&1; exit 1; }

$AUTOCONF
test -f configure || \
  { echo "autoconf failed to generate vfs/samba/configure" 2>&1; exit 1; }
) || exit 1

if test -x $srcdir/configure.mc; then
  $srcdir/configure.mc "$@"
else
  $srcdir/configure --cache-file=config.cache --enable-maintainer-mode "$@"
fi
