#! /bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

(
cd $srcdir
if test -d macros; then
	aclocal -I macros $ACLOCAL_FLAGS || exit 1
else
	if test -f aclocal.m4; then
		echo Warning: aclocal.m4 cannot be rebuilt
	else
		echo Error: aclocal.m4 cannot be created
		exit 1
	fi
fi
autoheader || exit 1
autoconf || exit 1
automake -a || exit 1
cd vfs/samba || exit 1
autoheader || exit 1
autoconf || exit 1
) || exit 1

$srcdir/configure --cache-file=config.cache --enable-maintainer-mode "$@"
