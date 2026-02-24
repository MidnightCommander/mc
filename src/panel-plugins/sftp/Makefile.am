panelplugindir = $(panel_plugins_dir)
panelplugin_LTLIBRARIES = mc-panel-sftp.la

mc_panel_sftp_la_SOURCES = sftp.c
mc_panel_sftp_la_CPPFLAGS = $(GLIB_CFLAGS) $(LIBSSH_CFLAGS) -I$(top_srcdir)
mc_panel_sftp_la_LDFLAGS = -module -avoid-version
mc_panel_sftp_la_LIBADD = $(GLIB_LIBS) $(LIBSSH_LIBS)
