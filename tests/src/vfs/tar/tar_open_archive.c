/*
   src/vfs/tar - tests for opening of tar archives

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Yury V. Zaytsev <yury@shurup.com>, 2026

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define TEST_SUITE_NAME "/src/vfs/tar"

#include "tests/mctest.h"

#include <fcntl.h>   // O_RDONLY
#include <unistd.h>  // unlink(), rmdir()

#include "lib/global.h"
#include "lib/strutil.h"
#include "lib/util.h"  // MC_PTR_FREE()
#include "lib/vfs/vfs.h"
#include "lib/widget.h"  // message()
#include "src/vfs/local/local.h"
#include "src/vfs/tar/tar.h"

/* --------------------------------------------------------------------------------------------- */

#define BLOCKSIZE 512

/* --------------------------------------------------------------------------------------------- */

struct pax_record
{
    const char *keyword;
    const char *value;
    size_t value_len;  // 0: use strlen (value)
};

/* --------------------------------------------------------------------------------------------- */
/* mocked functions */
/* --------------------------------------------------------------------------------------------- */

// text of the last error message, if any
static char *message__text = NULL;

/* @Mock */
void
message (int flags, const char *title, const char *text, ...)
{
    va_list ap;

    (void) flags;
    (void) title;

    g_free (message__text);
    va_start (ap, text);
    message__text = g_strdup_vprintf (text, ap);
    va_end (ap);
}

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    str_init_strings (NULL);

    vfs_init ();
    vfs_init_localfs ();
    vfs_init_tarfs ();
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    vfs_shut ();
    str_uninit_strings ();

    MC_PTR_FREE (message__text);
}

/* --------------------------------------------------------------------------------------------- */

static void
pad (GByteArray *archive)
{
    while (archive->len % BLOCKSIZE != 0)
        g_byte_array_append (archive, (const guint8 *) "", 1);
}

/* --------------------------------------------------------------------------------------------- */

/* Append a ustar header that declares size bytes of data. */
static void
append_header (GByteArray *archive, const char *name, char typeflag, guint64 size,
               const char *linkname)
{
    char h[BLOCKSIZE];
    unsigned int sum = 0;
    size_t i;

    memset (h, 0, sizeof (h));
    strncpy (h, name, 99);
    g_snprintf (h + 100, 8, "%07o", typeflag == '5' ? 0755U : 0644U);  // mode
    g_snprintf (h + 108, 8, "%07o", 0U);                               // uid
    g_snprintf (h + 116, 8, "%07o", 0U);                               // gid
    g_snprintf (h + 124, 12, "%011llo", (unsigned long long) size);    // size
    g_snprintf (h + 136, 12, "%011o", 0U);                             // mtime
    memset (h + 148, ' ', 8);                                          // chksum
    h[156] = typeflag;
    if (linkname != NULL)
        strncpy (h + 157, linkname, 99);
    memcpy (h + 257, "ustar", 6);         // magic
    memcpy (h + 263, "00", 2);            // version
    g_snprintf (h + 329, 8, "%07o", 0U);  // devmajor
    g_snprintf (h + 337, 8, "%07o", 0U);  // devminor

    for (i = 0; i < sizeof (h); i++)
        sum += (unsigned char) h[i];
    g_snprintf (h + 148, 7, "%06o", sum);
    h[155] = ' ';

    g_byte_array_append (archive, (const guint8 *) h, sizeof (h));
}

/* --------------------------------------------------------------------------------------------- */

/* Append a regular file with its data. */
static void
append_file (GByteArray *archive, const char *name, const char *data)
{
    append_header (archive, name, '0', strlen (data), NULL);
    g_byte_array_append (archive, (const guint8 *) data, strlen (data));
    pad (archive);
}

/* --------------------------------------------------------------------------------------------- */

