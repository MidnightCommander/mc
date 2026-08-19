PACKAGE_STRING = "/src/vfs/ftpfs"

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-DTEST_DATA_DIR=\"$(abs_srcdir)/data\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib/vfs \
	@CHECK_CFLAGS@

LDADD = \
	$(top_builddir)/src/libinternal.la \
	$(top_builddir)/lib/libmc.la \
	@CHECK_LIBS@

EXTRA_DIST = \
	data/aix_list.input \
	data/aix_list.output \
	data/ms_list.input \
	data/ms_list.output

TESTS = \
	ftpfs_parse_long_list

check_PROGRAMS = $(TESTS)

ftpfs_parse_long_list_SOURCES = \
	ftpfs_parse_long_list.c
