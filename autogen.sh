cat macros/gnome.m4 mc-aclocal.m4 > aclocal.m4
autoconf
./configure $*
