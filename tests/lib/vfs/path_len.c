/* lib/vfs - tests for vfspath_len() function.

   Copyright (C) 2011-2025
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2011, 2013

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

#define TEST_SUITE_NAME "/lib/vfs"

#include "tests/mctest.h"

#include "lib/charsets.h"

#include "lib/strutil.h"
#include "lib/vfs/xdirentry.h"
#include "lib/vfs/path.h"

#include "src/vfs/local/local.c"

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    str_init_strings ("UTF-8");

    vfs_init ();
    vfs_init_localfs ();
    vfs_setup_work_dir ();

    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;
    load_codepages_list ();
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    free_codepages_list ();
    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */
/* @DataSource("test_path_length_ds") */
static const struct test_path_length_ds
{
    const char *input_path;
    const size_t expected_length_element_encoding;
    const size_t expected_length_terminal_encoding;
} test_path_length_ds[] = {
    {
        // 0.
        NULL,
        0,
        0,
    },
    {
        // 1.
        "/",
        1,
        1,
    },
    {
        // 2.
        "/тестовый/путь",
        26,
        26,
    },
    {
        // 3.
        "/#enc:KOI8-R/тестовый/путь",
        14,
        38,
    },
};

/* @Test(dataSource = "test_path_length_ds") */
START_PARAMETRIZED_TEST (test_path_length, test_path_length_ds)
{
    // given
    vfs_path_t *vpath;
    char *path;
    size_t actual_length_terminal_encoding, actual_length_element_encoding;

    vpath = vfs_path_from_str (data->input_path);
    path = vpath != NULL ? vfs_path_get_by_index (vpath, 0)->path : NULL;

    // when
    actual_length_terminal_encoding = vfs_path_len (vpath);
    actual_length_element_encoding = path != NULL ? strlen (path) : 0;

    // then
    ck_assert_int_eq (actual_length_terminal_encoding, data->expected_length_terminal_encoding);
    ck_assert_int_eq (actual_length_element_encoding, data->expected_length_element_encoding);

    vfs_path_free (vpath, TRUE);
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
    mctest_add_parameterized_test (tc_core, test_path_length, test_path_length_ds);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
