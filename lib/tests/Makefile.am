SUBDIRS = . mcconfig

AM_CFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir) @CHECK_CFLAGS@
LIBS=@CHECK_LIBS@  $(top_builddir)/lib/libmc.la

TESTS = \
	library_independ

check_PROGRAMS = $(TESTS)

library_independ_SOURCES = \
	library_independ.c
