/* lib - canonicalize path

   Copyright (C) 2011 Free Software Foundation, Inc.

   Written by:
    Slava Zanko <slavazanko@gmail.com>, 2011

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#define TEST_SUITE_NAME "/lib"

#include <check.h>


#include "lib/global.h"
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

#define check_canonicalize( input, etalon ) { \
    path = g_strdup(input); \
    canonicalize_pathname (path); \
    fail_unless ( \
        strcmp(path, etalon) == 0, \
        "\nactual value (%s)\nnot equal to etalon (%s)", path, etalon \
    ); \
    g_free(path); \
}

START_TEST (test_canonicalize_path)
{
    char *path;

    /* UNC path */
    check_canonicalize ("//some_server/ww", "//some_server/ww");

    /* join slashes */
    check_canonicalize ("///some_server/////////ww", "/some_server/ww");

    /* Collapse "/./" -> "/" */
    check_canonicalize ("//some_server//.///////ww/./././.", "//some_server/ww");

    /* Remove leading "./" */
    check_canonicalize ("./some_server/ww", "some_server/ww");

    /* some/.. -> . */
    check_canonicalize ("some_server/..", ".");

    /* Collapse "/.." with the previous part of path */
    check_canonicalize ("/some_server/ww/some_server/../ww/../some_server/..//ww/some_server/ww", "/some_server/ww/ww/some_server/ww");

    /* URI style */
    check_canonicalize ("/some_server/ww/ftp://user:pass@host.net/path/", "/some_server/ww/ftp://user:pass@host.net/path");

    check_canonicalize ("/some_server/ww/ftp://user:pass@host.net/path/../../", "/some_server/ww");

    check_canonicalize ("ftp://user:pass@host.net/path/../../", ".");

    check_canonicalize ("ftp://user/../../", "..");

}
END_TEST

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
    tcase_add_test (tc_core, test_canonicalize_path);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "canonicalize_pathname.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
