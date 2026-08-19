PACKAGE_STRING = "/lib/strutil"

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	@CHECK_CFLAGS@

LDADD = \
	$(top_builddir)/lib/libmc.la \
	@CHECK_LIBS@

TESTS = \
	parse_integer \
	str_replace_all \
	str_verscmp \
	str_rstrip_eol \
	filevercmp

check_PROGRAMS = $(TESTS)

parse_integer_SOURCES = \
	parse_integer.c

str_replace_all_SOURCES = \
	str_replace_all.c

str_verscmp_SOURCES = \
	str_verscmp.c

str_rstrip_eol_SOURCES = \
	str_rstrip_eol.c

filevercmp_SOURCES = \
	filevercmp.c
