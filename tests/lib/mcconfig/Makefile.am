
AM_CPPFLAGS = -DWORKDIR=\"$(abs_builddir)\"
AM_CFLAGS = -I$(top_srcdir) $(GLIB_CFLAGS) @CHECK_CFLAGS@

AM_LDFLAGS = @TESTS_LDFLAGS@

LIBS=@CHECK_LIBS@  $(top_builddir)/lib/libmc.la

TESTS = \
	config_string \
	user_configs_path

check_PROGRAMS = $(TESTS)

config_string_SOURCES = \
	config_string.c

user_configs_path_SOURCES = \
	user_configs_path.c
