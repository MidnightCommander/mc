AM_CFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir) @CHECK_CFLAGS@
LIBS=@CHECK_LIBS@  $(top_builddir)/lib/libmc.la

TESTS = \
	examine_cd

check_PROGRAMS = $(TESTS)

examine_cd_SOURCES = \
	examine_cd.c
