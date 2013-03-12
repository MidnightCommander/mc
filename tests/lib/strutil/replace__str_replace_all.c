/*
   lib/strutil - tests for lib/strutil/replace:str_replace_all() function.

   Copyright (C) 2013
   The Free Software Foundation, Inc.

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

#define TEST_SUITE_NAME "/lib/strutil"

#include "tests/mctest.h"

#include "lib/strutil.h"

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    str_init_strings (NULL);
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("str_replace_all_test_ds") */
/* *INDENT-OFF* */
static const struct str_replace_all_test_ds
{
    const char *haystack;
    const char *needle;
    const char *replacement;
    const char *expected_result;
} str_replace_all_test_ds[] =
{
    {
        /* 0. needle not found*/
        "needle not found",
        "blablablabla",
        "1234567890",
        "needle not found",
    },
    {
        /* 1.  replacement is less rather that needle */
        "some string blablablabla string",
       "blablablabla",
        "1234",
        "some string 1234 string",
    },
    {
        /* 2.  replacement is great rather that needle */
        "some string bla string",
        "bla",
        "1234567890",
        "some string 1234567890 string",
    },
    {
        /* 3.  replace few substrings in a start of string */
        "blabla blabla string",
        "blabla",
        "111111111",
        "111111111 111111111 string",
    },
    {
        /* 4.  replace few substrings in a middle of string */
        "some string blabla str blabla string",
        "blabla",
        "111111111",
        "some string 111111111 str 111111111 string",
    },
    {
        /* 5.  replace few substrings in an end of string */
        "some string blabla str blabla",
        "blabla",
        "111111111",
        "some string 111111111 str 111111111",
    },
    {
        /* 6.  escaped substring */
        "\\blabla blabla",
        "blabla",
        "111111111",
        "blabla 111111111",
    },
    {
        /* 7.  escaped substring */
        "str \\blabla blabla",
        "blabla",
        "111111111",
        "str blabla 111111111",
    },
    {
        /* 8.  escaped substring */
        "str \\\\\\blabla blabla",
        "blabla",
        "111111111",
        "str \\\\blabla 111111111",
    },
    {
        /* 9.  double-escaped substring (actually non-escaped) */
        "\\\\blabla blabla",
        "blabla",
        "111111111",
        "\\\\111111111 111111111",
    },
    {
        /* 10.  partial substring */
        "blablabla",
        "blabla",
        "111111111",
        "111111111bla",
    },
    {
        /* 11.  special symbols */
        "bla bla",
        "bla",
        "111\t1 1\n1111",
        "111\t1 1\n1111 111\t1 1\n1111",
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "str_replace_all_test_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (str_replace_all_test, str_replace_all_test_ds)
/* *INDENT-ON* */
{
    /* given */
    char *actual_result;

    /* when */
    actual_result = str_replace_all (data->haystack, data->needle, data->replacement);

    /* then */
    g_assert_cmpstr (actual_result, ==, data->expected_result);
    g_free (actual_result);
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
    mctest_add_parameterized_test (tc_core, str_replace_all_test, str_replace_all_test_ds);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "replace__str_replace_all.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
