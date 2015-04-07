PACKAGE_STRING = "/lib/search"

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib/search \
	@CHECK_CFLAGS@

LIBS = @CHECK_LIBS@ $(top_builddir)/lib/libmc.la

TESTS = \
	glob_prepare_replace_str \
	glob_translate_to_regex \
	regex_get_compile_flags \
	regex_replace_esc_seq \
	regex_process_escape_sequence \
	translate_replace_glob_to_regex

check_PROGRAMS = $(TESTS)

glob_prepare_replace_str_SOURCES = \
	glob_prepare_replace_str.c

regex_replace_esc_seq_SOURCES = \
	regex_replace_esc_seq.c

regex_process_escape_sequence_SOURCES = \
	regex_process_escape_sequence.c

translate_replace_glob_to_regex_SOURCES = \
	translate_replace_glob_to_regex.c

glob_translate_to_regex_SOURCES = \
	glob_translate_to_regex.c

regex_get_compile_flags_SOURCES = \
	regex_get_compile_flags.c
