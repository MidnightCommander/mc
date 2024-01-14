/*
   lib/strutil - tests for lib/strutil/parse_integer function.

   Copyright (C) 2013-2024
   Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2013

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

#define TEST_SUITE_NAME "/lib/strutil"

#include "tests/mctest.h"

#include <inttypes.h>

#include "lib/strutil.h"

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("parse_integer_test_ds") */
/* *INDENT-OFF* */
static const struct parse_integer_test_ds
{
    const char *haystack;
    uintmax_t expected_result;
    gboolean invalid;
} parse_integer_test_ds[] =
{
    {
        /* too big */
        "99999999999999999999999999999999999999999999999999999999999999999999",
        0,
        TRUE
    },
    {
        "x",
        0,
        TRUE
    },
    {
        "9x",
        0,
        TRUE
    },
    {
        "1",
        1,
        FALSE
    },
    {
        "-1",
        0,
        TRUE
    },
    {
        "1k",
        1024,
        FALSE
    },
    {
        "1K",
        1024,
        FALSE
    },
    {
        "1M",
        1024 * 1024,
        FALSE
    },
    {
        "1m",
        0,
        TRUE
    },
    {
        "64M",
        64 * 1024 * 1024,
        FALSE
    },
    {
        "1G",
        1 * 1024 * 1024 * 1024,
        FALSE
    }
};
/* *INDENT-ON* */

/* @Test(dataSource = "parse_integer_test_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (parse_integer_test, parse_integer_test_ds)
/* *INDENT-ON* */
{
    /* given */
    uintmax_t actual_result;
    gboolean invalid = FALSE;

    /* when */
    actual_result = parse_integer (data->haystack, &invalid);

    /* then */
    ck_assert_msg (invalid == data->invalid && actual_result == data->expected_result,
                   "actual ( %" PRIuMAX ") not equal to\nexpected (%" PRIuMAX ")",
                   actual_result, data->expected_result);
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
    mctest_add_parameterized_test (tc_core, parse_integer_test, parse_integer_test_ds);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
