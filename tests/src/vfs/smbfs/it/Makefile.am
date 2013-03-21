AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib/vfs \
	@CHECK_CFLAGS@

AM_LDFLAGS = @TESTS_LDFLAGS@

LIBS=@CHECK_LIBS@  \
	$(top_builddir)/src/libinternal.la \
	$(top_builddir)/lib/libmc.la


INTEGRATION_SMBFS = \
	open_dir_test \
	mkdir_rmdir_stat_test

# run as make TESTS='$(INTEGRATION)' check
# see samba.stories(background) for getting requirements to your environment
INTEGRATION = $(INTEGRATION_SMBFS)

noinst_PROGRAMS = $(INTEGRATION)

TESTS =

check_PROGRAMS = $(TESTS)


open_dir_test_SOURCES = \
	open_dir_test.c

mkdir_rmdir_stat_test_SOURCES = \
	mkdir_rmdir_stat_test.c