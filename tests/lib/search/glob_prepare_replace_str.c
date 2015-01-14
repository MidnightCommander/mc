/*
   libmc - checks for processing esc sequences in replace string

   Copyright (C) 2011-2014
   Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2014

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

/* @DataSource("test_glob_prepare_replace_str_ds") */
/* *INDENT-OFF* */
static const struct test_glob_prepare_replace_str_ds
{
    const char *input_value;
    const char *glob_str;
    const char *replace_str;
    const char *expected_result;
} test_glob_prepare_replace_str_ds[] =
{
    { /* 0. */
        "qqwwee",
        "*ww*",
        "\\1AA\\2",
        "qqAAee"
    },
    { /* 1. */
        "qqwwee",
        "*qq*",
        "\\1SS\\2",
        "SSwwee"
    },
    { /* 2. */
        "qqwwee",
        "*ee*",
        "\\1RR\\2",
        "qqwwRR"
    }
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_glob_prepare_replace_str_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_glob_prepare_replace_str, test_glob_prepare_replace_str_ds)
/* *INDENT-ON* */
{
    /* given */
    mc_search_t *s;
    char *dest_str;

    s = mc_search_new (data->glob_str, -1, NULL);
    s->is_case_sensitive = TRUE;
    s->search_type = MC_SEARCH_T_GLOB;

    /* when */
    mc_search_run (s, data->input_value, 0, strlen (data->input_value), NULL);
    dest_str = mc_search_prepare_replace_str2 (s, (char *) data->replace_str);

    /* then */
    mctest_assert_str_eq (dest_str, data->expected_result);

    g_free (dest_str);
    mc_search_free (s);
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
    mctest_add_parameterized_test (tc_core, test_glob_prepare_replace_str,
                                   test_glob_prepare_replace_str_ds);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
