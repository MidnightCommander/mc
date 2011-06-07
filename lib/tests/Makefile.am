SUBDIRS = . mcconfig vfs

AM_CFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir) @CHECK_CFLAGS@
LIBS=@CHECK_LIBS@  $(top_builddir)/lib/libmc.la

TESTS = \
	canonicalize_pathname \
	library_independ \
	serialize

check_PROGRAMS = $(TESTS)

canonicalize_pathname_SOURCES = \
	canonicalize_pathname.c

library_independ_SOURCES = \
	library_independ.c

serialize_SOURCES = \
	serialize.c
