srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

(
cd $srcdir
if test -d macros; then
	cat macros/gnome.m4 macros/gnome-vfs.m4 macros/gnome-undelfs.m4 \
	macros/linger.m4 mc-aclocal.m4 gettext.m4 > aclocal.m4
else
	echo macros directory not found, skipping generation of aclocal.m4
fi
autoheader
autoconf
)

$srcdir/configure $*
