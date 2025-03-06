/*
   lib - Quote file names

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

#define TEST_SUITE_NAME "/lib/util"

#include "tests/mctest.h"

#include "lib/util.h"

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("data_source1") */
static const struct data_source1
{
    gboolean input_quote_percent;
    const char *input_string;

    const char *expected_string;
} data_source1[] = {
    { TRUE, "%%", "%%%%" },
    { FALSE, "%%", "%%" },
};

/* @Test(dataSource = "data_source1") */
START_PARAMETRIZED_TEST (quote_percent_test, data_source1)
{
    // given
    char *actual_string;

    // when
    actual_string = name_quote (data->input_string, data->input_quote_percent);

    // then
    mctest_assert_str_eq (actual_string, data->expected_string);

    g_free (actual_string);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("data_source2") */
static const struct data_source2
{
    const char *input_string;

    const char *expected_string;
} data_source2[] = {
    { "", NULL },
    { "-", "./-" },
    { "blabla-", "blabla-" },
    { "\r\n\t", "\\\r\\\n\\\t" },
    { "'\\\";?|[]{}<>`!$&*()", "\\'\\\\\\\"\\;\\?\\|\\[\\]\\{\\}\\<\\>\\`\\!\\$\\&\\*\\(\\)" },
    { "a b c ", "a\\ b\\ c\\ " },
    { "#", "\\#" },
    { "blabla#", "blabla#" },
    { "~", "\\~" },
    { "blabla~", "blabla~" },
    {
        NULL,
        NULL,
    },
};

/* @Test(dataSource = "data_source2") */
START_PARAMETRIZED_TEST (name_quote_test, data_source2)
{
    // given
    char *actual_string;

    // when
    actual_string = name_quote (data->input_string, FALSE);

    // then
    mctest_assert_str_eq (actual_string, data->expected_string);

    g_free (actual_string);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    // Add new tests here: ***************
    mctest_add_parameterized_test (tc_core, quote_percent_test, data_source1);
    mctest_add_parameterized_test (tc_core, name_quote_test, data_source2);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
