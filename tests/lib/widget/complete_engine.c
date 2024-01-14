/*
   lib/widget - tests for autocomplete feature

   Copyright (C) 2013-2024
   Free Software Foundation, Inc.

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

#define TEST_SUITE_NAME "/lib/widget"

#include "tests/mctest.h"

#include "lib/strutil.h"
#include "lib/widget.h"

/* --------------------------------------------------------------------------------------------- */

void complete_engine_fill_completions (WInput * in);
char **try_complete (char *text, int *lc_start, int *lc_end, input_complete_t flags);

/* --------------------------------------------------------------------------------------------- */

/* @CapturedValue */
static char *try_complete__text__captured;
/* @CapturedValue */
static int try_complete__lc_start__captured;
/* @CapturedValue */
static int try_complete__lc_end__captured;
/* @CapturedValue */
static input_complete_t try_complete__flags__captured;

/* @ThenReturnValue */
static char **try_complete__return_value;

/* @Mock */
char **
try_complete (char *text, int *lc_start, int *lc_end, input_complete_t flags)
{
    try_complete__text__captured = g_strdup (text);
    try_complete__lc_start__captured = *lc_start;
    try_complete__lc_end__captured = *lc_end;
    try_complete__flags__captured = flags;

    return try_complete__return_value;
}

static void
try_complete__init (void)
{
    try_complete__text__captured = NULL;
    try_complete__lc_start__captured = 0;
    try_complete__lc_end__captured = 0;
    try_complete__flags__captured = 0;
}

static void
try_complete__deinit (void)
{
    g_free (try_complete__text__captured);
}

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    str_init_strings (NULL);
    try_complete__init ();
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    try_complete__deinit ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_complete_engine_fill_completions_ds") */
/* *INDENT-OFF* */
static const struct test_complete_engine_fill_completions_ds
{
    const char *input_buffer;
    const int input_point;
    const input_complete_t input_completion_flags;
    int expected_start;
    int expected_end;
} test_complete_engine_fill_completions_ds[] =
{
    {
        "string",
        3,
        INPUT_COMPLETE_NONE,
        0,
        3
    },
    {
        "some string",
        7,
        INPUT_COMPLETE_NONE,
        0,
        7
    },
    {
        "some string",
        7,
        INPUT_COMPLETE_SHELL_ESC,
        5,
        7
    },
    {
        "some\\ string111",
        9,
        INPUT_COMPLETE_SHELL_ESC,
        0,
        9
    },
    {
        "some\\\tstring111",
        9,
        INPUT_COMPLETE_SHELL_ESC,
        0,
        9
    },
    {
        "some\tstring",
        7,
        INPUT_COMPLETE_NONE,
        5,
        7
    },
    {
        "some;string",
        7,
        INPUT_COMPLETE_NONE,
        5,
        7
    },
    {
        "some|string",
        7,
        INPUT_COMPLETE_NONE,
        5,
        7
    },
    {
        "some<string",
        7,
        INPUT_COMPLETE_NONE,
        5,
        7
    },
    {
        "some>string",
        7,
        INPUT_COMPLETE_NONE,
        5,
        7
    },
    {
        "some!@#$%^&*()_\\+~`\"',./?:string",
        30,
        INPUT_COMPLETE_NONE,
        0,
        30
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_complete_engine_fill_completions_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_complete_engine_fill_completions,
                         test_complete_engine_fill_completions_ds)
/* *INDENT-ON* */
{
    /* given */
    WInput *w_input;

    w_input = input_new (1, 1, NULL, 100, data->input_buffer, NULL, data->input_completion_flags);
    w_input->point = data->input_point;

    /* when */
    complete_engine_fill_completions (w_input);

    /* then */
    mctest_assert_str_eq (try_complete__text__captured, data->input_buffer);
    ck_assert_int_eq (try_complete__lc_start__captured, data->expected_start);
    ck_assert_int_eq (try_complete__lc_end__captured, data->expected_end);
    ck_assert_int_eq (try_complete__flags__captured, data->input_completion_flags);
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
    mctest_add_parameterized_test (tc_core, test_complete_engine_fill_completions,
                                   test_complete_engine_fill_completions_ds);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
