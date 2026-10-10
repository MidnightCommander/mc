/*
   src/vfs/cpio - tests for opening of cpio archives

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

#define TEST_SUITE_NAME "/src/vfs/cpio"

#include "tests/mctest.h"

#include <stdio.h>   // snprintf()
#include <unistd.h>  // unlink(), rmdir()

#include "lib/strutil.h"
#include "src/vfs/local/local.h"

#include "src/vfs/cpio/cpio.c"

/* --------------------------------------------------------------------------------------------- */

typedef enum
{
    FORMAT_BIN,
    FORMAT_OLDC,
    FORMAT_NEWC
} archive_format_t;

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
    vfs_init_cpiofs ();
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
pad (GByteArray *archive, guint alignment)
{
    while (archive->len % alignment != 0)
        g_byte_array_append (archive, (const guint8 *) "", 1);
}

/* --------------------------------------------------------------------------------------------- */

/* Append an archive member that declares data_size bytes of data, but carries only data_len. */
static void
append_member (GByteArray *archive, archive_format_t format, const char *name, mode_t mode,
               guint64 data_size, const char *data, size_t data_len)
{
    const size_t name_size = strlen (name) + 1;
    char header[128];

    switch (format)
    {
    case FORMAT_BIN:
    {
        // host byte order, just as cpio_find_head() expects for CPIO_BIN
        const unsigned short h[13] = {
            070707,                                 // magic
            0,                                      // dev
            1,                                      // ino
            (unsigned short) mode,                  // mode
            0,                                      // uid
            0,                                      // gid
            1,                                      // nlink
            0,                                      // rdev
            0,                                      // mtime, high
            0,                                      // mtime, low
            (unsigned short) name_size,             // namesize
            (unsigned short) (data_size >> 16),     // filesize, high
            (unsigned short) (data_size & 0xFFFF),  // filesize, low
        };

        g_byte_array_append (archive, (const guint8 *) h, sizeof (h));
        g_byte_array_append (archive, (const guint8 *) name, name_size);
        pad (archive, 2);
        g_byte_array_append (archive, (const guint8 *) data, data_len);
        pad (archive, 2);
        break;
    }
    case FORMAT_OLDC:
        snprintf (header, sizeof (header),
                  "070707"       // magic
                  "000000"       // dev
                  "000001"       // ino
                  "%06o"         // mode
                  "000000"       // uid
                  "000000"       // gid
                  "000001"       // nlink
                  "000000"       // rdev
                  "00000000000"  // mtime
                  "%06o"         // namesize
                  "%011llo",     // filesize
                  (unsigned int) mode, (unsigned int) name_size, (unsigned long long) data_size);
        g_byte_array_append (archive, (const guint8 *) header, strlen (header));
        g_byte_array_append (archive, (const guint8 *) name, name_size);
        g_byte_array_append (archive, (const guint8 *) data, data_len);
        break;
    case FORMAT_NEWC:
        snprintf (header, sizeof (header),
                  "070701"     // magic
                  "00000001"   // ino
                  "%08X"       // mode
                  "00000000"   // uid
                  "00000000"   // gid
                  "00000001"   // nlink
                  "00000000"   // mtime
                  "%08llX"     // filesize
                  "00000000"   // devmajor
                  "00000000"   // devminor
                  "00000000"   // rdevmajor
                  "00000000"   // rdevminor
                  "%08X"       // namesize
                  "00000000",  // check
                  (unsigned int) mode, (unsigned long long) data_size, (unsigned int) name_size);
        g_byte_array_append (archive, (const guint8 *) header, strlen (header));
        g_byte_array_append (archive, (const guint8 *) name, name_size);
        pad (archive, 4);
        g_byte_array_append (archive, (const guint8 *) data, data_len);
        pad (archive, 4);
        break;
    default:
        g_assert_not_reached ();
    }
}

/* --------------------------------------------------------------------------------------------- */

