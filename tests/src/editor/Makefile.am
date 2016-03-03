PACKAGE_STRING = "/src/editor"

AM_CPPFLAGS = \
	-DTEST_SHARE_DIR=\"$(abs_srcdir)\" \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src/editor \
	@CHECK_CFLAGS@

AM_LDFLAGS = @TESTS_LDFLAGS@

LIBS=@CHECK_LIBS@  \
	$(top_builddir)/src/libinternal.la \
	$(top_builddir)/lib/libmc.la

if ENABLE_VFS_SMB
# this is a hack for linking with own samba library in simple way
LIBS += $(top_builddir)/src/vfs/smbfs/helpers/libsamba.a
endif

EXTRA_DIST = mc.charsets test-data.txt.in \
	common_editor_includes.c

CLEANFILES = detect_linebreaks.log

TESTS = \
	editcmd__edit_complete_word_cmd \
	detect_linebreaks

check_PROGRAMS = $(TESTS)

editcmd__edit_complete_word_cmd_SOURCES = \
	editcmd__edit_complete_word_cmd.c

detect_linebreaks_SOURCES = \
	detect_linebreaks.c
