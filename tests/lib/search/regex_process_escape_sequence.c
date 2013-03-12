/*
   libmc - checks for processing esc sequences in replace string

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

#define TEST_SUITE_NAME "lib/search/regex"

#include "tests/mctest.h"

#include "regex.c"              /* for testing static functions */

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_regex_process_escape_sequence_ds") */
/* *INDENT-OFF* */
static const struct test_regex_process_escape_sequence_ds
{
    const char *input_from;
    const replace_transform_type_t input_initial_flags;
    const gboolean input_use_utf;
    const char *expected_string;
} test_regex_process_escape_sequence_ds[] =
{
    { /* 0. */
        "{101}",
        REPLACE_T_NO_TRANSFORM,
        FALSE,
        "A"
    },
    { /* 1. */
        "x42",
        REPLACE_T_NO_TRANSFORM,
        FALSE,
        "B"
    },
    { /* 2. */
        "x{444}",
        REPLACE_T_NO_TRANSFORM,
        FALSE,
        "D"
    },
    { /* 3. */
        "x{444}",
        REPLACE_T_NO_TRANSFORM,
        TRUE,
        "ф"
    },
    { /* 4. */
        "n",
        REPLACE_T_NO_TRANSFORM,
        FALSE,
        "\n"
    },
    { /* 5. */
        "t",
        REPLACE_T_NO_TRANSFORM,
        FALSE,
        "\t"
    },
    { /* 6. */
        "v",
        REPLACE_T_NO_TRANSFORM,
        FALSE,
        "\v"
    },
    { /* 7. */
        "b",
        REPLACE_T_NO_TRANSFORM,
        FALSE,
        "\b"
    },
    { /* 8. */
        "r",
        REPLACE_T_NO_TRANSFORM,
        FALSE,
        "\r"
    },
    { /* 9. */
        "f",
        REPLACE_T_NO_TRANSFORM,
        FALSE,
        "\f"
    },
    { /* 10. */
        "a",
        REPLACE_T_NO_TRANSFORM,
        FALSE,
        "\a"
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_regex_process_escape_sequence_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_regex_process_escape_sequence, test_regex_process_escape_sequence_ds)
/* *INDENT-ON* */
{
    /* given */
    GString *actual_string;
    replace_transform_type_t replace_flags = REPLACE_T_NO_TRANSFORM;

    replace_flags = data->input_initial_flags;
    actual_string = g_string_new ("");

    /* when */
    mc_search_regex__process_escape_sequence (actual_string, data->input_from, -1, &replace_flags,
                                              data->input_use_utf);

    /* then */
    mctest_assert_str_eq (actual_string->str, data->expected_string);

    g_string_free (actual_string, TRUE);
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
    mctest_add_parameterized_test (tc_core, test_regex_process_escape_sequence,
                                   test_regex_process_escape_sequence_ds);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
