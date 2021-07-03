PACKAGE_STRING = "/src/subshell"

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	@CHECK_CFLAGS@ \
	@PCRE_CPPFLAGS@

AM_LDFLAGS = @TESTS_LDFLAGS@

LIBS = @CHECK_LIBS@ \
	$(top_builddir)/src/libinternal.la \
	$(top_builddir)/lib/libmc.la \
	@PCRE_LIBS@

if ENABLE_MCLIB
LIBS += $(GLIB_LIBS)
endif

TESTS = \
	parse_subshell_prompt_string

check_PROGRAMS = $(TESTS)

parse_subshell_prompt_string_SOURCES = \
	parse_subshell_prompt_string.c
