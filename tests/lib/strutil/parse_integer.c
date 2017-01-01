/*
   lib/strutil - tests for lib/strutil/parse_integer function.

   Copyright (C) 2013-2017
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

/* @DataSource("str_replace_all_test_ds") */
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

/* @Test(dataSource = "str_replace_all_test_ds") */
/* *INDENT-OFF* */
START_TEST (parse_integer_test)
/* *INDENT-ON* */
{
    /* given */
    uintmax_t actual_result;
    gboolean invalid = FALSE;
    const struct parse_integer_test_ds *data = &parse_integer_test_ds[_i];

    /* when */
    actual_result = parse_integer (data->haystack, &invalid);

    /* then */
    fail_unless (invalid == data->invalid && actual_result == data->expected_result,
                 "actial ( %" PRIuMAX ") not equal to\nexpected (%" PRIuMAX ")",
                 actual_result, data->expected_result);
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
    tcase_add_loop_test (tc_core, parse_integer_test, 0, G_N_ELEMENTS (parse_integer_test_ds));
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "parse_integer.log");
    srunner_run_all (sr, CK_ENV);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* --------------------------------------------------------------------------------------------- */
