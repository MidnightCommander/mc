/*
   lib/vfs - x_basename() function testing

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
#define check_x_basename( input, etalon ) \
{ \
    result = x_basename ( input ); \
    fail_unless(strcmp(result, etalon) == 0, \
    "\ninput (%s)\nactial  (%s) not equal to\netalon (%s)", input, result, etalon); \
}

/* *INDENT-OFF* */
START_TEST (test_x_basename)
/* *INDENT-ON* */
{
    const char *result;
    check_x_basename ("/test/path/test2/path2", "path2");

    check_x_basename ("/test/path/test2/path2#vfsprefix", "path2#vfsprefix");

    check_x_basename ("/test/path/test2/path2/vfsprefix://", "path2/vfsprefix://");


    check_x_basename ("/test/path/test2/path2/vfsprefix://subdir", "subdir");

    check_x_basename ("/test/path/test2/path2/vfsprefix://subdir/", "subdir/");

    check_x_basename ("/test/path/test2/path2/vfsprefix://subdir/subdir2", "subdir2");

    check_x_basename ("/test/path/test2/path2/vfsprefix:///", "/");
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
    tcase_add_test (tc_core, test_x_basename);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "x_basename.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