/* Append a pax extended header ('x') with records "keyword=value". Values can be binary. */
static void
append_pax_header (GByteArray *archive, const struct pax_record *records)
{
    GByteArray *data;

    data = g_byte_array_new ();

    for (const struct pax_record *r = records; r->keyword != NULL; r++)
    {
        const size_t value_len = r->value_len != 0 ? r->value_len : strlen (r->value);
        // the record length includes the length field itself
        const size_t len = 1 + strlen (r->keyword) + 1 + value_len + 1;
        char num[32];
        size_t digits;

        for (digits = 1;; digits++)
        {
            g_snprintf (num, sizeof (num), "%zu", len + digits);
            if (strlen (num) == digits)
                break;
        }

        g_byte_array_append (data, (const guint8 *) num, digits);
        g_byte_array_append (data, (const guint8 *) " ", 1);
        g_byte_array_append (data, (const guint8 *) r->keyword, strlen (r->keyword));
        g_byte_array_append (data, (const guint8 *) "=", 1);
        g_byte_array_append (data, (const guint8 *) r->value, value_len);
        g_byte_array_append (data, (const guint8 *) "\n", 1);
    }

    append_header (archive, "PaxHeaders/x", 'x', data->len, NULL);
    g_byte_array_append (archive, data->data, data->len);
    pad (archive);

    g_byte_array_free (data, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

/* Write the archive into a temporary directory and return its path. */
static char *
write_archive (GByteArray *archive)
{
    static const guint8 zero_block[BLOCKSIZE] = { 0 };
    char *dir;
    char *path;

    // two zero blocks after the members, as written by tar(1)
    g_byte_array_append (archive, zero_block, sizeof (zero_block));
    g_byte_array_append (archive, zero_block, sizeof (zero_block));

    dir = g_dir_make_tmp ("mctest-XXXXXX", NULL);
    mctest_assert_not_null (dir);
    path = g_build_filename (dir, "test.tar", (char *) NULL);
    g_free (dir);
    mctest_assert_true (
        g_file_set_contents (path, (const char *) archive->data, (gssize) archive->len, NULL));

    return path;
}

/* --------------------------------------------------------------------------------------------- */

static void
remove_archive (char *path)
{
    char *dir;

    dir = g_path_get_dirname (path);
    unlink (path);
    rmdir (dir);
    g_free (dir);
    g_free (path);
}

/* --------------------------------------------------------------------------------------------- */

static vfs_path_t *
archive_vpath (const char *path, const char *member)
{
    char *s;
    vfs_path_t *vpath;

    s = g_strconcat (path, PATH_SEP_STR "utar" VFS_PATH_URL_DELIMITER, member, (char *) NULL);
    vpath = vfs_path_from_str (s);
    g_free (s);

    return vpath;
}

/* --------------------------------------------------------------------------------------------- */

/* List a directory of the archive through the tar VFS. Return NULL if it cannot be opened. */
static char **
list_dir (const char *path, const char *dir)
{
    vfs_path_t *vpath;
    DIR *d;
    struct vfs_dirent *de;
    GPtrArray *listing;

    vpath = archive_vpath (path, dir);
    d = mc_opendir (vpath);
    vfs_path_free (vpath, TRUE);
    if (d == NULL)
        return NULL;

    listing = g_ptr_array_new ();
    while ((de = mc_readdir (d)) != NULL)
        if (strcmp (de->d_name, ".") != 0 && strcmp (de->d_name, "..") != 0)
            g_ptr_array_add (listing, g_strdup (de->d_name));
    g_ptr_array_add (listing, NULL);
    mc_closedir (d);

    return (char **) g_ptr_array_free (listing, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/* Read the contents of a member of the archive through the tar VFS. */
static char *
read_member (const char *path, const char *member)
{
    vfs_path_t *vpath;
    int fd;
    char buf[BLOCKSIZE];
    ssize_t n;

    vpath = archive_vpath (path, member);
    fd = mc_open (vpath, O_RDONLY);
    vfs_path_free (vpath, TRUE);
    mctest_assert_true (fd >= 0);
    n = mc_read (fd, buf, sizeof (buf) - 1);
    mc_close (fd);
    mctest_assert_true (n >= 0);
    buf[n] = '\0';

    return g_strdup (buf);
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_tar_pax_dumpdir_ds") */
static const struct test_tar_pax_dumpdir_ds
{
    const char *dumpdir;
    size_t size;
} test_tar_pax_dumpdir_ds[] = {
    // well-formed, as written by GNU tar --format=posix --listed-incremental
    { "Ya\0Nb\0", 7 },
    { "", 1 },
    // malformed: no terminating NUL bytes
    { "Ya", 2 },
    { "Ya\0", 3 },
};

/* @Test(dataSource = "test_tar_pax_dumpdir_ds") */
START_PARAMETRIZED_TEST (test_tar_pax_dumpdir, test_tar_pax_dumpdir_ds)
{
    GByteArray *archive;
    char *path;
    char **root, **dir;

    const struct pax_record records[] = {
        { "GNU.dumpdir", data->dumpdir, data->size },
        { NULL, NULL, 0 },
    };

    // given: a directory with a GNU.dumpdir record in its pax extended header
    archive = g_byte_array_new ();
    append_pax_header (archive, records);
    append_header (archive, "d/", '5', 0, NULL);
    append_file (archive, "d/a", "hello\n");
    path = write_archive (archive);

    // when
    root = list_dir (path, "");
    dir = list_dir (path, "d");

    // then: the archive can be opened and all members are listed
    mctest_assert_null (message__text);
    mctest_assert_not_null (root);
    mctest_assert_true (g_strv_contains ((const char *const *) root, "d"));
    mctest_assert_not_null (dir);
    mctest_assert_true (g_strv_contains ((const char *const *) dir, "a"));

    // cleanup
    g_strfreev (dir);
    g_strfreev (root);
    remove_archive (path);
    g_byte_array_free (archive, TRUE);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_tar_no_data_size_ds") */
static const struct test_tar_no_data_size_ds
{
    char typeflag;
    const char *linkname;
} test_tar_no_data_size_ds[] = {
    { '2', "a" },   // SYMTYPE
    { '3', NULL },  // CHRTYPE
    { '4', NULL },  // BLKTYPE
    { '6', NULL },  // FIFOTYPE
};

/* @Test(dataSource = "test_tar_no_data_size_ds") */
START_PARAMETRIZED_TEST (test_tar_no_data_size, test_tar_no_data_size_ds)
{
    GByteArray *archive;
    char *path;
    char **listing;
    vfs_path_t *vpath;
    struct stat st;
    char *contents;

    // given: a member that must not have data, with a nonzero size field, between two files
    archive = g_byte_array_new ();
    append_file (archive, "a", "hello\n");
    append_header (archive, "x", data->typeflag, 20000, data->linkname);
    append_file (archive, "z", "world\n");
    path = write_archive (archive);

    // when
    listing = list_dir (path, "");

    // then: the size is ignored, and the following member is neither swallowed nor damaged
    mctest_assert_not_null (listing);
    mctest_assert_true (g_strv_contains ((const char *const *) listing, "a"));
    mctest_assert_true (g_strv_contains ((const char *const *) listing, "x"));
    mctest_assert_true (g_strv_contains ((const char *const *) listing, "z"));

    vpath = archive_vpath (path, "x");
    mctest_assert_true (mc_lstat (vpath, &st) == 0);
    vfs_path_free (vpath, TRUE);
    mctest_assert_true (st.st_size == 0);
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
    mctest_assert_true (st.st_blocks == 0);
#endif

    contents = read_member (path, "z");
    mctest_assert_str_eq (contents, "world\n");

    // cleanup
    g_free (contents);
    g_strfreev (listing);
    remove_archive (path);
    g_byte_array_free (archive, TRUE);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_tar_pax_sparse_0_0)
{
    const struct pax_record records[] = {
        { "GNU.sparse.size", "10", 0 },
        { "GNU.sparse.numblocks", "1", 0 },
        { "GNU.sparse.offset", "5", 0 },
        { "GNU.sparse.numbytes", "5", 0 },
        { NULL, NULL, 0 },
    };
    GByteArray *archive;
    char *path;
    char **listing;

    // given: a sparse file in GNU sparse format 0.0 (GNU tar --sparse-version=0.0)
    archive = g_byte_array_new ();
    append_pax_header (archive, records);
    append_file (archive, "sp", "xxxxx");
    path = write_archive (archive);

    // when
    listing = list_dir (path, "");

    // then: the archive is listed (and mc doesn't crash)
    mctest_assert_not_null (listing);
    mctest_assert_true (g_strv_contains ((const char *const *) listing, "sp"));

    // cleanup
    g_strfreev (listing);
    remove_archive (path);
    g_byte_array_free (archive, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    // Add new tests here: ***************
    mctest_add_parameterized_test (tc_core, test_tar_pax_dumpdir, test_tar_pax_dumpdir_ds);
    mctest_add_parameterized_test (tc_core, test_tar_no_data_size, test_tar_no_data_size_ds);
    tcase_add_test (tc_core, test_tar_pax_sparse_0_0);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
