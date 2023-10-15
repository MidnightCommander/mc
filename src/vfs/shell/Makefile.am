SUBDIRS = helpers
DIST_SUBDIRS = helpers

AM_CPPFLAGS = \
	-DLIBEXECDIR=\""$(libexecdir)/@PACKAGE@/"\" \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir)

noinst_LTLIBRARIES = libvfs-shell.la

libvfs_shell_la_SOURCES = \
	shell.c shell.h \
	shelldef.h
