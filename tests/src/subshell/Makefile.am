PACKAGE_STRING = "/src/subshell"

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib/vfs \
	@CHECK_CFLAGS@

LIBS = @CHECK_LIBS@ \
	$(top_builddir)/src/libinternal.la \
	$(top_builddir)/lib/libmc.la

if ENABLE_MCLIB
LIBS += $(GLIB_LIBS)
endif

EXTRA_DIST = \
	subshell__common.c

TESTS = \
	subshell__peek_subshell_switch_key \
	subshell__async_init

check_PROGRAMS = $(TESTS)

subshell__peek_subshell_switch_key_SOURCES = \
	subshell__peek_subshell_switch_key.c

subshell__async_init_SOURCES = \
	subshell__async_init.c
