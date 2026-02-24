panelplugindir = $(panel_plugins_dir)
panelplugin_LTLIBRARIES = mc-panel-docker.la

mc_panel_docker_la_SOURCES = docker.c
mc_panel_docker_la_CPPFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir)
mc_panel_docker_la_LDFLAGS = -module -avoid-version
mc_panel_docker_la_LIBADD = $(GLIB_LIBS)
