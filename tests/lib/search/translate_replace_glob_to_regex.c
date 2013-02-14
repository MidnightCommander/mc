/*
   libmc - checks for processing esc sequences in replace string

   Copyright (C) 2011, 2013
   The Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2011
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

#define TEST_SUITE_NAME "lib/search/glob"

#include "tests/mctest.h"

#include "glob.c"               /* for testing static functions */

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_translate_replace_glob_to_regex_ds") */
/* *INDENT-OFF* */
static const struct test_translate_replace_glob_to_regex_ds
{
    const char *input_value;
    const char *expected_result;
} test_translate_replace_glob_to_regex_ds[] =
{
    {
        "a&a?a",
        "a\\&a\\1a"
    },
    {
        "a\\&a?a",
        "a\\&a\\1a"
    },
    {
        "a&a\\?a",
        "a\\&a\\?a"
    },
    {
        "a\\&a\\?a",
        "a\\&a\\?a"
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_translate_replace_glob_to_regex_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_translate_replace_glob_to_regex, test_translate_replace_glob_to_regex_ds)
/* *INDENT-ON* */
{
    /* given */
    GString *dest_str;

    /* when */
    dest_str = mc_search__translate_replace_glob_to_regex (data->input_value);

    /* then */
    mctest_assert_str_eq (dest_str->str, data->expected_result) g_string_free (dest_str, TRUE);
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

    /* Add new tests here: *************** */
    mctest_add_parameterized_test (tc_core, test_translate_replace_glob_to_regex,
                                   test_translate_replace_glob_to_regex_ds);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
