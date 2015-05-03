/*
   libmc - checks for producing compile flags

   Copyright (C) 2011-2015
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2015

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

#include "regex.c"              /* for testing static functions */

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_glob_translate_to_regex_ds") */
/* *INDENT-OFF* */
static const struct test_regex_get_compile_flags_ds
{
    const char *charset;
    const gboolean utf_flag;
    const gboolean is_case_sensitive;
    const GRegexCompileFlags expected_result;
} test_regex_get_compile_flags_ds[] =
{
    {
        "utf8",
        TRUE,
        TRUE,
        G_REGEX_OPTIMIZE | G_REGEX_DOTALL
    },
    {
        "utf8",
        FALSE,
        TRUE,
        G_REGEX_OPTIMIZE | G_REGEX_DOTALL | G_REGEX_RAW
    },
    {
        "utf8",
        TRUE,
        FALSE,
        G_REGEX_OPTIMIZE | G_REGEX_DOTALL | G_REGEX_CASELESS
    },
    {
        "utf8",
        FALSE,
        FALSE,
        G_REGEX_OPTIMIZE | G_REGEX_DOTALL | G_REGEX_RAW | G_REGEX_CASELESS
    },
    {
        "utf-8",
        TRUE,
        TRUE,
        G_REGEX_OPTIMIZE | G_REGEX_DOTALL
    },
    {
        "utf-8",
        FALSE,
        TRUE,
        G_REGEX_OPTIMIZE | G_REGEX_DOTALL | G_REGEX_RAW
    },
    {
        "utf-8",
        TRUE,
        FALSE,
        G_REGEX_OPTIMIZE | G_REGEX_DOTALL | G_REGEX_CASELESS
    },
    {
        "utf-8",
        FALSE,
        FALSE,
        G_REGEX_OPTIMIZE | G_REGEX_DOTALL | G_REGEX_RAW | G_REGEX_CASELESS
    },
    {
        "latin1",
        TRUE,
        TRUE,
        G_REGEX_OPTIMIZE | G_REGEX_DOTALL  | G_REGEX_RAW
    },
    {
        "latin1",
        FALSE,
        TRUE,
        G_REGEX_OPTIMIZE | G_REGEX_DOTALL | G_REGEX_RAW
    },
    {
        "blablabla",
        TRUE,
        TRUE,
        G_REGEX_OPTIMIZE | G_REGEX_DOTALL  | G_REGEX_RAW
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_regex_get_compile_flags_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_regex_get_compile_flags, test_regex_get_compile_flags_ds)
/* *INDENT-ON* */
{
    GRegexCompileFlags actual_result;

    /* given */
    mc_global.utf8_display = data->utf_flag;

    /* when */
    actual_result = mc_search__regex_get_compile_flags (data->charset, data->is_case_sensitive);

    /* then */
    mctest_assert_int_eq (actual_result, data->expected_result);
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
    mctest_add_parameterized_test (tc_core, test_regex_get_compile_flags,
                                   test_regex_get_compile_flags_ds);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "regex_get_compile_flags.log");
    srunner_run_all (sr, CK_ENV);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* --------------------------------------------------------------------------------------------- */
