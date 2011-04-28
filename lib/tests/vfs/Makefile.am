AM_CFLAGS = -I$(top_srcdir)/lib/vfs $(GLIB_CFLAGS) -I$(top_srcdir) @CHECK_CFLAGS@
LIBS=@CHECK_LIBS@  \
    $(top_builddir)/lib/libmc.la

TESTS = \
	vfs_prefix_to_class \
	get_vfs_class \
	vfs_split \
	vfs_path_string_convert \
	vfs_s_get_path_mangle

check_PROGRAMS = $(TESTS)

get_vfs_class_SOURCES = \
	get_vfs_class.c

vfs_split_SOURCES = \
	vfs_split.c

vfs_prefix_to_class_SOURCES = \
	vfs_prefix_to_class.c

vfs_path_string_convert_SOURCES = \
	vfs_path_string_convert.c

vfs_s_get_path_mangle_SOURCES = \
	vfs_s_get_path_mangle.c