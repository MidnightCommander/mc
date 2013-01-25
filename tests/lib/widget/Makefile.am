
AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib/vfs \
	@CHECK_CFLAGS@

AM_LDFLAGS = @TESTS_LDFLAGS@

LIBS=@CHECK_LIBS@  \
    $(top_builddir)/lib/libmc.la

TESTS = \
	complete_engine

check_PROGRAMS = $(TESTS)

complete_engine_SOURCES = \
	complete_engine.c
