#! /bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

(
cd $srcdir

: ${CVS=cvs}
test -d macros || $CVS co -d macros gnome-common/macros || exit 1
test -d intl || $CVS co -d intl gnome-common/intl || exit 1

aclocal -I macros $ACLOCAL_FLAGS || exit 1
autoheader || exit 1
autoconf || exit 1
automake -a || exit 1

cd vfs/samba || exit 1
date -u >include/stamp-h.in
autoheader || exit 1
autoconf || exit 1
) || exit 1

$srcdir/configure --cache-file=config.cache --enable-maintainer-mode "$@"
