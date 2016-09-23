/*
   libmc - checks for hex pattern parsing

   Copyright (C) 2016
   Free Software Foundation, Inc.

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

#define TEST_SUITE_NAME "lib/search/hex"

#include "tests/mctest.h"

#include "hex.c"                /* for testing static functions */

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_hex_translate_to_regex_ds") */
/* *INDENT-OFF* */
static const struct test_hex_translate_to_regex_ds
{
    const char *input_value;
    const char *expected_result;
} test_hex_translate_to_regex_ds[] =
{
    {
        /* Simplest case */
        "12 34",
        "\\x12\\x34"
    },
    {
        /* Prefixes (0x, 0X) */
        "0x12 0X34",
        "\\x12\\x34"
    },
    {
        /* Prefix "0" doesn't signify octal! Numbers are always interpreted in hex. */
        "012",
        "\\x12"
    },
    {
        /* Extra whitespace (but not trailing one) */
        "  12  34",
        "\\x12\\x34"
    },
    {
        /* Min/max values */
        "0 ff",
        "\\x00\\xFF"
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_hex_translate_to_regex_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_hex_translate_to_regex, test_hex_translate_to_regex_ds)
/* *INDENT-ON* */
{
    GString *tmp, *dest_str;

    /* given */
    tmp = g_string_new (data->input_value);

    /* when */
    dest_str = mc_search__hex_translate_to_regex (tmp);

    /* then */
    g_string_free (tmp, TRUE);

    mctest_assert_str_eq (dest_str->str, data->expected_result);
    g_string_free (dest_str, TRUE);
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
    mctest_add_parameterized_test (tc_core, test_hex_translate_to_regex,
                                   test_hex_translate_to_regex_ds);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "hex_translate_to_regex.log");
    srunner_run_all (sr, CK_ENV);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* --------------------------------------------------------------------------------------------- */
