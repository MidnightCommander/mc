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
#define test_helper_check_valid_data( a, b, c, d, e, f ) \
{ \
    fail_unless( a == b, "ret_value != %s", (b) ? "TRUE": "FALSE" ); \
    fail_unless( c == d, "skip_len(%d) != %d", c, d ); \
    if (f!=0) fail_unless( e == f, "ret(%d) != %d", e, f ); \
}

#define test_helper_handle_esc_seq( pos, r, skip, flag ) \
{ \
    skip_len = 0;\
    test_helper_check_valid_data(\
        mc_search_regex__replace_handle_esc_seq( replace_str, pos, &skip_len, &ret ), r,\
        skip_len, skip,\
        ret, flag\
    ); \
}

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */
START_TEST (test_regex_replace_esc_seq_prepare_valid)
/* *INDENT-ON* */
{
    GString *replace_str;
    gsize skip_len;
    int ret;

    replace_str =
        g_string_new
        ("bla-bla\\{123}bla-bla\\xabc234 bla-bla\\x{456abcd}bla-bla\\xtre\\n\\t\\v\\b\\r\\f\\a");

    test_helper_handle_esc_seq (7, FALSE, 6, REPLACE_PREPARE_T_ESCAPE_SEQ);     /* \\{123} */
    test_helper_handle_esc_seq (20, FALSE, 4, REPLACE_PREPARE_T_ESCAPE_SEQ);    /* \\xab */
    test_helper_handle_esc_seq (36, FALSE, 11, REPLACE_PREPARE_T_ESCAPE_SEQ);   /* \\x{456abcd}  */
    test_helper_handle_esc_seq (54, FALSE, 2, REPLACE_PREPARE_T_NOTHING_SPECIAL);       /* \\xtre */
    test_helper_handle_esc_seq (59, FALSE, 2, REPLACE_PREPARE_T_ESCAPE_SEQ);    /* \\n */
    test_helper_handle_esc_seq (61, FALSE, 2, REPLACE_PREPARE_T_ESCAPE_SEQ);    /* \\t */
    test_helper_handle_esc_seq (63, FALSE, 2, REPLACE_PREPARE_T_ESCAPE_SEQ);    /* \\v */
    test_helper_handle_esc_seq (65, FALSE, 2, REPLACE_PREPARE_T_ESCAPE_SEQ);    /* \\b */
    test_helper_handle_esc_seq (67, FALSE, 2, REPLACE_PREPARE_T_ESCAPE_SEQ);    /* \\r */
    test_helper_handle_esc_seq (69, FALSE, 2, REPLACE_PREPARE_T_ESCAPE_SEQ);    /* \\f */
    test_helper_handle_esc_seq (71, FALSE, 2, REPLACE_PREPARE_T_ESCAPE_SEQ);    /* \\a */

    g_string_free (replace_str, TRUE);
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */
START_TEST (test_regex_replace_esc_seq_prepare_invalid)
/* *INDENT-ON* */
{

    GString *replace_str;
    gsize skip_len;
    int ret;

    replace_str = g_string_new ("\\{123 \\x{qwerty} \\12} \\x{456a-bcd}bla-bla\\satre");

    test_helper_handle_esc_seq (0, TRUE, 5, REPLACE_PREPARE_T_NOTHING_SPECIAL); /* \\{123 */
    test_helper_handle_esc_seq (6, TRUE, 3, REPLACE_PREPARE_T_NOTHING_SPECIAL); /* \\x{qwerty} */
    test_helper_handle_esc_seq (17, TRUE, 0, REPLACE_PREPARE_T_NOTHING_SPECIAL);        /* \\12} */
    test_helper_handle_esc_seq (22, TRUE, 7, REPLACE_PREPARE_T_NOTHING_SPECIAL);        /* \\x{456a-bcd} */
    test_helper_handle_esc_seq (41, TRUE, 0, REPLACE_PREPARE_T_NOTHING_SPECIAL);        /* \\satre */

    g_string_free (replace_str, TRUE);
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
