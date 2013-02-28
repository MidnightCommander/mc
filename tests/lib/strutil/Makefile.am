
AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib/search \
	@CHECK_CFLAGS@

LIBS = @CHECK_LIBS@ $(top_builddir)/lib/libmc.la

TESTS = \
	replace__str_replace_all \
	parse_integer

check_PROGRAMS = $(TESTS)

replace__str_replace_all_SOURCES = \
	replace__str_replace_all.c

parse_integer_SOURCES = \
	parse_integer.c
