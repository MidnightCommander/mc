AM_CFLAGS = -I$(top_srcdir)/src/editor $(GLIB_CFLAGS) -I$(top_srcdir) @CHECK_CFLAGS@

LIBS=@CHECK_LIBS@  \
    $(top_builddir)/lib/libmc.la \
    $(top_builddir)/src/vfs/libmc-vfs.la

EXTRA_DIST = common_editor_includes.c

CLEANFILES = detect_linebreaks.log

TESTS = \
	detect_linebreaks

check_PROGRAMS = $(TESTS)

detect_linebreaks_SOURCES = \
	detect_linebreaks.c
