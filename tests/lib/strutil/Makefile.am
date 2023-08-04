PACKAGE_STRING = "/lib/strutil"

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	@CHECK_CFLAGS@

LIBS = @CHECK_LIBS@ \
	$(top_builddir)/lib/libmc.la

if ENABLE_MCLIB
LIBS += $(GLIB_LIBS)
endif

TESTS = \
	parse_integer \
	str_replace_all \
	str_verscmp \
	filevercmp

check_PROGRAMS = $(TESTS)

parse_integer_SOURCES = \
	parse_integer.c

str_replace_all_SOURCES = \
	str_replace_all.c

str_verscmp_SOURCES = \
	str_verscmp.c

filevercmp_SOURCES = \
	filevercmp.c
