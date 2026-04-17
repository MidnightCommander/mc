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

if USE_TREE_SITTER
TESTS += edit_syntax_ts
endif

check_PROGRAMS = $(TESTS)

edit_complete_word_cmd_SOURCES = \
	edit_complete_word_cmd.c

edit_insert_column_of_text_SOURCES = \
	edit_insert_column_of_text.c

edit_replace_cmd_SOURCES = \
	edit_replace_cmd.c

if USE_TREE_SITTER
edit_syntax_ts_SOURCES = \
	edit_syntax_ts.c

edit_syntax_ts_CPPFLAGS = \
	-DTS_GRAMMAR_LIBDIR=\"$(libdir)/mc/ts-grammars\" \
	$(GLIB_CFLAGS) \
	$(TREE_SITTER_CFLAGS) \
	$(GMODULE_CFLAGS) \
	-I$(top_srcdir) \
	@CHECK_CFLAGS@

edit_syntax_ts_LDADD = \
	$(TREE_SITTER_LIBS) \
	$(GMODULE_LIBS) \
	$(GLIB_LIBS) \
	@CHECK_LIBS@
endif
