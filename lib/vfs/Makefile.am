noinst_LTLIBRARIES = libmcvfs.la

AM_CPPFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir)

libmcvfs_la_SOURCES = \
	direntry.c		\
	gc.c gc.h		\
	interface.c \
	parse_ls_vga.c \
	path.c path.h		\
	vfs.c vfs.h		\
	utilvfs.c utilvfs.h	\
	xdirentry.h

if ENABLE_VFS_NET
libmcvfs_la_SOURCES += netutil.c netutil.h
endif

EXTRA_DIST = HACKING README
