PACKAGE_STRING = "/src/panel-plugins/ftp"

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	@CHECK_CFLAGS@

LIBS = @CHECK_LIBS@ \
	$(top_builddir)/lib/libmc.la

if ENABLE_MCLIB
LIBS += $(GLIB_LIBS)
endif

TESTS = \
	ftp_utils

check_PROGRAMS = $(TESTS)

ftp_utils_SOURCES = \
	ftp_utils.c
