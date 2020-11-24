PACKAGE_STRING = "/lib/widget"

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib/vfs \
	@CHECK_CFLAGS@

AM_LDFLAGS = @TESTS_LDFLAGS@

LIBS = @CHECK_LIBS@ \
	$(top_builddir)/lib/libmc.la

if ENABLE_MCLIB
LIBS += $(GLIB_LIBS)
endif

TESTS = \
	complete_engine \
	hotkey_equal \
	group_init_destroy \
	widget_find_by_id

check_PROGRAMS = $(TESTS)

complete_engine_SOURCES = \
	complete_engine.c

hotkey_equal_SOURCES = \
	hotkey_equal.c

group_init_destroy_SOURCES = \
	group_init_destroy.c

widget_find_by_id_SOURCES = \
	widget_find_by_id.c
