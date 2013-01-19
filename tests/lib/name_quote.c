/*
   lib/vfs - Quote file names

   Copyright (C) 2011
   The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2011

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

#include <config.h>

#include <check.h>

#include "lib/global.h"
#include "lib/util.h"

/* --------------------------------------------------------------------------------------------- */

static void
setup (void)
{
}

static void
teardown (void)
{
}

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */
static const struct data_source1_struct
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

/* *INDENT-OFF* */
START_TEST (quote_percent_test)
/* *INDENT-ON* */
{
    /* given */
    char *actual_string;
    const struct data_source1_struct test_data = data_source1[_i];

    /* when */
    actual_string = name_quote (test_data.input_string, test_data.input_quote_percent);

    /* then */
    g_assert_cmpstr (actual_string, ==, test_data.expected_string);

    g_free (actual_string);
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */
static const struct data_source2_struct
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

/* *INDENT-OFF* */
START_TEST (name_quote_test)
/* *INDENT-ON* */
{
    /* given */
    char *actual_string;
    const struct data_source2_struct test_data = data_source2[_i];

    /* when */
    actual_string = name_quote (test_data.input_string, FALSE);

    /* then */
    g_assert_cmpstr (actual_string, ==, test_data.expected_string);

    g_free (actual_string);
}
/* *INDENT-OFF* */
END_TEST
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
    tcase_add_loop_test (tc_core, quote_percent_test, 0,
                         sizeof (data_source1) / sizeof (data_source1[0]));

    tcase_add_loop_test (tc_core, name_quote_test, 0,
                         sizeof (data_source2) / sizeof (data_source2[0]));
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "serialize.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
