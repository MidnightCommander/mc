SUBDIRS = helpers
DIST_SUBDIRS = helpers

AM_CPPFLAGS = \
	-DLIBEXECDIR=\""$(libexecdir)/@PACKAGE@/"\" \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir)

noinst_LTLIBRARIES = libvfs-fish.la

libvfs_fish_la_SOURCES = \
	fish.c fish.h \
	fishdef.h
