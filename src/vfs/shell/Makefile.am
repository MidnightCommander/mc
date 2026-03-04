SUBDIRS = helpers
DIST_SUBDIRS = helpers

AM_CPPFLAGS = \
	-DLIBEXECDIR=\""$(pkglibexecdir)/"\" \
	$(GLIB_CFLAGS) \
	$(LIBSSH_CFLAGS) \
	-I$(top_srcdir)

noinst_LTLIBRARIES = libvfs-shell.la

libvfs_shell_la_SOURCES = \
	shell.c shell.h \
	shell_ssh2.c shell_ssh2.h \
	shelldef.h

libvfs_shell_la_LIBADD = $(LIBSSH_LIBS)
