PACKAGE_STRING = "/src/vfs/local"

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	@CHECK_CFLAGS@

LDADD = \
	$(top_builddir)/src/libinternal.la \
	$(top_builddir)/lib/libmc.la \
	@CHECK_LIBS@

TESTS = \
	local_opendir

check_PROGRAMS = $(TESTS)

local_opendir_SOURCES = \
	local_opendir.c
