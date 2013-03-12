/*
   libmc - checks for processing esc sequences in replace string

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

#define TEST_SUITE_NAME "lib/search/regex"

#include "tests/mctest.h"

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

/* @DataSource("test_regex_replace_esc_seq_prepare_ds") */
/* *INDENT-OFF* */
static const struct test_regex_replace_esc_seq_prepare_ds
{
    const char *input_string;
    const size_t input_pos;

    const gboolean expected_result;
    const gsize expected_skipped_len;
    const int expected_flags;
} test_regex_replace_esc_seq_prepare_ds[] =
{
    { /* 0. \\{123} */
        "bla-bla\\{123}bla-bla\\xabc234 bla-bla\\x{456abcd}bla-bla\\xtre\\n\\t\\v\\b\\r\\f\\a",
        7,
        FALSE,
        6,
        REPLACE_PREPARE_T_ESCAPE_SEQ
    },
    { /* 1. \\xab */
        "bla-bla\\{123}bla-bla\\xabc234 bla-bla\\x{456abcd}bla-bla\\xtre\\n\\t\\v\\b\\r\\f\\a",
        20,
        FALSE,
        4,
        REPLACE_PREPARE_T_ESCAPE_SEQ
    },
    { /* 2. \\x{456abcd}  */
        "bla-bla\\{123}bla-bla\\xabc234 bla-bla\\x{456abcd}bla-bla\\xtre\\n\\t\\v\\b\\r\\f\\a",
        36,
        FALSE,
        11,
        REPLACE_PREPARE_T_ESCAPE_SEQ
    },
    { /* 3. \\xtre */
        "bla-bla\\{123}bla-bla\\xabc234 bla-bla\\x{456abcd}bla-bla\\xtre\\n\\t\\v\\b\\r\\f\\a",
        54,
        FALSE,
        2,
        REPLACE_PREPARE_T_NOTHING_SPECIAL
    },
    { /* 4. \\n */
        "bla-bla\\{123}bla-bla\\xabc234 bla-bla\\x{456abcd}bla-bla\\xtre\\n\\t\\v\\b\\r\\f\\a",
        59,
        FALSE,
        2,
        REPLACE_PREPARE_T_ESCAPE_SEQ
    },
    { /* 5. \\t */
        "bla-bla\\{123}bla-bla\\xabc234 bla-bla\\x{456abcd}bla-bla\\xtre\\n\\t\\v\\b\\r\\f\\a",
        61,
        FALSE,
        2,
        REPLACE_PREPARE_T_ESCAPE_SEQ
    },
    { /* 6. \\v */
        "bla-bla\\{123}bla-bla\\xabc234 bla-bla\\x{456abcd}bla-bla\\xtre\\n\\t\\v\\b\\r\\f\\a",
        63,
        FALSE,
        2,
        REPLACE_PREPARE_T_ESCAPE_SEQ
    },
    { /* 7. \\b */
        "bla-bla\\{123}bla-bla\\xabc234 bla-bla\\x{456abcd}bla-bla\\xtre\\n\\t\\v\\b\\r\\f\\a",
        65,
        FALSE,
        2,
        REPLACE_PREPARE_T_ESCAPE_SEQ
    },
    { /* 8. \\r */
        "bla-bla\\{123}bla-bla\\xabc234 bla-bla\\x{456abcd}bla-bla\\xtre\\n\\t\\v\\b\\r\\f\\a",
        67,
        FALSE,
        2,
        REPLACE_PREPARE_T_ESCAPE_SEQ
    },
    { /* 9. \\f */
        "bla-bla\\{123}bla-bla\\xabc234 bla-bla\\x{456abcd}bla-bla\\xtre\\n\\t\\v\\b\\r\\f\\a",
        69,
        FALSE,
        2,
        REPLACE_PREPARE_T_ESCAPE_SEQ
    },
    {  /* 10. \\a */
        "bla-bla\\{123}bla-bla\\xabc234 bla-bla\\x{456abcd}bla-bla\\xtre\\n\\t\\v\\b\\r\\f\\a",
        71,
        FALSE,
        2,
        REPLACE_PREPARE_T_ESCAPE_SEQ
    },
    { /* 11. \\{123 */
        "\\{123 \\x{qwerty} \\12} \\x{456a-bcd}bla-bla\\satre",
        0,
        TRUE,
        5,
        REPLACE_PREPARE_T_NOTHING_SPECIAL
    },
    { /* 12. \\x{qwerty} */
        "\\{123 \\x{qwerty} \\12} \\x{456a-bcd}bla-bla\\satre",
        6,
        TRUE,
        3,
        REPLACE_PREPARE_T_NOTHING_SPECIAL
    },
    { /* 13. \\12} */
        "\\{123 \\x{qwerty} \\12} \\x{456a-bcd}bla-bla\\satre",
        17,
        TRUE,
        0,
        0
    },
    { /* 14. \\x{456a-bcd} */
        "\\{123 \\x{qwerty} \\12} \\x{456a-bcd}bla-bla\\satre",
        22,
        TRUE,
        7,
        REPLACE_PREPARE_T_NOTHING_SPECIAL
    },
    { /* 15. \\satre */
        "\\{123 \\x{qwerty} \\12} \\x{456a-bcd}bla-bla\\satre",
        41,
        TRUE,
        0,
        0
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_regex_replace_esc_seq_prepare_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_regex_replace_esc_seq_prepare, test_regex_replace_esc_seq_prepare_ds)
/* *INDENT-ON* */
{
    /* given */
    GString *replace_str;
    gsize actual_skipped_len = 0;
    int actual_flags = 0;
    gboolean actual_result;

    replace_str = g_string_new (data->input_string);

    /* when */
    actual_result =
        mc_search_regex__replace_handle_esc_seq (replace_str, data->input_pos, &actual_skipped_len,
                                                 &actual_flags);

    /* then */
    mctest_assert_int_eq (actual_result, data->expected_result);
    mctest_assert_int_eq (actual_skipped_len, data->expected_skipped_len);
    mctest_assert_int_eq (actual_flags, data->expected_flags);

    g_string_free (replace_str, TRUE);
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

    /* Add new tests here: *************** */
    mctest_add_parameterized_test (tc_core, test_regex_replace_esc_seq_prepare,
                                   test_regex_replace_esc_seq_prepare_ds);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
