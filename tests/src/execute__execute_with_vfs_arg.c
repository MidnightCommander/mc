/*
   src - tests for execute_with_vfs_arg() function

   Copyright (C) 2013-2025
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2013

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

#define TEST_SUITE_NAME "/src"

#include "tests/mctest.h"

#include "execute__common.c"

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("the_file_is_local_ds") */
static const struct the_file_is_local_ds
{
    const char *input_path;
} the_file_is_local_ds[] = {
    {
        NULL,
    },
    {
        "/blabla",
    },
};

/* @Test(dataSource = "the_file_is_local_ds") */
START_PARAMETRIZED_TEST (the_file_is_local, the_file_is_local_ds)
{
    // given
    vfs_path_t *filename_vpath;
    filename_vpath = vfs_path_from_str (data->input_path);

    vfs_file_is_local__return_value = TRUE;

    // when
    execute_with_vfs_arg ("cmd_for_local_file", filename_vpath);

    // then
    mctest_assert_str_eq (do_execute__lc_shell__captured, "cmd_for_local_file");
    mctest_assert_str_eq (do_execute__command__captured, data->input_path);

    ck_assert_int_eq (vfs_file_is_local__vpath__captured->len, 1);
    {
        const vfs_path_t *tmp_vpath;

        tmp_vpath = (data->input_path == NULL) ? vfs_get_raw_current_dir () : filename_vpath;
        mctest_assert_true (
            vfs_path_equal (g_ptr_array_index (vfs_file_is_local__vpath__captured, 0), tmp_vpath));
    }
    ck_assert_int_eq (do_execute__flags__captured, EXECUTE_INTERNAL);
    ck_assert_msg (mc_getlocalcopy__pathname_vpath__captured == NULL,
                   "\nFunction mc_getlocalcopy() shouldn't be called!");

    vfs_path_free (filename_vpath, TRUE);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (the_file_is_remote_but_empty)
{
    // given
    vfs_path_t *filename_vpath;
    filename_vpath = NULL;

    vfs_file_is_local__return_value = FALSE;

    // when
    execute_with_vfs_arg ("cmd_for_remote_file", filename_vpath);

    // then
    mctest_assert_str_eq (do_execute__lc_shell__captured, NULL);
    mctest_assert_str_eq (do_execute__command__captured, NULL);

    ck_assert_int_eq (vfs_file_is_local__vpath__captured->len, 2);

    mctest_assert_true (vfs_path_equal (g_ptr_array_index (vfs_file_is_local__vpath__captured, 0),
                                        vfs_get_raw_current_dir ()));
    ck_assert_msg (g_ptr_array_index (vfs_file_is_local__vpath__captured, 1) == NULL,
                   "\nParameter for second call to vfs_file_is_local() should be NULL!");
    ck_assert_msg (mc_getlocalcopy__pathname_vpath__captured == NULL,
                   "\nFunction mc_getlocalcopy() shouldn't be called!");

    vfs_path_free (filename_vpath, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (the_file_is_remote_fail_to_create_local_copy)
{
    // given
    vfs_path_t *filename_vpath;

    filename_vpath = vfs_path_from_str ("/ftp://some.host/editme.txt");

    vfs_file_is_local__return_value = FALSE;
    mc_getlocalcopy__return_value = NULL;

    // when
    execute_with_vfs_arg ("cmd_for_remote_file", filename_vpath);

    // then
    mctest_assert_str_eq (do_execute__lc_shell__captured, NULL);
    mctest_assert_str_eq (do_execute__command__captured, NULL);

    ck_assert_int_eq (vfs_file_is_local__vpath__captured->len, 1);

    mctest_assert_true (
        vfs_path_equal (g_ptr_array_index (vfs_file_is_local__vpath__captured, 0), filename_vpath));

    mctest_assert_true (vfs_path_equal (mc_getlocalcopy__pathname_vpath__captured, filename_vpath));

    mctest_assert_str_eq (message_title__captured, _ ("Error"));
    mctest_assert_str_eq (message_text__captured,
                          _ ("Cannot fetch a local copy of /ftp://some.host/editme.txt"));

    vfs_path_free (filename_vpath, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (the_file_is_remote)
{
    // given
    vfs_path_t *filename_vpath, *local_vpath, *local_vpath_should_be_freeing;

    filename_vpath = vfs_path_from_str ("/ftp://some.host/editme.txt");
    local_vpath = vfs_path_from_str ("/tmp/blabla-editme.txt");
    local_vpath_should_be_freeing = vfs_path_clone (local_vpath);

    vfs_file_is_local__return_value = FALSE;
    mc_getlocalcopy__return_value = local_vpath_should_be_freeing;

    // when
    execute_with_vfs_arg ("cmd_for_remote_file", filename_vpath);

    // then
    mctest_assert_str_eq (do_execute__lc_shell__captured, "cmd_for_remote_file");
    mctest_assert_str_eq (do_execute__command__captured, "/tmp/blabla-editme.txt");

    ck_assert_int_eq (vfs_file_is_local__vpath__captured->len, 1);

    mctest_assert_true (
        vfs_path_equal (g_ptr_array_index (vfs_file_is_local__vpath__captured, 0), filename_vpath));

    mctest_assert_true (vfs_path_equal (mc_getlocalcopy__pathname_vpath__captured, filename_vpath));

    ck_assert_int_eq (mc_stat__vpath__captured->len, 2);

    mctest_assert_true (
        vfs_path_equal (g_ptr_array_index (mc_stat__vpath__captured, 0), local_vpath));
    mctest_assert_true (vfs_path_equal (g_ptr_array_index (mc_stat__vpath__captured, 0),
                                        g_ptr_array_index (mc_stat__vpath__captured, 1)));

    mctest_assert_true (
        vfs_path_equal (mc_ungetlocalcopy__pathname_vpath__captured, filename_vpath));

    mctest_assert_true (vfs_path_equal (mc_ungetlocalcopy__local_vpath__captured, local_vpath));

    vfs_path_free (filename_vpath, TRUE);
    vfs_path_free (local_vpath, TRUE);
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
    mctest_add_parameterized_test (tc_core, the_file_is_local, the_file_is_local_ds);
    tcase_add_test (tc_core, the_file_is_remote_but_empty);
    tcase_add_test (tc_core, the_file_is_remote_fail_to_create_local_copy);
    tcase_add_test (tc_core, the_file_is_remote);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
