/*
   src/subshell - tests for parse_subshell_prompt_string() function

   Copyright (C) 2021
   Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2021

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

#define TEST_SUITE_NAME "/src/subshell"

#include "tests/mctest.h"

#include "src/subshell/common.c"

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    mc_global.mc_run_mode = MC_RUN_FULL;
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    g_string_free (subshell_prompt, TRUE);
    subshell_prompt = NULL;
    g_string_free (subshell_prompt_temp_buffer, TRUE);
    subshell_prompt_temp_buffer = NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_parse_subshell_prompt_string_ds") */
/* *INDENT-OFF* */
static const struct test_parse_subshell_prompt_string_ds
{
    const char *input_value;
    const char *expected_result;
} test_parse_subshell_prompt_string_ds[] =
{
    { /* 0 */
        "blabla",
        "blabla"
    },
    { /* 1 */
        "blabla\r",
        "blabla"
    },
    { /* 2 */
        "blabla\n",
        "blabla"
    },
    { /* 3 */
        "blabla\r\n",
        "blabla"
    },
    { /* 4 */
        "\r\n",
        ""
    },
    { /* 5 */
        "\rblabla",
        "blabla"
    },
    { /* 6 */
        "\r\nblabla",
        "blabla"
    },
    { /* 7 */
        "\r\nblabla\r\n",
        "blabla"
    },
    { /* 8 */
        "bla1\r\nbla2\r\n",
        "bla2"
    }
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_parse_subshell_prompt_string_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_parse_subshell_prompt_string, test_parse_subshell_prompt_string_ds)
/* *INDENT-ON* */
{
    /* when */
    parse_subshell_prompt_string (data->input_value, strlen (data->input_value));
    /* then */
    mctest_assert_str_eq (subshell_prompt_temp_buffer->str, data->expected_result);
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

    tcase_add_checked_fixture (tc_core, setup, teardown);

    /* Add new tests here: *************** */
    mctest_add_parameterized_test (tc_core, test_parse_subshell_prompt_string,
                                   test_parse_subshell_prompt_string_ds);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
