
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


TESTS = \
	do_cd_command \
	examine_cd \
	exec_get_export_variables_ext

check_PROGRAMS = $(TESTS)

do_cd_command_SOURCES = \
	do_cd_command.c

examine_cd_SOURCES = \
	examine_cd.c

exec_get_export_variables_ext_SOURCES = \
	exec_get_export_variables_ext.c
