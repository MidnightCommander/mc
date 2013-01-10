SUBDIRS = . filemanager

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib/vfs \
	@CHECK_CFLAGS@

AM_LDFLAGS = @TESTS_LDFLAGS@

LIBS=@CHECK_LIBS@  \
	$(top_builddir)/src/libinternal.la \
	$(top_builddir)/lib/libmc.la

if ENABLE_VFS_SMB
# this is a hack for linking with own samba library in simple way
LIBS += $(top_builddir)/src/vfs/smbfs/helpers/libsamba.a
endif

EXTRA_DIST = execute__common.c

TESTS = \
	execute__execute_with_vfs_arg \
	execute__execute_external_editor_or_viewer \
	execute__execute_get_external_cmd_opts_from_config

check_PROGRAMS = $(TESTS)

execute__execute_with_vfs_arg_SOURCES = \
	execute__execute_with_vfs_arg.c


execute__execute_external_editor_or_viewer_SOURCES = \
	execute__execute_external_editor_or_viewer.c


execute__execute_get_external_cmd_opts_from_config_SOURCES = \
	execute__execute_get_external_cmd_opts_from_config.c

