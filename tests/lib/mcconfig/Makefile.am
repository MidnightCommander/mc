PACKAGE_STRING = "/lib/mcconfig"

AM_CPPFLAGS = \
	-DWORKDIR=\"$(abs_builddir)\" \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	@CHECK_CFLAGS@

LDADD = \
	$(top_builddir)/lib/libmc.la \
	@CHECK_LIBS@

TESTS = \
	config_string \
	user_configs_path

check_PROGRAMS = $(TESTS)

CLEANFILES = config_string.ini

config_string_SOURCES = \
	config_string.c

user_configs_path_SOURCES = \
	user_configs_path.c
