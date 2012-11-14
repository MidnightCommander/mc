SUBDIRS = . mcconfig search vfs

AM_CPPFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir) @CHECK_CFLAGS@

LIBS = @CHECK_LIBS@ $(top_builddir)/lib/libmc.la

TESTS = \
	library_independ \
	mc_build_filename \
	name_quote \
	serialize \
	x_basename

check_PROGRAMS = $(TESTS)

library_independ_SOURCES = \
	library_independ.c

mc_build_filename_SOURCES = \
	mc_build_filename.c

name_quote_SOURCES = \
	name_quote.c

serialize_SOURCES = \
	serialize.c

x_basename_SOURCES = \
	x_basename.c
