## Process this file with automake to produce Makefile.in.

## FIXME: this is not nearly finished.  It is pointless to try to use
## it without fixing it, and libvfs, first.  Feel free!


## This file is used only by the Gnome libvfs build.  If you make
## changes here, they must be synchronized with Make-mc.in.

lib_LTLIBRARIES = libvfs.la

BUILT_SOURCES = mad.c mad.h

libvfsincludedir=$(includedir)/vfs

libvfsinclude_HEADERS=vfs.h

mad.c: mc-src/mad.c
	$(LN_S) $< $@

mad.h: mc-src/mad.h
	$(LN_S) $< $@

libvfs_la_SOURCES = callback.h container.c container.h extfs.c extfs.h \
ftpfs.c ftpfs.h fish.c fish.h local.c mad.c mad.h mcfs.c mcfs.h mcserv.c \
names.c names.h shared_ftp_fish.c shared_tar_ext.c tar.c tar.h sfs.c \
tcputil.c tcputil.h undelfs.c undelfs.h util-alone.c util-alone.h \
utilvfs.c vfs.c vfs.h

libvfs_la_LDFLAGS = -version-info 27:0:27
