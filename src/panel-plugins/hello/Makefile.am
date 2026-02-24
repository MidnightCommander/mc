panelplugindir = $(panel_plugins_dir)
panelplugin_LTLIBRARIES = mc-panel-hello.la

mc_panel_hello_la_SOURCES = hello.c
mc_panel_hello_la_CPPFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir)
mc_panel_hello_la_LDFLAGS = -module -avoid-version
mc_panel_hello_la_LIBADD = $(GLIB_LIBS)
