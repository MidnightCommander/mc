/*
   libmc - checks for processing esc sequences in replace string

   Copyright (C) 2011
   The Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2011

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

#define TEST_SUITE_NAME "lib/search/glob"

#include <config.h>

#include <check.h>

#include "glob.c" /* for testing static functions */

/* --------------------------------------------------------------------------------------------- */

static void
test_helper_valid_data (const char *from, const char *etalon)
{
    GString *dest_str;

    dest_str = mc_search__translate_replace_glob_to_regex (from);
    fail_if (strcmp (dest_str->str, etalon), "dest_str(%s) != %s", dest_str->str, etalon);
    g_string_free (dest_str, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_translate_replace_glob_to_regex)
{
    test_helper_valid_data ("a&a?a", "a\\&a\\1a");
    test_helper_valid_data ("a\\&a?a", "a\\&a\\1a");
    test_helper_valid_data ("a&a\\?a", "a\\&a\\?a");
    test_helper_valid_data ("a\\&a\\?a", "a\\&a\\?a");
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

    /* Add new tests here: *************** */
    tcase_add_test (tc_core, test_translate_replace_glob_to_regex);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
