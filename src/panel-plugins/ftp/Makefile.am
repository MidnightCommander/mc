panelplugindir = $(panel_plugins_dir)
panelplugin_LTLIBRARIES = mc-panel-ftp.la

mc_panel_ftp_la_SOURCES = ftp.c
mc_panel_ftp_la_CPPFLAGS = $(GLIB_CFLAGS) $(LIBCURL_CFLAGS) -I$(top_srcdir)
mc_panel_ftp_la_LDFLAGS = -module -avoid-version
mc_panel_ftp_la_LIBADD = $(GLIB_LIBS) $(LIBCURL_LIBS)
