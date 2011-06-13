AM_CFLAGS = -I$(top_srcdir)/lib/vfs $(GLIB_CFLAGS) -I$(top_srcdir) @CHECK_CFLAGS@ \
	-DTEST_SHARE_DIR=\"$(abs_srcdir)\" -z muldefs

AM_LDFLAGS = -z muldefs

LIBS=@CHECK_LIBS@  \
    $(top_builddir)/lib/libmc.la

TESTS = \
	current_dir \
	path_serialize \
	vfs_path_string_convert \
	vfs_prefix_to_class \
	vfs_split \
	vfs_s_get_path_mangle

check_PROGRAMS = $(TESTS)

current_dir_SOURCES = \
	current_dir.c

path_serialize_SOURCES = \
	path_serialize.c

vfs_split_SOURCES = \
	vfs_split.c

vfs_prefix_to_class_SOURCES = \
	vfs_prefix_to_class.c

vfs_path_string_convert_SOURCES = \
	vfs_path_string_convert.c

vfs_s_get_path_mangle_SOURCES = \
	vfs_s_get_path_mangle.c
