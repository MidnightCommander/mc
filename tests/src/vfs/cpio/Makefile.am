PACKAGE_STRING = "/src/vfs/cpio"

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	@CHECK_CFLAGS@

LIBS = @CHECK_LIBS@ \
	$(top_builddir)/src/libinternal.la \
	$(top_builddir)/lib/libmc.la

if ENABLE_MCLIB
LIBS += $(GLIB_LIBS)
endif

TESTS = \
	cpio_open_archive

check_PROGRAMS = $(TESTS)

cpio_open_archive_SOURCES = \
	cpio_open_archive.c
