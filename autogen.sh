#! /bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

(
cd $srcdir
if test -d macros; then
	test -f mc-aclocal.m4 && test -f gettext.m4 && cat mc-aclocal.m4 gettext.m4 > acinclude.m4
	aclocal -I macros $ACLOCAL_FLAGS
else
	echo macros directory not found, skipping generation of aclocal.m4
fi
autoheader
autoconf
)

$srcdir/configure --enable-maintainer-mode $*
