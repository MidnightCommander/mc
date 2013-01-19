/*
   libmc - checks for processing esc sequences in replace string

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

#define TEST_SUITE_NAME "lib/search/regex"

#include <config.h>

#include <check.h>

#include "regex.c"              /* for testing static functions */

/* --------------------------------------------------------------------------------------------- */
#define test_helper_valid_data(from, etalon, dest_str, replace_flags, utf) { \
    dest_str = g_string_new(""); \
    mc_search_regex__process_escape_sequence (dest_str, from, -1, &replace_flags, utf); \
    fail_if (strcmp(dest_str->str, etalon), "dest_str(%s) != %s", dest_str->str, etalon); \
    g_string_free(dest_str, TRUE); \
}

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */
START_TEST (test_regex_process_escape_sequence_valid)
/* *INDENT-ON* */
{
    GString *dest_str;
    replace_transform_type_t replace_flags = REPLACE_T_NO_TRANSFORM;

    test_helper_valid_data ("{101}", "A", dest_str, replace_flags, FALSE);
    test_helper_valid_data ("x42", "B", dest_str, replace_flags, FALSE);
    test_helper_valid_data ("x{444}", "D", dest_str, replace_flags, FALSE);
    test_helper_valid_data ("x{444}", "ф", dest_str, replace_flags, TRUE);

    test_helper_valid_data ("n", "\n", dest_str, replace_flags, FALSE);
    test_helper_valid_data ("t", "\t", dest_str, replace_flags, FALSE);
    test_helper_valid_data ("v", "\v", dest_str, replace_flags, FALSE);
    test_helper_valid_data ("b", "\b", dest_str, replace_flags, FALSE);
    test_helper_valid_data ("r", "\r", dest_str, replace_flags, FALSE);
    test_helper_valid_data ("f", "\f", dest_str, replace_flags, FALSE);
    test_helper_valid_data ("a", "\a", dest_str, replace_flags, FALSE);
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

    /* Add new tests here: *************** */
    tcase_add_test (tc_core, test_regex_process_escape_sequence_valid);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
