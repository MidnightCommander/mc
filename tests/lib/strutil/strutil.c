/*
   lib/strutil - tests for lib/strutil/fileverscmp function.

   Copyright (C) 2019-2025
   Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2019

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

#define TEST_SUITE_NAME "/lib/strutil"

#include "tests/mctest.h"

#include "lib/strutil.h"

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

/* @DataSource("str_chomp_test_ds1") */
/* Testcases are taken from Glib */
static const struct str_chomp_test_struct
{
    const char *input_string;
    const char *expected_result;
} str_chomp_test_ds1[] = {
    {"", ""},
    {" \n\r", " \n"},
    {" \t\r\n", " \t"},
    {"a \r ", "a \r "},
    {"a  \n ", "a  \n "},
    {"a a\n\r\n", "a a\n"},
    {"\na a \r", "\na a "},
};

/* @Test(dataSource = "str_chomp_test_ds1") */
START_TEST (str_chomp_test1)
{
    /* given */
    const struct str_chomp_test_struct *data = &str_chomp_test_ds1[_i];

    /* when */
    char *actual_result = g_strdup (data->input_string);
    str_chomp (actual_result);

    /* then */
    ck_assert_str_eq (actual_result, data->expected_result);

    g_free (actual_result);
}

END_TEST
    /* --------------------------------------------------------------------------------------------- */
START_TEST (str_chomp_test_null)
{
    char *ptr = NULL;
    str_chomp (ptr);
    ck_assert_ptr_null (ptr);
}

END_TEST
    /* --------------------------------------------------------------------------------------------- */
    int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    /* Add new tests here: *************** */
    mctest_add_parameterized_test (tc_core, str_chomp_test1, str_chomp_test_ds1);
    tcase_add_test (tc_core, str_chomp_test_null);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
