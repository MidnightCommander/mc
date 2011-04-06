/* libmc - checks for processing esc sequences in replace string

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

#define TEST_SUITE_NAME "lib/search/regex"

#include <check.h>

#include "regex.c" /* for testing static functions*/

/* --------------------------------------------------------------------------------------------- */
#define test_helper_check_valid_data( a, b, c, d, e, f, g, h ) \
{ \
    fail_unless( a == b, "ret_value != %s", (b) ? "TRUE": "FALSE" ); \
    fail_unless( c == d, "skip_len(%d) != %d", c, d ); \
    if (f!=0) fail_unless( e == f, "ret(%d) != %d", e, f ); \
    fail_unless( g == h, "next_char('%c':%d) != %d", g, g, h ); \
} \

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_regex_replace_esc_seq_prepare_valid)
{
    GString *replace_str;
    gsize skip_len;
    int ret;
    char next_char;

    replace_str = g_string_new("bla-bla\\{123}bla-bla\\xabc234 bla-bla\\x{456abcd}bla-bla\\xtre\\n\\t\\v\\b\\r\\f\\a");

    /* \\{123} */
    skip_len=0;
    test_helper_check_valid_data(
        mc_search_regex__replace_handle_esc_seq ( replace_str, 7, &skip_len, &ret, &next_char ), FALSE,
        skip_len, 6,
        ret, REPLACE_PREPARE_T_ESCAPE_SEQ,
        next_char, '{'
    );

    /* \\xab */
    skip_len=0;
    test_helper_check_valid_data(
        mc_search_regex__replace_handle_esc_seq ( replace_str, 20, &skip_len, &ret, &next_char ), FALSE,
        skip_len, 4,
        ret, REPLACE_PREPARE_T_ESCAPE_SEQ,
        next_char, 'b'
    );

    /* \\x{456abcd}  */
    skip_len=0;
    test_helper_check_valid_data(
        mc_search_regex__replace_handle_esc_seq ( replace_str, 36, &skip_len, &ret, &next_char ), FALSE,
        skip_len, 11,
        ret, REPLACE_PREPARE_T_ESCAPE_SEQ,
        next_char, '{'
    );

    /* \\xtre */
    skip_len=0;
    test_helper_check_valid_data(
        mc_search_regex__replace_handle_esc_seq ( replace_str, 54, &skip_len, &ret, &next_char ), FALSE,
        skip_len, 2,
        ret, REPLACE_PREPARE_T_NOTHING_SPECIAL,
        next_char, 't'
    );

    /* \\n */
    skip_len=0;
    test_helper_check_valid_data(
        mc_search_regex__replace_handle_esc_seq ( replace_str, 59, &skip_len, &ret, &next_char ), FALSE,
        skip_len, 2,
        ret, REPLACE_PREPARE_T_ESCAPE_SEQ,
        next_char, 'n'
    );

    /* \\t */
    skip_len=0;
    test_helper_check_valid_data(
        mc_search_regex__replace_handle_esc_seq ( replace_str, 61, &skip_len, &ret, &next_char ), FALSE,
        skip_len, 2,
        ret, REPLACE_PREPARE_T_ESCAPE_SEQ,
        next_char, 't'
    );

    /* \\v */
    skip_len=0;
    test_helper_check_valid_data(
        mc_search_regex__replace_handle_esc_seq ( replace_str, 63, &skip_len, &ret, &next_char ), FALSE,
        skip_len, 2,
        ret, REPLACE_PREPARE_T_ESCAPE_SEQ,
        next_char, 'v'
    );

    /* \\b */
    skip_len=0;
    test_helper_check_valid_data(
        mc_search_regex__replace_handle_esc_seq ( replace_str, 65, &skip_len, &ret, &next_char ), FALSE,
        skip_len, 2,
        ret, REPLACE_PREPARE_T_ESCAPE_SEQ,
        next_char, 'b'
    );

    /* \\r */
    skip_len=0;
    test_helper_check_valid_data(
        mc_search_regex__replace_handle_esc_seq ( replace_str, 67, &skip_len, &ret, &next_char ), FALSE,
        skip_len, 2,
        ret, REPLACE_PREPARE_T_ESCAPE_SEQ,
        next_char, 'r'
    );

    /* \\f */
    skip_len=0;
    test_helper_check_valid_data(
        mc_search_regex__replace_handle_esc_seq ( replace_str, 69, &skip_len, &ret, &next_char ), FALSE,
        skip_len, 2,
        ret, REPLACE_PREPARE_T_ESCAPE_SEQ,
        next_char, 'f'
    );

    /* \\a */
    skip_len=0;
    test_helper_check_valid_data(
        mc_search_regex__replace_handle_esc_seq ( replace_str, 71, &skip_len, &ret, &next_char ), FALSE,
        skip_len, 2,
        ret, REPLACE_PREPARE_T_ESCAPE_SEQ,
        next_char, 'a'
    );

    g_string_free(replace_str, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_regex_replace_esc_seq_prepare_invalid)
{

    GString *replace_str;
    gsize skip_len;
    int ret;
    char next_char;

    replace_str = g_string_new("\\{123 \\x{qwerty} \\12} \\x{456a-bcd}bla-bla\\satre");

    /* \\{123 */
    skip_len=0;
    test_helper_check_valid_data(
        mc_search_regex__replace_handle_esc_seq ( replace_str, 0, &skip_len, &ret, &next_char ), TRUE,
        skip_len, 0,
        0, 0,
        next_char, '{'
    );

    /* \\x{qwerty} */
    skip_len=0;
    test_helper_check_valid_data(
        mc_search_regex__replace_handle_esc_seq ( replace_str, 6, &skip_len, &ret, &next_char ), TRUE,
        skip_len, 0,
        0, 0,
        next_char, 'x'
    );
    /* \\12} */
    skip_len=0;
    test_helper_check_valid_data(
        mc_search_regex__replace_handle_esc_seq ( replace_str, 17, &skip_len, &ret, &next_char ), TRUE,
        skip_len, 0,
        0, 0,
        next_char, '1'
    );

    /* \\x{456a-bcd} */
    skip_len=0;
    test_helper_check_valid_data(
        mc_search_regex__replace_handle_esc_seq ( replace_str, 22, &skip_len, &ret, &next_char ), TRUE,
        skip_len, 0,
        0, 0,
        next_char, 'x'
    );

    /* \\satre */
    skip_len=0;
    test_helper_check_valid_data(
        mc_search_regex__replace_handle_esc_seq ( replace_str, 41, &skip_len, &ret, &next_char ), TRUE,
        skip_len, 0,
        0, 0,
        next_char, 's'
    );

    g_string_free(replace_str, TRUE);
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
    tcase_add_test (tc_core, test_regex_replace_esc_seq_prepare_valid);
    tcase_add_test (tc_core, test_regex_replace_esc_seq_prepare_invalid);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
