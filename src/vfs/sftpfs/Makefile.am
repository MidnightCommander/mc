
AM_CPPFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir) $(LIBSSH_CFLAGS)

noinst_LTLIBRARIES = libvfs-sftpfs.la

libvfs_sftpfs_la_SOURCES = \
	config_parser.c \
	connection.c \
	dir.c \
	file.c \
	internal.c internal.h \
	sftpfs.c sftpfs.h
