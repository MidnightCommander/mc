#! /bin/sh
# Run this to generate all the initial makefiles, etc.

# Make it possible to specify path in the environment
: ${AUTOCONF=autoconf}
: ${AUTOHEADER=autoheader}
: ${AUTOMAKE=automake}
: ${ACLOCAL=aclocal}
: ${GETTEXTIZE=gettextize}

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

(
cd $srcdir

# The autoconf cache (version after 2.52) is not reliable yet.
rm -rf autom4te.cache vfs/samba/autom4te.cache

# Ensure that gettext is reasonably new.
gettext_ver=`$GETTEXTIZE --version | sed -n '1s/\.//g;1s/.* //;1s/^\(...\)$/\100/p;1s/^\(...\)\(.\)$/\10\2/p'`
if test $gettext_ver -lt 01038; then
  echo "Don't use gettext older than 0.10.38" 2>&1
  exit 1
fi

if test $gettext_ver -ge 01100; then
  GETTEXTIZE_ARGS='--intl --no-changelog'
fi

rm -rf intl
$GETTEXTIZE $GETTEXTIZE_ARGS --copy --force >tmpout || exit 1

if test -e po/ChangeLog~; then
  rm -f po/ChangeLog
  mv po/ChangeLog~ po/ChangeLog
fi

if test ! -d config; then
  mkdir config || exit 1
fi

rm -f aclocal.m4
if test -f `aclocal --print-ac-dir`/gettext.m4; then
  : # gettext macro files are available to aclocal.
else
  # gettext macro files are not available.
  # Find them and copy to a local directory.
  # Ugly way to parse the instructions gettexize gives us.
  m4files="`cat tmpout | sed -n -e '/^Please/,/^from/s/^  *//p'`"
  fromdir=`cat tmpout | sed -n -e '/^Please/,/^from/s/^from the \([^ ]*\) .*$/\1/p'`
  rm tmpout
  rm -rf gettext.m4
  mkdir gettext.m4
  for i in $m4files; do
    cp -f $fromdir/$i gettext.m4
  done
  ACLOCAL_INCLUDES="-I gettext.m4"
fi

# Some old version of GNU build tools fail to set error codes.
# Check that they generate some of the files they should.

$ACLOCAL $ACLOCAL_INCLUDES $ACLOCAL_FLAGS || exit 1
test -f aclocal.m4 || \
  { echo "aclocal failed to generate aclocal.m4" 2>&1; exit 1; }

$AUTOHEADER || exit 1
test -f config.h.in || \
  { echo "autoheader failed to generate config.h.in" 2>&1; exit 1; }

$AUTOCONF || exit 1
test -f configure || \
  { echo "autoconf failed to generate configure" 2>&1; exit 1; }

# Workaround for Automake 1.5 to ensure that depcomp is distributed.
$AUTOMAKE -a src/Makefile || exit 1
$AUTOMAKE -a || exit 1
test -f Makefile.in || \
  { echo "automake failed to generate Makefile.in" 2>&1; exit 1; }

cd vfs/samba || exit 1
date -u >include/stamp-h.in

$AUTOHEADER || exit 1
test -f include/config.h.in || \
  { echo "autoheader failed to generate vfs/samba/include/config.h.in" 2>&1; exit 1; }

$AUTOCONF || exit 1
test -f configure || \
  { echo "autoconf failed to generate vfs/samba/configure" 2>&1; exit 1; }
) || exit 1

$srcdir/configure --cache-file=config.cache --enable-maintainer-mode "$@"
