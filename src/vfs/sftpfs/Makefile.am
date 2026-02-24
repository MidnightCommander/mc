
AM_CPPFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir) $(LIBSSH_CFLAGS)

vfsplugindir = $(vfs_plugins_dir)
vfsplugin_LTLIBRARIES = mc-vfs-sftp.la

mc_vfs_sftp_la_SOURCES = \
	config_parser.c \
	connection.c \
	dir.c \
	file.c \
	internal.c internal.h \
	sftpfs.c sftpfs.h

mc_vfs_sftp_la_LDFLAGS = -module -avoid-version
mc_vfs_sftp_la_LIBADD = $(GLIB_LIBS) $(LIBSSH_LIBS)
