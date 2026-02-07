PACKAGE_STRING = "/src/editor"

AM_CPPFLAGS = \
	-DTEST_SHARE_DIR=\"$(abs_srcdir)/../fixtures\" \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	@CHECK_CFLAGS@

LIBS = @CHECK_LIBS@ \
	$(top_builddir)/src/libinternal.la \
	$(top_builddir)/lib/libmc.la

if ENABLE_MCLIB
LIBS += $(GLIB_LIBS)
endif

EXTRA_DIST = edit_complete_word_cmd_test_data.txt.in

TESTS = \
	edit_complete_word_cmd \
	edit_insert_column_of_text \
	edit_replace_cmd

check_PROGRAMS = $(TESTS)

edit_complete_word_cmd_SOURCES = \
	edit_complete_word_cmd.c

edit_insert_column_of_text_SOURCES = \
	edit_insert_column_of_text.c

edit_replace_cmd_SOURCES = \
	edit_replace_cmd.c

