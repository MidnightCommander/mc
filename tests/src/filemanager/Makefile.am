PACKAGE_STRING = "/src/filemanager"

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib/vfs \
	-DTEST_SHARE_DIR=\"$(abs_srcdir)\" \
	@CHECK_CFLAGS@

AM_LDFLAGS = @TESTS_LDFLAGS@

LIBS = @CHECK_LIBS@ \
	$(top_builddir)/src/libinternal.la \
	$(top_builddir)/lib/libmc.la

if ENABLE_MCLIB
LIBS += $(GLIB_LIBS)
endif

EXTRA_DIST = hints/mc.hint

TESTS = \
	cd_to \
	examine_cd \
	exec_get_export_variables_ext \
	filegui_is_wildcarded \
	get_random_hint

check_PROGRAMS = $(TESTS)

cd_to_SOURCES = \
	cd_to.c

examine_cd_SOURCES = \
	examine_cd.c

exec_get_export_variables_ext_SOURCES = \
	exec_get_export_variables_ext.c

get_random_hint_SOURCES = \
	get_random_hint.c

filegui_is_wildcarded_SOURCES = \
	filegui_is_wildcarded.c
