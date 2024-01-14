/*
   libmc - checks for processing esc sequences in replace string

   Copyright (C) 2011-2024
   Free Software Foundation, Inc.

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

/* @DataSource("test_glob_translate_to_regex_ds") */
/* *INDENT-OFF* */
static const struct test_glob_translate_to_regex_ds
{
    const char *input_value;
    const char *expected_result;
} test_glob_translate_to_regex_ds[] =
{
    {
        "test*",
        "test(.*)"
    },
    {
        "t?es*t",
        "t(.)es(.*)t"
    },
    {
        "te{st}",
        "te(st)"
    },
    {
        "te{st|ts}",
        "te(st|ts)"
    },
    {
        "te{st,ts}",
        "te(st|ts)"
    },
    {
        "te[st]",
        "te[st]"
    },
    {
        "t,e.st",
        "t,e\\.st"
    },
    {
        "^t,e.+st+$",
        "\\^t,e\\.\\+st\\+\\$"
    },
    {
        "te!@#$%^&*()_+|\";:'{}:><?\\?\\*.,/[]|\\/st",
        "te!@#\\$%\\^&(.*)\\(\\)_\\+|\";:'():><(.)\\?\\*\\.,/[]|\\/st"
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_glob_translate_to_regex_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_glob_translate_to_regex, test_glob_translate_to_regex_ds)
/* *INDENT-ON* */
{
    /* given */
    GString *tmp = g_string_new (data->input_value);
    GString *dest_str;

    /* when */
    dest_str = mc_search__glob_translate_to_regex (tmp);

    /* then */
    g_string_free (tmp, TRUE);

    mctest_assert_str_eq (dest_str->str, data->expected_result);
    g_string_free (dest_str, TRUE);
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    /* Add new tests here: *************** */
    mctest_add_parameterized_test (tc_core, test_glob_translate_to_regex,
                                   test_glob_translate_to_regex_ds);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
