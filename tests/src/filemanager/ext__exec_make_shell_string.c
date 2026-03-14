/*
   src/filemanager - exec_make_shell_string() function testing

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Manuel Einfalt <einfalt1@proton.me>, 2026

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

#define TEST_SUITE_NAME "/lib"

#include "tests/mctest.h"
#include "lib/global.h"
#include "lib/vfs/path.h"
#include "src/vfs/local/local.h"
#include "lib/strutil.h"
#include "src/filemanager/panel.h"

extern char buffer[BUF_1K];

MC_TESTABLE GString *exec_make_shell_string (const char *lc_data, const vfs_path_t *filename_vpath);

/* --------------------------------------------------------------------------------------------- */

static struct test_paths
{
    const char *lc_data;
    const char *path;
    const char *expected_buffer;
    const char *expected_ret;
} test_paths[] = { { "/foo/bar/hello_world.c", "/does_not_appear", "", "/foo/bar/hello_world.c" },
                   { "%cd /tmp/file/with/a/longer/path/name/xyz.sock", "/never/used",
                     "/tmp/file/with/a/longer/path/name/xyz.sock", "" },
                   { "%f/utar://", "/path/to/foo.tar", "", "/path/to/foo.tar/utar://" },
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

    vpath = vfs_path_from_str (data->path);
    gs = exec_make_shell_string (data->lc_data, vpath);
    vfs_path_free (vpath, FALSE);
    mctest_assert_str_eq (buffer, data->expected_buffer);
    mctest_assert_str_eq (gs->str, data->expected_ret);
    g_string_free (gs, TRUE);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");
    tcase_add_checked_fixture (tc_core, setup, NULL);

    // Add new tests here: ***************
    mctest_add_parameterized_test (tc_core, shell_str_test, test_paths);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */