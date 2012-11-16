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

#include "lib/global.h"
#include "lib/strutil.h"
#include "lib/search.h"


static mc_search_t *search;

static void
setup (void)
{
    str_init_strings (NULL);
    search = mc_search_new ("<a", -1);
    search->search_type = MC_SEARCH_T_REGEX;
}

static void
teardown (void)
{
    mc_search_free (search);
    str_uninit_strings ();
}


/* --------------------------------------------------------------------------------------------- */
START_TEST (regex_search_test)
{
    // given
    gsize actual_found_len;
    gboolean actual_result;


    // when
    actual_result = mc_search_run (
        search,
        "some string with <a for searching",
        0,
        -1,
        &actual_found_len
    );

    // then
     ck_assert_int_eq (actual_result, TRUE);
     ck_assert_int_eq (actual_found_len, 2);
     ck_assert_int_eq (search->normal_offset, 17);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

static mc_search_cbret_t
regex_search_callback (const void *user_data, gsize char_offset, int *current_char) {
    static const char *data = "some string with <a for searching";

    (void) user_data;

    *current_char = data[char_offset];
    return MC_SEARCH_CB_OK;
}

START_TEST (regex_search_callback_test)
{
    // given
    gsize actual_found_len;
    gboolean actual_result;

    search->search_fn = regex_search_callback;

    // when
    actual_result = mc_search_run (
        search,
        NULL,
        0,
        -1,
        &actual_found_len
    );
    // then
     ck_assert_int_eq (actual_result, TRUE);
     ck_assert_int_eq (actual_found_len, 2);
     ck_assert_int_eq (search->normal_offset, 17);

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
    tcase_add_test (tc_core, regex_search_test);
    tcase_add_test (tc_core, regex_search_callback_test);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
