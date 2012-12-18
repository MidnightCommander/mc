
AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib/vfs \
	@CHECK_CFLAGS@

AM_LDFLAGS = @TESTS_LDFLAGS@

LIBS=@CHECK_LIBS@  \
    $(top_builddir)/lib/libmc.la

TESTS = \
	do_cd_command \
	examine_cd

check_PROGRAMS = $(TESTS)

do_cd_command_SOURCES = \
	do_cd_command.c

examine_cd_SOURCES = \
	examine_cd.c

EXTRA_DIST = do_cd_command_mock_env.c
