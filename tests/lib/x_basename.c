/*
   lib - x_basename() function testing

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

#define TEST_SUITE_NAME "/lib"

#include "tests/mctest.h"

#include "lib/strutil.h"
#include "lib/util.h"

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_x_basename_ds") */
static const struct test_x_basename_ds
{
    const char *input_value;
    const char *expected_result;
} test_x_basename_ds[] = {
    {
        "/test/path/test2/path2",
        "path2",
    },
    {
        "/test/path/test2/path2#vfsprefix",
        "path2#vfsprefix",
    },
    {
        "/test/path/test2/path2/vfsprefix://",
        "path2/vfsprefix://",
    },
    {
        "/test/path/test2/path2/vfsprefix://subdir",
        "subdir",
    },
    {
        "/test/path/test2/path2/vfsprefix://subdir/",
        "subdir/",
    },
    {
        "/test/path/test2/path2/vfsprefix://subdir/subdir2",
        "subdir2",
    },
    {
        "/test/path/test2/path2/vfsprefix:///",
        "/",
    },
};

/* @Test(dataSource = "test_x_basename_ds") */
START_PARAMETRIZED_TEST (test_x_basename, test_x_basename_ds)
{
    // given
    const char *actual_result;

    // when
    actual_result = x_basename (data->input_value);

    // then
    mctest_assert_str_eq (actual_result, data->expected_result);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    // Add new tests here: ***************
    mctest_add_parameterized_test (tc_core, test_x_basename, test_x_basename_ds);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
