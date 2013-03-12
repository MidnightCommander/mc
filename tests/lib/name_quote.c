/*
   lib/vfs - Quote file names

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

#define TEST_SUITE_NAME "/lib/util"

#include "tests/mctest.h"

#include "lib/util.h"

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
}

/* @After */
static void
teardown (void)
{
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("data_source1") */
/* *INDENT-OFF* */
static const struct data_source1
{
    gboolean input_quote_percent;
    const char *input_string;

    const char *expected_string;
} data_source1[] =
{
    { TRUE, "%%", "%%%%"},
    { FALSE, "%%", "%%"},
};
/* *INDENT-ON* */

/* @Test(dataSource = "data_source1") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (quote_percent_test, data_source1)
/* *INDENT-ON* */
{
    /* given */
    char *actual_string;

    /* when */
    actual_string = name_quote (data->input_string, data->input_quote_percent);

    /* then */
    mctest_assert_str_eq (actual_string, data->expected_string);

    g_free (actual_string);
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("data_source2") */
/* *INDENT-OFF* */
static const struct data_source2
{
    const char *input_string;

    const char *expected_string;
} data_source2[] =
{
    {"-", "./-"},
    {"blabla-", "blabla-"},
    {"\r\n\t", "\\\r\\\n\\\t"},
    {"'\\\";?|[]{}<>`!$&*()", "\\'\\\\\\\"\\;\\?\\|\\[\\]\\{\\}\\<\\>\\`\\!\\$\\&\\*\\(\\)"},
    {"a b c ", "a\\ b\\ c\\ "},
    {"#", "\\#"},
    {"blabla#", "blabla#"},
    {"~", "\\~"},
    {"blabla~", "blabla~"},
};
/* *INDENT-ON* */

/* @Test(dataSource = "data_source2") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (name_quote_test, data_source2)
/* *INDENT-ON* */
{
    /* given */
    char *actual_string;

    /* when */
    actual_string = name_quote (data->input_string, FALSE);

    /* then */
    mctest_assert_str_eq (actual_string, data->expected_string);

    g_free (actual_string);
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
    mctest_add_parameterized_test (tc_core, quote_percent_test, data_source1);
    mctest_add_parameterized_test (tc_core, name_quote_test, data_source2);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "name_quote.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
