PACKAGE_STRING = "/lib"

SUBDIRS = . mcconfig search strutil vfs widget

AM_CPPFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir) @CHECK_CFLAGS@

LIBS = @CHECK_LIBS@ \
	$(top_builddir)/lib/libmc.la

if ENABLE_MCLIB
LIBS += $(GLIB_LIBS) \
	@E2P_LIBS@
endif

EXTRA_DIST = utilunix__my_system-common.c

TESTS = \
	library_independ \
	mc_build_filename \
	name_quote \
	serialize \
	tty \
	utilunix__mc_pstream_get_string \
	utilunix__my_system_fork_fail \
	utilunix__my_system_fork_child_shell \
	utilunix__my_system_fork_child \
	x_basename

if CHARSET
TESTS += mc_realpath
endif

check_PROGRAMS = $(TESTS)

library_independ_SOURCES = \
	library_independ.c

mc_build_filename_SOURCES = \
	mc_build_filename.c

mc_realpath_SOURCES = \
	mc_realpath.c

name_quote_SOURCES = \
	name_quote.c

serialize_SOURCES = \
	serialize.c

tty_SOURCES = \
	tty.c

utilunix__mc_pstream_get_string_SOURCES = \
	utilunix__mc_pstream_get_string.c

utilunix__my_system_fork_fail_SOURCES = \
	utilunix__my_system-fork_fail.c

utilunix__my_system_fork_child_shell_SOURCES = \
	utilunix__my_system-fork_child_shell.c

utilunix__my_system_fork_child_SOURCES = \
	utilunix__my_system-fork_child.c

x_basename_SOURCES = \
	x_basename.c

