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

#include <stdio.h>   // snprintf()
#include <unistd.h>  // unlink(), rmdir()

#include "lib/strutil.h"
#include "src/vfs/local/local.h"

#include "src/vfs/tar/tar.c"

/* --------------------------------------------------------------------------------------------- */

// real size of the sparse file, much larger than the whole archive
#define SPARSE_SIZE (1024 * 1024)
// the last data chunk of the sparse file
#define CHUNK_OFFSET 2048
#define CHUNK_DATA   "hello\n"

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

/* Append data padded with zeros to the block boundary. */
static void
append_data (GByteArray *archive, const char *data, size_t len)
{
    g_byte_array_append (archive, (const guint8 *) data, len);
    while (archive->len % BLOCKSIZE != 0)
        g_byte_array_append (archive, (const guint8 *) "", 1);
}

/* --------------------------------------------------------------------------------------------- */

/* Append an ustar header followed by its data. */
static void
append_member (GByteArray *archive, const char *name, char typeflag, const char *data, size_t len)
{
    union block b;
    unsigned int sum = 0;
    size_t i;

    memset (&b, 0, sizeof (b));
    g_strlcpy (b.header.name, name, sizeof (b.header.name));
    strcpy (b.header.mode, "0000644");
    strcpy (b.header.uid, "0000000");
    strcpy (b.header.gid, "0000000");
    snprintf (b.header.size, sizeof (b.header.size), "%011o", (unsigned int) len);
    strcpy (b.header.mtime, "00000000000");
    b.header.typeflag = typeflag;
    memcpy (b.header.magic, TMAGIC, sizeof (b.header.magic));
    memcpy (b.header.version, "00", sizeof (b.header.version));

    memset (b.header.chksum, ' ', sizeof (b.header.chksum));
    for (i = 0; i < sizeof (b); i++)
        sum += (unsigned char) b.buffer[i];
    snprintf (b.header.chksum, sizeof (b.header.chksum), "%06o", sum);

    g_byte_array_append (archive, (const guint8 *) b.buffer, sizeof (b));
    append_data (archive, data, len);
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_tar_pax_sparse_ds") */
static const struct test_tar_pax_sparse_ds
{
    const char *xheader;
    const char *map;   // sparse map stored in the data, if any
    size_t head_len;   // size of a chunk at offset 0 before the last chunk, if any
    gboolean bad_map;  // the map is malformed: check the listing only
} test_tar_pax_sparse_ds[] = {
    // GNU tar --sparse-version=0.0: each chunk is a pair of keywords in the extended header
    {
        "27 GNU.sparse.size=1048576\n"
        "26 GNU.sparse.numblocks=1\n"
        "26 GNU.sparse.offset=2048\n"
        "25 GNU.sparse.numbytes=6\n"
        "15 path=sparse\n",
        NULL,
        0,
    },
    // GNU tar --sparse-version=0.1: the map is in the extended header
    {
        "27 GNU.sparse.size=1048576\n"
        "26 GNU.sparse.numblocks=1\n"
        "25 GNU.sparse.map=2048,6\n"
        "15 path=sparse\n",
        NULL,
        0,
    },
    // GNU tar and bsdtar default (1.0): the map precedes the chunk in the data
    {
        "22 GNU.sparse.major=1\n"
        "22 GNU.sparse.minor=0\n"
        "26 GNU.sparse.name=sparse\n"
        "31 GNU.sparse.realsize=1048576\n",
        "1\n2048\n6\n",
        0,
    },
    // 1.0 with a chunk that doesn't fill its last block, which is padded in the archive
    {
        "22 GNU.sparse.major=1\n"
        "22 GNU.sparse.minor=0\n"
        "26 GNU.sparse.name=sparse\n"
        "31 GNU.sparse.realsize=1048576\n",
        "2\n0\n1000\n2048\n6\n",
        1000,
    },
    // 0.0 with a chunk count that doesn't fit into memory
    {
        "27 GNU.sparse.size=1048576\n"
        "35 GNU.sparse.numblocks=4294967295\n"
        "26 GNU.sparse.offset=2048\n"
        "25 GNU.sparse.numbytes=6\n"
        "15 path=sparse\n",
        NULL,
        0,
    },
    // 1.0 with a chunk count that doesn't fit into memory
    {
        "22 GNU.sparse.major=1\n"
        "22 GNU.sparse.minor=0\n"
        "26 GNU.sparse.name=sparse\n"
        "31 GNU.sparse.realsize=1048576\n",
        "4294967295\n2048\n6\n",
        0,
        TRUE,
    },
};

/* @Test(dataSource = "test_tar_pax_sparse_ds") */
START_PARAMETRIZED_TEST (test_tar_pax_sparse, test_tar_pax_sparse_ds)
{
    static const guint8 zero_blocks[2 * BLOCKSIZE] = { 0 };
    GByteArray *archive, *content;
    char *dir, *path, *archive_path, *joined;
    vfs_path_t *vpath, *sparse_vpath;
    DIR *d;
    struct vfs_dirent *de;
    GString *listing;
    char *expected, *actual;
    ssize_t n, total;
    int fd;

    // given: a pax sparse file between two plain ustar files, as written by bsdtar
    content = g_byte_array_new ();
    if (data->map != NULL)
        append_data (content, data->map, strlen (data->map));
    if (data->head_len != 0)
    {
        char *head = g_strnfill (data->head_len, 'x');

        append_data (content, head, data->head_len);
        g_free (head);
    }
    g_byte_array_append (content, (const guint8 *) CHUNK_DATA, sizeof (CHUNK_DATA) - 1);

    archive = g_byte_array_new ();
    append_member (archive, "a", REGTYPE, "hello\n", 6);
    append_member (archive, "PaxHeader/sparse", XHDTYPE, data->xheader, strlen (data->xheader));
    append_member (archive, "GNUSparseFile.0/sparse", REGTYPE, (const char *) content->data,
                   content->len);
    append_member (archive, "z", REGTYPE, "world\n", 6);
    g_byte_array_append (archive, zero_blocks, sizeof (zero_blocks));

    dir = g_dir_make_tmp ("mctest-XXXXXX", NULL);
    mctest_assert_not_null (dir);
    path = g_build_filename (dir, "test.tar", (char *) NULL);
    mctest_assert_true (
        g_file_set_contents (path, (const char *) archive->data, (gssize) archive->len, NULL));
    archive_path = g_strconcat (path, PATH_SEP_STR "utar" VFS_PATH_URL_DELIMITER, (char *) NULL);
    vpath = vfs_path_from_str (archive_path);

    // when
    listing = g_string_new (NULL);
    d = mc_opendir (vpath);
    mctest_assert_not_null (d);
    while ((de = mc_readdir (d)) != NULL)
        g_string_append_printf (listing, "%s ", de->d_name);
    mc_closedir (d);

    expected = g_malloc0 (SPARSE_SIZE);
    memset (expected, 'x', data->head_len);
    memcpy (expected + CHUNK_OFFSET, CHUNK_DATA, sizeof (CHUNK_DATA) - 1);
    actual = g_malloc0 (SPARSE_SIZE + 1);
    sparse_vpath = vfs_path_append_new (vpath, "sparse", (char *) NULL);
    fd = mc_open (sparse_vpath, O_RDONLY);
    mctest_assert_true (fd != -1);
    for (total = 0; (n = mc_read (fd, actual + total, SPARSE_SIZE + 1 - total)) > 0; total += n)
        ;
    mc_close (fd);

    // then: all members are listed and the sparse file is expanded correctly
    joined = g_string_free (listing, FALSE);
    mctest_assert_null (message__text);
    mctest_assert_str_eq (joined, "a sparse z ");
    ck_assert_int_eq (total, SPARSE_SIZE);
    if (!data->bad_map)
        mctest_assert_true (memcmp (actual, expected, SPARSE_SIZE) == 0);

    // cleanup
    g_free (joined);
    g_free (actual);
    g_free (expected);
    vfs_path_free (sparse_vpath, TRUE);
    vfs_path_free (vpath, TRUE);
    g_free (archive_path);
    unlink (path);
    g_free (path);
    rmdir (dir);
    g_free (dir);
    g_byte_array_free (archive, TRUE);
    g_byte_array_free (content, TRUE);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    // Add new tests here: ***************
    mctest_add_parameterized_test (tc_core, test_tar_pax_sparse, test_tar_pax_sparse_ds);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
