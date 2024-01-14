/*
   lib/strutil - tests for lib/strutil/str_verscmp function.
   Testcases are taken from Gnulib.

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

#define TEST_SUITE_NAME "/lib/strutil"

#include "tests/mctest.h"

#include "lib/strutil.h"
#include "lib/util.h"           /* _GL_CMP() */

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
    return _GL_CMP (n, 0);
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
    ck_assert_int_eq (sign (actual_result), sign (data->expected_result));
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    /* Add new tests here: *************** */
    mctest_add_parameterized_test (tc_core, str_verscmp_test, str_verscmp_test_ds);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
