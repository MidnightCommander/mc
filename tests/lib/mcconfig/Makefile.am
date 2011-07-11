AM_CFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir) @CHECK_CFLAGS@ \
	-DWORKDIR=\"$(abs_builddir)\"

LIBS=@CHECK_LIBS@  $(top_builddir)/lib/libmc.la

TESTS = \
	config_string

check_PROGRAMS = $(TESTS)

config_string_SOURCES = \
	config_string.c
