/*
   lib/strutil - tests for lib/strutil/str_verscmp function.
   Testcases are taken from Gnulib.

   Copyright (C) 2020
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

#define TEST_SUITE_NAME "/lib/strutil"

#include "tests/mctest.h"

#include "lib/strutil.h"

/* --------------------------------------------------------------------------------------------- */

/* From glibc bug 9913 */
static char const a[] = "B0075022800016.gbp.corp.com";
static char const b[] = "B007502280067.gbp.corp.com";
static char const c[] = "B007502357019.GBP.CORP.COM";

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

static int
sign (int n)
{
    return ((n < 0) ? -1 : (n == 0) ? 0 : 1);
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("str_verscmp_test_ds") */
/* *INDENT-OFF* */
static const struct str_verscmp_test_struct
{
    const char *s1;
    const char *s2;
    int expected_result;
} str_verscmp_test_ds[] =
{
    { "", "", 0 },
    { "a", "a", 0 },
    { "a", "b", -1 },
    { "b", "a", 1 },
    { "000", "00", -1 },
    { "00", "000", 1 },
    { "a0", "a", 1 },
    { "00", "01", -1 },
    { "01", "010", -1 },
    { "010", "09", -1 },
    { "09", "0", -1 },
    { "9", "10", -1 },
    { "0a", "0", 1 },
    /* From glibc bug 9913 */
    { a, b, -1 },
    { b, c, -1 },
    { a, c, -1 },
    { b, a, 1 },
    { c, b, 1 },
    { c, a, 1 }
};
/* *INDENT-ON* */

/* @Test(dataSource = "str_verscmp_test_ds") */
/* *INDENT-OFF* */
START_TEST (str_verscmp_test)
/* *INDENT-ON* */
{
    /* given */
    int actual_result;
    const struct str_verscmp_test_struct *data = &str_verscmp_test_ds[_i];

    /* when */
    actual_result = str_verscmp (data->s1, data->s2);

    /* then */
    mctest_assert_int_eq (sign (actual_result), sign (data->expected_result));
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
    mctest_add_parameterized_test (tc_core, str_verscmp_test, str_verscmp_test_ds);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "str_verscmp.log");
    srunner_run_all (sr, CK_ENV);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* --------------------------------------------------------------------------------------------- */
