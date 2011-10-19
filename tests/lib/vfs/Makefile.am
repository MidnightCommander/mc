AM_CFLAGS = -I$(top_srcdir)/lib/vfs $(GLIB_CFLAGS) -I$(top_srcdir) @CHECK_CFLAGS@ \
	-DTEST_SHARE_DIR=\"$(abs_srcdir)\" -z muldefs

AM_LDFLAGS = -z muldefs

EXTRA_DIST = mc.charsets

LIBS=@CHECK_LIBS@  \
    $(top_builddir)/lib/libmc.la

TESTS = \
	canonicalize_pathname \
	current_dir \
	path_recode \
	path_serialize \
	vfs_parse_ls_lga \
	vfs_path_string_convert \
	vfs_prefix_to_class \
	vfs_split \
	vfs_s_get_path

check_PROGRAMS = $(TESTS)

canonicalize_pathname_SOURCES = \
	canonicalize_pathname.c

current_dir_SOURCES = \
	current_dir.c

path_recode_SOURCES = \
	path_recode.c

path_serialize_SOURCES = \
	path_serialize.c

vfs_split_SOURCES = \
	vfs_split.c

vfs_parse_ls_lga_SOURCES = \
	vfs_parse_ls_lga.c

vfs_prefix_to_class_SOURCES = \
	vfs_prefix_to_class.c

vfs_path_string_convert_SOURCES = \
	vfs_path_string_convert.c

vfs_s_get_path_SOURCES = \
	vfs_s_get_path.c
