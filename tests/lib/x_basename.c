/*
   lib/vfs - x_basename() function testing

   Copyright (C) 2011, 2013
   The Free Software Foundation, Inc.

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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define TEST_SUITE_NAME "/lib"

#include "tests/mctest.h"

#include <stdio.h>

#include "lib/strutil.h"
#include "lib/util.h"

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_x_basename_ds") */
/* *INDENT-OFF* */
static const struct test_x_basename_ds
{
    const char *input_value;
    const char *expected_result;
} test_x_basename_ds[] =
{
    {
        "/test/path/test2/path2",
        "path2"
    },
    {
        "/test/path/test2/path2#vfsprefix",
        "path2#vfsprefix"
    },
    {
        "/test/path/test2/path2/vfsprefix://",
        "path2/vfsprefix://"
    },
    {
        "/test/path/test2/path2/vfsprefix://subdir",
        "subdir"
    },
    {
        "/test/path/test2/path2/vfsprefix://subdir/",
        "subdir/"
    },
    {
        "/test/path/test2/path2/vfsprefix://subdir/subdir2",
        "subdir2"
    },
    {
        "/test/path/test2/path2/vfsprefix:///",
        "/"
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_x_basename_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_x_basename, test_x_basename_ds)
/* *INDENT-ON* */
{
    /* given */
    const char *actual_result;

    /* when */
    actual_result = x_basename (data->input_value);

    /* then */
    mctest_assert_str_eq (actual_result, data->expected_result);
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
    mctest_add_parameterized_test (tc_core, test_x_basename, test_x_basename_ds);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "x_basename.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
