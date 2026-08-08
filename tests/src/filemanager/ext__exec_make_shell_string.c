/*
   src/filemanager - exec_make_shell_string() function testing

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Manuel Einfalt <einfalt1@proton.me>, 2026
   Andrew Borodin <aborodin@vmail.ru>, 2026

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

#define TEST_SUITE_NAME "/src/filemanager"

#include "tests/mctest.h"
#include "lib/global.h"
#include "lib/vfs/path.h"
#include "src/vfs/local/local.h"
#include "lib/strutil.h"
#include "src/filemanager/panel.h"

MC_TESTABLE GString *exec_make_shell_string (const char *lc_data, const vfs_path_t *filename_vpath,
                                             GString **buffer);

/* --------------------------------------------------------------------------------------------- */

static struct test_paths
{
    const char *lc_data;
    const char *path;
    const char *expected_buffer;
    const char *expected_ret;
} test_paths[] = { { "/foo/bar/hello_world.c", "/does_not_appear", NULL, "/foo/bar/hello_world.c" },
                   { "%cd /tmp/file/with/a/longer/path/name/xyz.sock", "/never/used",
                     "/tmp/file/with/a/longer/path/name/xyz.sock", "" },
                   { "%f/utar://", "/path/to/foo.tar", NULL, "/path/to/foo.tar/utar://" },
                   { "%cd %f", "/path/to/directory", "/path/to/directory", "" },
                   { "%f%cd /dev", "/etc/mc", "/dev", "/etc/mc" },
                   { "%cd %f%view{ascii}/some/subdir", "/tmp", "/tmp/some/subdir", "" }

};

/* --------------------------------------------------------------------------------------------- */

static void
setup (void)
{
    str_init_strings (NULL);
    vfs_init ();
    vfs_init_localfs ();
}

/* --------------------------------------------------------------------------------------------- */

START_PARAMETRIZED_TEST (shell_str_test, test_paths)
{
    vfs_path_t *vpath;
    GString *gs;
    GString *buffer = NULL;

    vpath = vfs_path_from_str (data->path);
    gs = exec_make_shell_string (data->lc_data, vpath, &buffer);
    vfs_path_free (vpath, FALSE);

    if (buffer == NULL)
    {
        mctest_assert_null (data->expected_buffer);
    }
    else
    {
        mctest_assert_str_eq (buffer->str, data->expected_buffer);
    }
    mctest_assert_str_eq (gs->str, data->expected_ret);

    if (buffer != NULL)
        g_string_free (buffer, TRUE);
    g_string_free (gs, TRUE);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (shell_long_path_test)
{
    char *dir;
    GString *path;
    vfs_path_t *filename_vpath;
    GString *shell_string;
    GString *cd_path = NULL;

    dir = g_strnfill (200, 'x');
    path = g_string_new ("/tmp");
    for (int i = 0; i < 6; i++)
    {
        g_string_append_c (path, PATH_SEP);
        g_string_append (path, dir);
    }
    g_string_append (path, "/file");
    g_free (dir);

    filename_vpath = vfs_path_from_str (path->str);
    shell_string = exec_make_shell_string ("%cd %f", filename_vpath, &cd_path);

    ck_assert_ptr_nonnull (cd_path);
    ck_assert_uint_gt (cd_path->len, BUF_1K);
    mctest_assert_str_eq (shell_string->str, "");
    mctest_assert_str_eq (cd_path->str, path->str);

    g_string_free (cd_path, TRUE);
    g_string_free (shell_string, TRUE);
    vfs_path_free (filename_vpath, TRUE);
    g_string_free (path, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");
    tcase_add_checked_fixture (tc_core, setup, NULL);

    // Add new tests here: ***************
    mctest_add_parameterized_test (tc_core, shell_str_test, test_paths);
    tcase_add_test (tc_core, shell_long_path_test);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
