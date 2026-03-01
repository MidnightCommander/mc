panelplugindir = $(panel_plugins_dir)
panelplugin_LTLIBRARIES = mc-panel-git.la
git_helpdir = $(pkgdatadir)/help
git_help_DATA = git-panel.hlp

mc_panel_git_la_SOURCES = git.c
mc_panel_git_la_CPPFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir)
mc_panel_git_la_LDFLAGS = -module -avoid-version
mc_panel_git_la_LIBADD = $(GLIB_LIBS)
