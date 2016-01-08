PACKAGE_STRING = "/src/editor"

AM_CPPFLAGS = \
	-DTEST_SHARE_DIR=\"$(abs_srcdir)\" \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
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

EXTRA_DIST = mc.charsets test-data.txt.in

TESTS = \
	editcmd__edit_complete_word_cmd

check_PROGRAMS = $(TESTS)

editcmd__edit_complete_word_cmd_SOURCES = \
	editcmd__edit_complete_word_cmd.c

