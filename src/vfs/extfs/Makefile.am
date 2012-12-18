SUBDIRS = helpers
DIST_SUBDIRS = helpers

AM_CPPFLAGS = \
	-DLIBEXECDIR=\""$(libexecdir)/@PACKAGE@/"\" \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir)

noinst_LTLIBRARIES = libvfs-extfs.la

libvfs_extfs_la_SOURCES = \
	extfs.c extfs.h
