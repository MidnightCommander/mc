SUBDIRS = helpers
DIST_SUBDIRS = helpers

AM_CFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir)
AM_CPPFLAGS = -DLIBEXECDIR=\""$(libexecdir)/@PACKAGE@/"\"

noinst_LTLIBRARIES = libvfs-fish.la

libvfs_fish_la_SOURCES = \
	fish.c fish.h \
	fishdef.h
