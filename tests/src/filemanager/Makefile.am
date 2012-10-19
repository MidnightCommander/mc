AM_CFLAGS = -I$(top_srcdir)/lib/vfs $(GLIB_CFLAGS) -I$(top_srcdir) @CHECK_CFLAGS@

AM_LDFLAGS = @TESTS_LDFLAGS@

LIBS=@CHECK_LIBS@  \
	$(top_builddir)/src/libinternal.la \
	$(top_builddir)/lib/libmc.la

if ENABLE_VFS_SMB
# this is a hack for linking with own samba library in simple way
LIBS += $(top_builddir)/src/vfs/smbfs/helpers/libsamba.a
endif


TESTS = \
	do_panel_cd \
	examine_cd

check_PROGRAMS = $(TESTS)

do_panel_cd_SOURCES = \
	do_panel_cd.c

examine_cd_SOURCES = \
	examine_cd.c
