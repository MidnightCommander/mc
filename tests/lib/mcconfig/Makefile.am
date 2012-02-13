AM_CFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir) @CHECK_CFLAGS@ \
	-DWORKDIR=\"$(abs_builddir)\"

AM_LDFLAGS = -Wl,-z,muldefs

LIBS=@CHECK_LIBS@  $(top_builddir)/lib/libmc.la

TESTS = \
	config_string \
	user_configs_path

check_PROGRAMS = $(TESTS)

config_string_SOURCES = \
	config_string.c

user_configs_path_SOURCES = \
	user_configs_path.c
