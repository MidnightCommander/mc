/*
   src/vfs/local - tests for local_opendir() function

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

#define TEST_SUITE_NAME "/src/vfs/local"

#include "tests/mctest.h"

#include <dirent.h>
#include <unistd.h>  // rmdir()

#include "lib/strutil.h"

/* --------------------------------------------------------------------------------------------- */
/* mocked functions */
/* --------------------------------------------------------------------------------------------- */

static struct dirent *mock_readdir (DIR *dirp);

// route the readdir() calls of the local VFS through the mock
#define readdir(dirp) mock_readdir (dirp)
#include "src/vfs/local/local.c"
#undef readdir

// whether to report the end of the directory right away
static gboolean mock_readdir__eof = FALSE;

// entries to consume before failing with EIO, or -1 to pass through
static int mock_readdir__fail_after = -1;

/* @Mock */
static struct dirent *
mock_readdir (DIR *dirp)
{
    if (mock_readdir__eof)
        return NULL;  // like readdir(), leaving errno untouched

    if (mock_readdir__fail_after < 0)
        return readdir (dirp);

    // FreeBSD and macOS keep the advanced offset if reading fails after partial progress
    for (; mock_readdir__fail_after > 0; mock_readdir__fail_after--)
        (void) readdir (dirp);
    mock_readdir__fail_after = -1;

    errno = EIO;
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    str_init_strings (NULL);

    vfs_init ();
    vfs_init_localfs ();
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

static int
count_entries (const vfs_path_t *vpath)
{
    void *info;
    struct vfs_dirent *entry;
    int count = 0;

    info = local_opendir (vpath);
    mctest_assert_not_null (info);

    while ((entry = local_readdir (info)) != NULL)
    {
        vfs_dirent_free (entry);
        count++;
    }

    local_closedir (info);

    return count;
}

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_local_opendir_stale_eintr)
{
    char *dir;
    vfs_path_t *vpath;
    void *info;

    // given: a directory without any entries, not even "." and ".." (as on some FUSE filesystems),
    // and errno left at EINTR by an earlier interrupted syscall (#5156)
    dir = g_dir_make_tmp ("mctest-XXXXXX", NULL);
    mctest_assert_not_null (dir);
    vpath = vfs_path_from_str (dir);
    mock_readdir__eof = TRUE;
    errno = EINTR;

    // when
    info = local_opendir (vpath);

    // then: the directory is opened rather than reopened forever
    mctest_assert_not_null (info);

    // cleanup
    local_closedir (info);
    mock_readdir__eof = FALSE;
    vfs_path_free (vpath, TRUE);
    rmdir (dir);
    g_free (dir);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_local_opendir_readdir_error)
{
    char *dir;
    vfs_path_t *vpath;
    int expected;
    int actual;

    // given: a directory with entries ("." and ".."), the first readdir() of which fails with an
    // error other than EINTR after consuming one of them
    dir = g_dir_make_tmp ("mctest-XXXXXX", NULL);
    mctest_assert_not_null (dir);
    vpath = vfs_path_from_str (dir);
    expected = count_entries (vpath);
    ck_assert_int_gt (expected, 0);
    mock_readdir__fail_after = 1;

    // when
    actual = count_entries (vpath);

    // then: the stream is rewound, so that no entry is lost
    ck_assert_int_eq (actual, expected);

    // cleanup
    vfs_path_free (vpath, TRUE);
    rmdir (dir);
    g_free (dir);
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
    tcase_add_test (tc_core, test_local_opendir_stale_eintr);
    tcase_add_test (tc_core, test_local_opendir_readdir_error);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
