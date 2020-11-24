/* lib/vfs - test vfs_path_from_str_flags() function

   Copyright (C) 2013-2020
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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define TEST_SUITE_NAME "/lib/vfs"

#include "tests/mctest.h"

#include "lib/strutil.h"
#include "lib/vfs/xdirentry.h"
#include "lib/vfs/path.h"

#include "src/vfs/local/local.c"

/* --------------------------------------------------------------------------------------------- */

/* @Mock */
const char *
mc_config_get_home_dir (void)
{
    return "/mock/test";
}

/* --------------------------------------------------------------------------------------------- */
/* @Before */
static void
setup (void)
{
    mc_global.timer = mc_timer_new ();
    str_init_strings (NULL);

    vfs_init ();
    vfs_init_localfs ();
    vfs_setup_work_dir ();
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    vfs_shut ();
    str_uninit_strings ();
    mc_timer_destroy (mc_global.timer);
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_from_to_string_ds") */
/* *INDENT-OFF* */
static const struct test_strip_home_ds
{
    const char *input_string;
    const char *expected_result;
} test_strip_home_ds[] =
{
    { /* 0. */
        "/mock/test/some/path",
        "~/some/path"
    },
    { /* 1. */
        "/mock/testttt/some/path",
        "/mock/testttt/some/path"
    },
};
/* *INDENT-ON* */

/* @Test */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_strip_home, test_strip_home_ds)
/* *INDENT-ON* */
{
    /* given */
    vfs_path_t *actual_result;

    /* when */
    actual_result = vfs_path_from_str_flags (data->input_string, VPF_STRIP_HOME);

    /* then */
    mctest_assert_str_eq (actual_result->str, data->expected_result);

    vfs_path_free (actual_result);
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    int number_failed;

    Suite *s = suite_create (TEST_SUITE_NAME);
    TCase *tc_core = tcase_create ("Core");
    SRunner *sr;

    tcase_add_checked_fixture (tc_core, setup, teardown);

    /* Add new tests here: *************** */
    mctest_add_parameterized_test (tc_core, test_strip_home, test_strip_home_ds);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "vfs_path_from_str_flags.log");
    srunner_run_all (sr, CK_ENV);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* --------------------------------------------------------------------------------------------- */
