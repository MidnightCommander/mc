## Process this file with automake to produce Makefile.in.

## FIXME: this is not nearly finished.  It is pointless to try to use
## it without fixing it, and libvfs, first.  Feel free!


## This file is used only by the Gnome libvfs build.  If you make
## changes here, they must be synchronized with Make-mc.in.

lib_LTLIBRARIES = libvfs.la

BUILT_SOURCES = mad.c mad.h

libvfsincludedir=$(includedir)/vfs

libvfsinclude_HEADERS=vfs.h

EXTRA_DIST = mc-src/mad.c mc-src/mad.h

mad.c: mc-src/mad.c
	$(LN_S) $(srcdir)/mc-src/mad.c mad.c

mad.h: mc-src/mad.h
	$(LN_S) $(srcdir)/mc-src/mad.h mad.h

libvfs_la_SOURCES = \
	container.c 		\
	direntry.c		\
	extfs.c 		\
	fish.c 			\
	ftpfs.c 		\
	local.c 		\
	mcfs.c			\
	mcserv.c 		\
	names.c 		\
	sfs.c 			\
	shared_ftp_fish.c	\
	shared_tar_ext.c 	\
	tar.c 			\
	tcputil.c		\
	undelfs.c 		\
	utilvfs.c		\
	utilvfs.h		\
	vfs.c			\
	container.h 		\
	extfs.h 		\
	fish.h			\
	ftpfs.h 		\
	mcfs.h 			\
	names.h 		\
	tar.h 			\
	tcputil.h 		\
	vfs.h			\
	xdirentry.h

libvfs_la_LDFLAGS = -version-info 27:0:27

SUBDIRS = extfs
