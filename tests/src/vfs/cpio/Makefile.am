PACKAGE_STRING = "/src/vfs/cpio"

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	@CHECK_CFLAGS@

LDADD = \
	$(top_builddir)/src/libinternal.la \
	$(top_builddir)/lib/libmc.la \
	@CHECK_LIBS@

TESTS = \
	cpio_open_archive

check_PROGRAMS = $(TESTS)

cpio_open_archive_SOURCES = \
	cpio_open_archive.c
