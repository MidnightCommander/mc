/*
   lib/widget - tests for hotkey comparison

   Copyright (C) 2019-2024
   Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2019

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

#include "lib/widget.h"

#define C(x) ((char *) x)

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

/* @DataSource("test_hotkey_equal_ds") */
/* *INDENT-OFF* */
static const struct test_hotkey_equal_ds
{
    const hotkey_t hotkey1;
    const hotkey_t hotkey2;
    gboolean expected_result;
} test_hotkey_equal_ds[] =
{
    /* 0 */
    {
        { .start = C ("abc"), .hotkey = NULL, .end = NULL },
        { .start = C ("abc"), .hotkey = NULL, .end = NULL },
        TRUE
    },
    /* 1 */
    {
        { .start = C (""), .hotkey = C (""), .end = C ("") },
        { .start = C (""), .hotkey = C (""), .end = C ("") },
        TRUE
    },
    /* 2 */
    {
        { .start = C ("abc"), .hotkey = C ("d"), .end = C ("efg") },
        { .start = C ("abc"), .hotkey = C ("d"), .end = C ("efg") },
        TRUE
    },
    /* 3 */
    {
        { .start = C ("abc"), .hotkey = NULL, .end = C ("efg") },
        { .start = C ("abc"), .hotkey = NULL, .end = C ("efg") },
        TRUE
    },
    /* 4 */
    {
        { .start = C ("abc"), .hotkey = C ("d"), .end = NULL },
        { .start = C ("abc"), .hotkey = C ("d"), .end = NULL },
        TRUE
    },
    /* 5 */
    {
        { .start = C ("abc"), .hotkey = C ("d"), .end = C ("efg") },
        { .start = C ("_bc"), .hotkey = C ("d"), .end = C ("efg") },
        FALSE
    },
    /* 6 */
    {
        { .start = C ("abc"), .hotkey = C ("d"), .end = C ("efg") },
        { .start = C ("abc"), .hotkey = C ("_"), .end = C ("efg") },
        FALSE
    },
    /* 7 */
    {
        { .start = C ("abc"), .hotkey = C ("d"), .end = C ("efg") },
        { .start = C ("abc"), .hotkey = C ("d"), .end = C ("_fg") },
        FALSE
    },
    /* 8 */
    {
        { .start = C ("abc"), .hotkey = C ("d"), .end = C ("efg") },
        { .start = C ("adc"), .hotkey = NULL,    .end = C ("efg") },
        FALSE
    },
    /* 9 */
    {
        { .start = C ("abc"), .hotkey = C ("d"), .end = C ("efg") },
        { .start = C ("abc"), .hotkey = C ("d"), .end = NULL      },
        FALSE
    },
    /* 10 */
    {
        { .start = C ("abc"), .hotkey = C ("d"), .end = C ("efg") },
        { .start = C ("abc"), .hotkey = NULL,    .end = NULL      },
        FALSE
    }
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_hotkey_equal_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_hotkey_equal,
                         test_hotkey_equal_ds)
/* *INDENT-ON* */
{
    /* given */
    gboolean result;

    /* when */
    result = hotkey_equal (data->hotkey1, data->hotkey2);

    /* then */
    ck_assert_int_eq (result, data->expected_result);
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
    mctest_add_parameterized_test (tc_core, test_hotkey_equal, test_hotkey_equal_ds);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
