AM_CFLAGS = -I$(top_srcdir)/lib/vfs $(GLIB_CFLAGS) -I$(top_srcdir) @CHECK_CFLAGS@

AM_LDFLAGS = @TESTS_LDFLAGS@

LIBS=@CHECK_LIBS@  \
    $(top_builddir)/lib/libmc.la

TESTS = \
	do_panel_cd \
	examine_cd

check_PROGRAMS = $(TESTS)

do_panel_cd_SOURCES = \
	do_panel_cd.c

examine_cd_SOURCES = \
	examine_cd.c

EXTRA_DIST = do_panel_cd_stub_env.c
