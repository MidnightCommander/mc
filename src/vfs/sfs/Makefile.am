
AM_CPPFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir)

noinst_LTLIBRARIES = libvfs-sfs.la

libvfs_sfs_la_SOURCES = \
	sfs.c sfs.h

SFSCONFFILES = sfs.ini

if ENABLE_VFS_SFS
sfsconfdir = $(sysconfdir)/@PACKAGE@
sfsconf_DATA = $(SFSCONFFILES)
endif

EXTRA_DIST = $(SFSCONFFILES)
