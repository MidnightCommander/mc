noinst_LTLIBRARIES = libmcvfs.la

AM_CFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir)

libmcvfs_la_SOURCES = \
	interface.c interface.h \
	vfs.c vfs.h		\
	direntry.c xdirentry.h	\
	utilvfs.c utilvfs.h	\
	gc.c gc.h

if ENABLE_VFS_NET
libmcvfs_la_SOURCES += netutil.c netutil.h
endif

EXTRA_DIST = HACKING README
