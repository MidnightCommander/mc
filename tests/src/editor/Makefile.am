PACKAGE_STRING = "/src/editor"

AM_CPPFLAGS = \
	-DTEST_SHARE_DIR=\"$(abs_srcdir)/../fixtures\" \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	@CHECK_CFLAGS@

LDADD = \
	$(top_builddir)/src/libinternal.la \
	$(top_builddir)/lib/libmc.la \
	@CHECK_LIBS@

EXTRA_DIST = edit_complete_word_cmd_test_data.txt.in

TESTS = \
	edit_complete_word_cmd \
	edit_replace_cmd

check_PROGRAMS = $(TESTS)

edit_complete_word_cmd_SOURCES = \
	edit_complete_word_cmd.c

edit_replace_cmd_SOURCES = \
	edit_replace_cmd.c

