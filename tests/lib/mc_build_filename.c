/*
   lib/vfs - mc_build_filename() function testing

   Copyright (C) 2011
   The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2011

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

#include <config.h>

#include <check.h>

#include <stdio.h>

#include "lib/global.h"
#include "lib/strutil.h"
#include "lib/util.h"


static void
setup (void)
{
}

static void
teardown (void)
{
}

/* --------------------------------------------------------------------------------------------- */
#define check_mc_build_filename( inargs, etalon ) \
{ \
    result = mc_build_filename inargs; \
    fail_unless( strcmp (result, etalon) == 0, \
    "\nactial  (%s) not equal to\netalon (%s)", result, etalon); \
    g_free (result); \
}

/* *INDENT-OFF* */
START_TEST (test_mc_build_filename)
/* *INDENT-ON* */
{
    char *result;

    check_mc_build_filename (("test", "path", NULL), "test/path");

    check_mc_build_filename (("/test", "path/", NULL), "/test/path");

    check_mc_build_filename (("/test", "pa/th", NULL), "/test/pa/th");

    check_mc_build_filename (("/test", "#vfsprefix:", "path  ", NULL), "/test/#vfsprefix:/path  ");

    check_mc_build_filename (("/test", "vfsprefix://", "path  ", NULL), "/test/vfsprefix://path  ");

    check_mc_build_filename (("/test", "vfs/../prefix:///", "p\\///ath", NULL),
                             "/test/prefix://p\\/ath");

    check_mc_build_filename (("/test", "path", "..", "/test", "path/", NULL), "/test/test/path");

    check_mc_build_filename (("", "path", NULL), "path");

    check_mc_build_filename (("", "/path", NULL), "path");

    check_mc_build_filename (("path", "", NULL), "path");

    check_mc_build_filename (("/path", "", NULL), "/path");

    check_mc_build_filename (("pa", "", "th", NULL), "pa/th");

    check_mc_build_filename (("/pa", "", "/th", NULL), "/pa/th");
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
    tcase_add_test (tc_core, test_mc_build_filename);
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
