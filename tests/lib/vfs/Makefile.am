
AM_CPPFLAGS = \
	-DTEST_SHARE_DIR=\"$(abs_srcdir)\" \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib/vfs \
	@CHECK_CFLAGS@

AM_LDFLAGS = @TESTS_LDFLAGS@

EXTRA_DIST = mc.charsets

LIBS=@CHECK_LIBS@  \
    $(top_builddir)/lib/libmc.la

TESTS = \
	canonicalize_pathname \
	current_dir \
	path_cmp \
	path_len \
	path_manipulations \
	path_serialize \
	relative_cd \
	tempdir \
	vfs_parse_ls_lga \
	vfs_path_string_convert \
	vfs_prefix_to_class \
	vfs_split \
	vfs_s_get_path

if CHARSET
TESTS += path_recode
endif

check_PROGRAMS = $(TESTS)

canonicalize_pathname_SOURCES = \
	canonicalize_pathname.c

current_dir_SOURCES = \
	current_dir.c

path_cmp_SOURCES = \
	path_cmp.c

path_len_SOURCES = \
	path_len.c

path_manipulations_SOURCES = \
	path_manipulations.c

path_recode_SOURCES = \
	path_recode.c

path_serialize_SOURCES = \
	path_serialize.c

relative_cd_SOURCES = \
	relative_cd.c

tempdir_SOURCES = \
	tempdir.c

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
