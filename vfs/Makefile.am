## Process this file with automake to produce Makefile.in.

## FIXME: this is not nearly finished.  It is pointless to try to use
## it without fixing it, and libvfs, first.  Feel free!


## This file is used only by the Gnome libvfs build.  If you make
## changes here, they must be synchronized with Make-mc.in.

lib_LTLIBRARIES = libvfs.la

if FALSE
# FIXME: to fool make dist.
# In `mc' mad.c and mad.h are symlinks to topdir/src/mad.[ch]
BUILT_SOURCES = mad.c mad.h
mad_ch = mad.c mad.h
endif

libvfs_la_SOURCES = callback.h container.c container.h extfs.c extfs.h \
ftpfs.c ftpfs.h libvfs.c local.c $(mad_ch) mcfs.c mcfs.h mcserv.c \
names.c names.h shared.c tar.c tar.h tcputil.c tcputil.h undelfs.c \
undelfs.h util-alone.c util-alone.h utilvfs.c vfs.c vfs.h

libvfs_la_LDFLAGS = -version-info 27:0:27
