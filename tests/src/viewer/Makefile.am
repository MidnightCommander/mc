PACKAGE_STRING = "/src/viewer"

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	@CHECK_CFLAGS@

LIBS = @CHECK_LIBS@ \
	$(top_builddir)/src/libinternal.la \
	$(top_builddir)/lib/libmc.la

if ENABLE_MCLIB
LIBS += $(GLIB_LIBS)
endif

TESTS = \
	ansi_parser \
	syntax_cmd

check_PROGRAMS = $(TESTS)

ansi_parser_SOURCES = \
	ansi_parser.c

syntax_cmd_SOURCES = \
	syntax_cmd.c
