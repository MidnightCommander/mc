SUBDIRS = helpers
DIST_SUBDIRS = helpers

AM_CFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir)
AM_CPPFLAGS = -DLIBEXECDIR=\""$(libexecdir)/@PACKAGE@/"\"

noinst_LTLIBRARIES = libvfs-extfs.la

libvfs_extfs_la_SOURCES = \
	extfs.c extfs.h
