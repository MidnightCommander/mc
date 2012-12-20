
AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib/vfs \
	@CHECK_CFLAGS@

AM_LDFLAGS = @TESTS_LDFLAGS@

LIBS=@CHECK_LIBS@  \
    $(top_builddir)/lib/libmc.la

TESTS = \
	examine_cd

if HAVE_SUBSHELL_SUPPORT
TESTS +=
	do_cd_command
do_cd_command_SOURCES = \
	do_cd_command.c
endif

examine_cd_SOURCES = \
	examine_cd.c

check_PROGRAMS = $(TESTS)

EXTRA_DIST = do_cd_command_mock_env.c
