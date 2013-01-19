/*
   lib/vfs - mc_build_filename() function testing

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

#define TEST_SUITE_NAME "/lib"

#include "tests/mctest.h"

#include <stdio.h>

#include "lib/strutil.h"
#include "lib/util.h"

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

static char *
run_mc_build_filename (int iteration)
{
    switch (iteration)
    {
    case 0:
        return mc_build_filename ("test", "path", NULL);
    case 1:
        return mc_build_filename ("/test", "path/", NULL);
    case 2:
        return mc_build_filename ("/test", "pa/th", NULL);
    case 3:
        return mc_build_filename ("/test", "#vfsprefix:", "path  ", NULL);
    case 4:
        return mc_build_filename ("/test", "vfsprefix://", "path  ", NULL);
    case 5:
        return mc_build_filename ("/test", "vfs/../prefix:///", "p\\///ath", NULL);
    case 6:
        return mc_build_filename ("/test", "path", "..", "/test", "path/", NULL);
    case 7:
        return mc_build_filename ("", "path", NULL);
    case 8:
        return mc_build_filename ("", "/path", NULL);
    case 9:
        return mc_build_filename ("path", "", NULL);
    case 10:
        return mc_build_filename ("/path", "", NULL);
    case 11:
        return mc_build_filename ("pa", "", "th", NULL);
    case 12:
        return mc_build_filename ("/pa", "", "/th", NULL);
    }
    return NULL;
}

/* @DataSource("test_mc_build_filename_ds") */
/* *INDENT-OFF* */
static const struct test_mc_build_filename_ds
{
    const char *expected_result;
} test_mc_build_filename_ds[] =
{
    {"test/path"},
    {"/test/path"},
    {"/test/pa/th"},
    {"/test/#vfsprefix:/path  "},
    {"/test/vfsprefix://path  "},
    {"/test/prefix://p\\/ath"},
    {"/test/test/path"},
    {"path"},
    {"path"},
    {"path"},
    {"/path"},
    {"pa/th"},
    {"/pa/th"},
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_mc_build_filename_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_mc_build_filename, test_mc_build_filename_ds)
/* *INDENT-ON* */
{
    /* given */
    char *actual_result;

    /* when */
    actual_result = run_mc_build_filename (_i);

    /* then */
    mctest_assert_str_eq (actual_result, data->expected_result);

    g_free (actual_result);
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

    tcase_add_checked_fixture (tc_core, setup, teardown);

    /* Add new tests here: *************** */
    mctest_add_parameterized_test (tc_core, test_mc_build_filename, test_mc_build_filename_ds);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "mc_build_filename.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
