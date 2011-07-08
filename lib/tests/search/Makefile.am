AM_CFLAGS = -I$(top_srcdir)/lib/search $(GLIB_CFLAGS) -I$(top_srcdir) @CHECK_CFLAGS@
LIBS=@CHECK_LIBS@  $(top_builddir)/lib/libmc.la

TESTS = \
	regex_replace_esc_seq \
	regex_process_escape_sequence

check_PROGRAMS = $(TESTS)

regex_replace_esc_seq_SOURCES = \
	regex_replace_esc_seq.c

regex_process_escape_sequence_SOURCES = \
	regex_process_escape_sequence.c