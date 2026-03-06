panelplugindir = $(panel_plugins_dir)
panelplugin_LTLIBRARIES = mc-panel-arcmc.la

mc_panel_arcmc_la_SOURCES = \
    arcmc.c \
    archive-io.c \
    progress.c \
    dialog-pack.c

mc_panel_arcmc_la_CPPFLAGS = $(GLIB_CFLAGS) $(LIBARCHIVE_CFLAGS) -I$(top_srcdir) -DLIBEXECDIR=\""$(pkglibexecdir)"\" $(LIBMAGIC_CFLAGS)
mc_panel_arcmc_la_LDFLAGS = -module -avoid-version
mc_panel_arcmc_la_LIBADD = $(GLIB_LIBS) $(LIBARCHIVE_LIBS) $(LIBMAGIC_LIBS)
