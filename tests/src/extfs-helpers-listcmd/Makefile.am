PACKAGE_STRING = "/src/extfs-helpers-listcmd"

AM_CPPFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir)

# This lets mc_parse_ls_l.c override MC's message() without the linker
# complaining about multiple definitions.
AM_LDFLAGS = @TESTS_LDFLAGS@

LIBS = $(top_builddir)/lib/libmc.la

# Programs/scripts to build on 'make check'.
check_PROGRAMS = mc_parse_ls_l

# Tests to run on 'make check'
TESTS = run

mc_parse_ls_l_SOURCES = \
	mc_parse_ls_l.c

EXTRA_DIST = run
