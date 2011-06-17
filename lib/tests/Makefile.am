SUBDIRS = . mcconfig vfs

AM_CFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir) @CHECK_CFLAGS@
LIBS=@CHECK_LIBS@  $(top_builddir)/lib/libmc.la

TESTS = \
	library_independ \
	serialize \
	x_basename

check_PROGRAMS = $(TESTS)

library_independ_SOURCES = \
	library_independ.c

serialize_SOURCES = \
	serialize.c

x_basename_SOURCES = \
	x_basename.c
