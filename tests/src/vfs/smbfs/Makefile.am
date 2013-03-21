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
	dir__smbfs_opendir \
	dir__smbfs_readdir \
	internal__smbfs_make_url \
	internal__smbfs_strerror \
	stat__smbfs_lstat

check_PROGRAMS = $(TESTS)

dir__smbfs_opendir_SOURCES = \
	dir__smbfs_opendir.c

dir__smbfs_readdir_SOURCES = \
	dir__smbfs_readdir.c


dir__smbfs_strerror_SOURCES = \
	internal__smbfs_strerror.c

internal__smbfs_make_url_SOURCES = \
	internal__smbfs_make_url.c

stat__smbfs_lstat_SOURCES = \
	stat__smbfs_lstat.c
