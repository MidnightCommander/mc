/*
   lib/strutil - tests for lib/strutil.c:str_rstrip_eol function

   Copyright (C) 2025
   Free Software Foundation, Inc.

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

#define TEST_SUITE_NAME "/lib/strutil"

#include "tests/mctest.h"

#include "lib/strutil.h"

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("str_rstrip_eol_test_ds1") */
/* Testcases are taken from Glib */
static const struct str_rstrip_eol_test_struct
{
    const char *input_string;
    const char *expected_result;
} str_rstrip_eol_test_ds1[] = {
    {
        "",
        "",
    },
    {
        " \n\r",
        " \n",
    },
    {
        " \t\r\n",
        " \t",
    },
    {
        "a \r ",
        "a \r ",
    },
    {
        " a  \n ",
        " a  \n ",
    },
    {
        "a a\n\r\n",
        "a a\n",
    },
    {
        "\na a \r",
        "\na a ",
    },
};

/* @Test(dataSource = "str_rstrip_eol_test_ds1") */
START_TEST (str_rstrip_eol_test1)
{
    /* given */
    const struct str_rstrip_eol_test_struct *data = &str_rstrip_eol_test_ds1[_i];

    /* when */
    char *actual_result = g_strdup (data->input_string);
    str_rstrip_eol (actual_result);

    /* then */
    ck_assert_str_eq (actual_result, data->expected_result);

    g_free (actual_result);
}

END_TEST
/* --------------------------------------------------------------------------------------------- */
START_TEST (str_rstrip_eol_test_null)
{
    char *ptr = NULL;
    str_rstrip_eol (ptr);
    ck_assert_ptr_null (ptr);
}

END_TEST
/* --------------------------------------------------------------------------------------------- */
int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    /* Add new tests here: *************** */
    mctest_add_parameterized_test (tc_core, str_rstrip_eol_test1, str_rstrip_eol_test_ds1);
    tcase_add_test (tc_core, str_rstrip_eol_test_null);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
