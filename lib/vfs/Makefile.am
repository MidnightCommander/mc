noinst_LTLIBRARIES = libmcvfs.la

AM_CPPFLAGS = \
	-DMC_VFS_PLUGINS_DIR=\""$(vfs_plugins_dir)"\" \
	$(GLIB_CFLAGS) $(GMODULE_CFLAGS) -I$(top_srcdir)

libmcvfs_la_SOURCES = \
	direntry.c		\
	gc.c gc.h		\
	interface.c \
	parse_ls_vga.c \
	path.c path.h		\
	plugin-loader.c		\
	vfs.c vfs.h		\
	utilvfs.c utilvfs.h	\
	xdirentry.h

if ENABLE_VFS_NET
libmcvfs_la_SOURCES += netutil.c netutil.h
endif

EXTRA_DIST = README
