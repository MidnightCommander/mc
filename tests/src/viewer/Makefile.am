PACKAGE_STRING = "/src/viewer"

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	-DTEST_SHARE_DIR=\"$(abs_srcdir)/../fixtures\" \
	@CHECK_CFLAGS@

LDADD = \
	$(top_builddir)/src/libinternal.la \
	$(top_builddir)/lib/libmc.la \
	@CHECK_LIBS@

TESTS = \
	viewer__text

check_PROGRAMS = $(TESTS)

viewer__text_SOURCES = \
	viewer__text.c