/* Write the archive into a temporary directory and list it through the cpio VFS. */
static char **
list_archive (GByteArray *archive, const char *link_name, char *link_target, size_t target_size)
{
    static const guint8 zero_block[512] = { 0 };
    char *dir;
    char *path;
    char *archive_path;
    vfs_path_t *vpath;
    DIR *d;
    struct vfs_dirent *de;
    GPtrArray *listing;

    // a zero block after the members, as written by cpio(1)
    g_byte_array_append (archive, zero_block, sizeof (zero_block));

    dir = g_dir_make_tmp ("mctest-XXXXXX", NULL);
    mctest_assert_not_null (dir);
    path = g_build_filename (dir, "test.cpio", (char *) NULL);
    mctest_assert_true (
        g_file_set_contents (path, (const char *) archive->data, (gssize) archive->len, NULL));

    archive_path = g_strconcat (path, PATH_SEP_STR "ucpio" VFS_PATH_URL_DELIMITER, (char *) NULL);
    vpath = vfs_path_from_str (archive_path);
    d = mc_opendir (vpath);
    mctest_assert_not_null (d);

    listing = g_ptr_array_new ();
    while ((de = mc_readdir (d)) != NULL)
        g_ptr_array_add (listing, g_strdup (de->d_name));
    g_ptr_array_add (listing, NULL);
    mc_closedir (d);

    if (link_target != NULL)
    {
        vfs_path_t *link_vpath;
        ssize_t len;

        link_vpath = vfs_path_append_new (vpath, link_name, (char *) NULL);
        len = mc_readlink (link_vpath, link_target, target_size - 1);
        link_target[MAX (len, 0)] = '\0';
        vfs_path_free (link_vpath, TRUE);
    }

    vfs_path_free (vpath, TRUE);
    g_free (archive_path);
    unlink (path);
    g_free (path);
    rmdir (dir);
    g_free (dir);

    return (char **) g_ptr_array_free (listing, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_cpio_symlink_size_ds") */
static const struct test_cpio_symlink_size_ds
{
    archive_format_t format;
    guint64 link_size;
} test_cpio_symlink_size_ds[] = {
    // the sizes that made g_malloc() abort or, with 32-bit size_t, wrap to g_malloc (0) (#5084)
    { FORMAT_BIN, 0xFFFFFFFF },
    { FORMAT_NEWC, 0xFFFFFFFF },
    { FORMAT_OLDC, 077777777777 },
    // longer than any path, but with the data actually present
    { FORMAT_BIN, MC_MAXPATHLEN + 1 },
    { FORMAT_NEWC, MC_MAXPATHLEN + 1 },
    { FORMAT_OLDC, MC_MAXPATHLEN + 1 },
};

/* @Test(dataSource = "test_cpio_symlink_size_ds") */
START_PARAMETRIZED_TEST (test_cpio_symlink_size, test_cpio_symlink_size_ds)
{
    GByteArray *archive;
    char *filler;
    size_t filler_len;
    char **listing;

    // given: a symlink, the target of which is too long, between two files
    archive = g_byte_array_new ();
    filler_len = MC_MAXPATHLEN + 1;
    filler = g_strnfill (filler_len, 'x');
    append_member (archive, data->format, "a", S_IFREG | 0644, 6, "hello\n", 6);
    append_member (archive, data->format, "link", S_IFLNK | 0777, data->link_size, filler,
                   MIN (filler_len, data->link_size));
    append_member (archive, data->format, "z", S_IFREG | 0644, 6, "hello\n", 6);
    append_member (archive, data->format, "TRAILER!!!", 0, 0, "", 0);

    // when
    listing = list_archive (archive, NULL, NULL, 0);

    // then: the archive is reported as corrupted, and the symlink is not added to the tree
    mctest_assert_not_null (message__text);
    mctest_assert_true (g_str_has_prefix (message__text, "Corrupted cpio header"));
    mctest_assert_true (g_strv_contains ((const char *const *) listing, "a"));
    mctest_assert_false (g_strv_contains ((const char *const *) listing, "link"));

    // cleanup
    g_strfreev (listing);
    g_free (filler);
    g_byte_array_free (archive, TRUE);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_cpio_symlink_long_target_ds") */
static const struct test_cpio_symlink_long_target_ds
{
    archive_format_t format;
} test_cpio_symlink_long_target_ds[] = {
    { FORMAT_BIN },
    { FORMAT_NEWC },
    { FORMAT_OLDC },
};

/* @Test(dataSource = "test_cpio_symlink_long_target_ds") */
START_PARAMETRIZED_TEST (test_cpio_symlink_long_target, test_cpio_symlink_long_target_ds)
{
    GByteArray *archive;
    char *target;
    char link_target[MC_MAXPATHLEN + 1];
    char **listing;
    char *joined;

    // given: a symlink with a long target that a Linux file system can hold, between two files
    archive = g_byte_array_new ();
    target = g_strnfill (2000, 'd');
    append_member (archive, data->format, "a", S_IFREG | 0644, 6, "hello\n", 6);
    append_member (archive, data->format, "link", S_IFLNK | 0777, strlen (target), target,
                   strlen (target));
    append_member (archive, data->format, "z", S_IFREG | 0644, 6, "hello\n", 6);
    append_member (archive, data->format, "TRAILER!!!", 0, 0, "", 0);

    // when
    listing = list_archive (archive, "link", link_target, sizeof (link_target));

    // then: all members are listed and the symlink target is read completely
    joined = g_strjoinv (" ", listing);
    mctest_assert_null (message__text);
    mctest_assert_str_eq (joined, "a link z");
    mctest_assert_str_eq (link_target, target);

    // cleanup
    g_free (joined);
    g_strfreev (listing);
    g_free (target);
    g_byte_array_free (archive, TRUE);
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
    mctest_add_parameterized_test (tc_core, test_cpio_symlink_size, test_cpio_symlink_size_ds);
    mctest_add_parameterized_test (tc_core, test_cpio_symlink_long_target,
                                   test_cpio_symlink_long_target_ds);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
