PACKAGE_STRING = "/src/vfs/tar"

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	@CHECK_CFLAGS@

LDADD = \
	$(top_builddir)/src/libinternal.la \
	$(top_builddir)/lib/libmc.la \
	@CHECK_LIBS@

TESTS = \
	tar_open_archive

check_PROGRAMS = $(TESTS)

tar_open_archive_SOURCES = \
	tar_open_archive.c
