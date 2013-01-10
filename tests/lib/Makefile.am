SUBDIRS = . mcconfig search strutil vfs widget

AM_CPPFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir) @CHECK_CFLAGS@

AM_LDFLAGS = @TESTS_LDFLAGS@

LIBS = @CHECK_LIBS@ $(top_builddir)/lib/libmc.la

EXTRA_DIST = utilunix__my_system-common.c

TESTS = \
	library_independ \
	mc_build_filename \
	name_quote \
	serialize \
	utilunix__my_system_fork_fail \
	utilunix__my_system_fork_child_shell \
	utilunix__my_system_fork_child \
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

utilunix__my_system_fork_fail_SOURCES = \
	utilunix__my_system-fork_fail.c

utilunix__my_system_fork_child_shell_SOURCES = \
	utilunix__my_system-fork_child_shell.c

utilunix__my_system_fork_child_SOURCES = \
	utilunix__my_system-fork_child.c

x_basename_SOURCES = \
	x_basename.c

