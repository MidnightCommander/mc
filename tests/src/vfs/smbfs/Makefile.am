SUBDIRS = . it

EXTRA_DIST = samba.stories

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib/vfs \
	@CHECK_CFLAGS@

AM_LDFLAGS = @TESTS_LDFLAGS@

LIBS=@CHECK_LIBS@  \
	$(top_builddir)/src/libinternal.la \
	$(top_builddir)/lib/libmc.la

TESTS = \
	smbfs_dir_open \
	smbfs_dir_read \
	internal__smbfs_make_url \
	internal__smbfs_strerror \
	smbfs_lstat

check_PROGRAMS = $(TESTS)

smbfs_dir_open_SOURCES = \
	smbfs_dir_open.c

smbfs_dir_read_SOURCES = \
	smbfs_dir_read.c

internal__smbfs_strerror_SOURCES = \
	internal__smbfs_strerror.c

internal__smbfs_make_url_SOURCES = \
	internal__smbfs_make_url.c

smbfs_lstat_SOURCES = \
	smbfs_lstat.c
