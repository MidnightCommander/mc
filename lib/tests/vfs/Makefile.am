AM_CFLAGS = -I$(top_srcdir)/lib/vfs $(GLIB_CFLAGS) -I$(top_srcdir) @CHECK_CFLAGS@
LIBS=@CHECK_LIBS@  \
    $(top_builddir)/lib/libmc.la

TESTS = \
	get_vfs_class

check_PROGRAMS = $(TESTS)

get_vfs_class_SOURCES = \
	get_vfs_class.c
