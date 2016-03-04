PACKAGE_STRING = "/src/filemanager"

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib/vfs \
	-DTEST_SHARE_DIR=\"$(abs_srcdir)\" \
	@CHECK_CFLAGS@ \
	@PCRE_CPPFLAGS@

AM_LDFLAGS = @TESTS_LDFLAGS@

LIBS=@CHECK_LIBS@  \
	$(top_builddir)/src/libinternal.la \
	$(top_builddir)/lib/libmc.la \
	@PCRE_LIBS@

if ENABLE_VFS_SMB
# this is a hack for linking with own samba library in simple way
LIBS += $(top_builddir)/src/vfs/smbfs/helpers/libsamba.a
endif

EXTRA_DIST = hints/mc.hint

TESTS = \
	do_cd_command \
	examine_cd \
	exec_get_export_variables_ext \
	filegui_is_wildcarded \
	get_random_hint

check_PROGRAMS = $(TESTS)

do_cd_command_SOURCES = \
	do_cd_command.c

examine_cd_SOURCES = \
	examine_cd.c

exec_get_export_variables_ext_SOURCES = \
	exec_get_export_variables_ext.c

get_random_hint_SOURCES = \
	get_random_hint.c

filegui_is_wildcarded_SOURCES = \
	filegui_is_wildcarded.c
