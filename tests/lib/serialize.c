/*
   lib/vfs - common serialize/deserialize functions

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

#define TEST_SUITE_NAME "/lib"

#include "tests/mctest.h"

#include "lib/strutil.h"
#include "lib/serialize.h"

static GError *error = NULL;

static const char *deserialize_input_value1 =
    "g6:group1p6:param1v10:some valuep6:param2v11:some value "
    "g6:group2p6:param1v4:truep6:param2v6:123456"
    "g6:group3p6:param1v11:::bla-bla::p6:param2v31:bla-:p1:w:v2:12:g3:123:bla-bla\n"
    "g6:group4p6:param1v5:falsep6:param2v6:654321";

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    str_init_strings (NULL);
    error = NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    g_clear_error (&error);
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_serialize_ds") */
/* *INDENT-OFF* */
static const struct test_serialize_ds
{
    const char input_char_prefix;
    const char *input_string;
    const char *expected_result;
} test_serialize_ds[] =
{
    {
        's',
        "some test string",
        "s16:some test string"
    },
    {
        'a',
        "some test test test string",
        "a26:some test test test string"
    },
};
/* *INDENT-ON* */
/* @Test(dataSource = "test_serialize_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_serialize, test_serialize_ds)
/* *INDENT-ON* */
{
    /* given */
    char *actual_result;

    /* when */
    actual_result = mc_serialize_str (data->input_char_prefix, data->input_string, &error);

    /* then */
    mctest_assert_str_eq (actual_result, data->expected_result);

    g_free (actual_result);

}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_deserialize_incorrect_ds") */
/* *INDENT-OFF* */
static const struct test_deserialize_incorrect_ds
{
    const char input_char_prefix;
    const char *input_string;
    const int  expected_error_code;
    const char *expected_error_string;
} test_deserialize_incorrect_ds[] =
{
    {
        's',
        NULL,
        -1,
        "mc_serialize_str(): Input data is NULL or empty."
    },
    {
        's',
        "incorrect string",
        -2,
        "mc_serialize_str(): String prefix doesn't equal to 's'"
    },
    {
        's',
        "s12345string without delimiter",
        -3,
        "mc_serialize_str(): Length delimiter ':' doesn't exists"
    },
    {
        's',
        "s1234567890123456789012345678901234567890123456789012345678901234567890:too big number",
        -3,
        "mc_serialize_str(): Too big string length"
    },
    {
        's',
        "s500:actual string length less that specified length",
        -3,
        "mc_serialize_str(): Specified data length (500) is greater than actual data length (47)"
    },
};
/* *INDENT-ON* */
/* @Test(dataSource = "test_deserialize_incorrect_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_deserialize_incorrect, test_deserialize_incorrect_ds)
/* *INDENT-ON* */
{
    /* given */
    char *actual_result;

    /* when */
    actual_result = mc_deserialize_str (data->input_char_prefix, data->input_string, &error);

    /* then */
    mctest_assert_null (actual_result);

    mctest_assert_int_eq (error->code, data->expected_error_code);
    mctest_assert_str_eq (error->message, data->expected_error_string);
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_deserialize_ds") */
/* *INDENT-OFF* */
static const struct test_deserialize_ds
{
    const char input_char_prefix;
    const char *input_string;
    const char *expected_result;
} test_deserialize_ds[] =
{
    {
        's',
        "s10:actual string length great that specified length",
        "actual str"
    },
    {
        'r',
        "r21:The right test string",
        "The right test string"
    },
};
/* *INDENT-ON* */
/* @Test(dataSource = "test_deserialize_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_deserialize, test_deserialize_ds)
/* *INDENT-ON* */
{
    /* given */
    char *actual_result;

    /* when */
    actual_result = mc_deserialize_str (data->input_char_prefix, data->input_string, &error);

    /* then */
    mctest_assert_str_eq (actual_result, data->expected_result);

    g_free (actual_result);
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */
START_TEST (test_serialize_config)
/* *INDENT-ON* */
{
    /* given */
    mc_config_t *test_data;
    char *actual;
    const char *expected_result = "g6:group1p6:param1v10:some valuep6:param2v11:some value "
        "g6:group2p6:param1v4:truep6:param2v6:123456"
        "g6:group3p6:param1v11:::bla-bla::p6:param2v31:bla-:p1:w:v2:12:g3:123:bla-bla\n"
        "g6:group4p6:param1v5:falsep6:param2v6:654321";

    test_data = mc_config_init (NULL, FALSE);

    mc_config_set_string_raw (test_data, "group1", "param1", "some value");
    mc_config_set_string (test_data, "group1", "param2", "some value ");

    mc_config_set_bool (test_data, "group2", "param1", TRUE);
    mc_config_set_int (test_data, "group2", "param2", 123456);

    mc_config_set_string_raw (test_data, "group3", "param1", "::bla-bla::");
    mc_config_set_string (test_data, "group3", "param2", "bla-:p1:w:v2:12:g3:123:bla-bla\n");

    mc_config_set_bool (test_data, "group4", "param1", FALSE);
    mc_config_set_int (test_data, "group4", "param2", 654321);

    /* when */
    actual = mc_serialize_config (test_data, &error);
    mc_config_deinit (test_data);

    /* then */
    mctest_assert_not_null (actual);
    mctest_assert_str_eq (actual, expected_result);

    g_free (actual);
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */
/* @DataSource("test_deserialize_config_incorrect_ds") */
/* *INDENT-OFF* */
static const struct test_deserialize_config_incorrect_ds
{
    const char *input_string;
    const int  expected_error_code;
    const char *expected_error_string;
} test_deserialize_config_incorrect_ds[] =
{
    {
        "g123error in group name",
        -3,
        "mc_deserialize_config() at 1: mc_serialize_str(): Length delimiter ':' doesn't exists"
    },
    {
        "p6:param1v10:some valuep6:param2v11:some value ",
        -2,
        "mc_deserialize_config() at 1: mc_serialize_str(): String prefix doesn't equal to 'g'"
    },
    {
        "g6:group1v10:some valuep6:param2v11:some value ",
        -2,
        "mc_deserialize_config() at 10: mc_serialize_str(): String prefix doesn't equal to 'p'"
    },
    {
        "g6:group1p6000:param2v11:some value ",
        -3,
        "mc_deserialize_config() at 10: mc_serialize_str(): Specified data length (6000) is greater than actual data length (21)"
    },
};
/* *INDENT-ON* */
/* @Test(dataSource = "test_deserialize_config_incorrect_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_deserialize_config_incorrect, test_deserialize_config_incorrect_ds)
/* *INDENT-ON* */
{
    /* given */
    mc_config_t *actual_result;

    /* when */
    actual_result = mc_deserialize_config (data->input_string, &error);

    /* then */
    mctest_assert_null (actual_result);

    mctest_assert_int_eq (error->code, data->expected_error_code);
    mctest_assert_str_eq (error->message, data->expected_error_string);
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */
START_TEST (test_deserialize_config)
/* *INDENT-ON* */
{
    /* given */
    mc_config_t *actual;
    char *actual_value;

    /* when */
    actual = mc_deserialize_config (deserialize_input_value1, &error);

    /* then */
    mctest_assert_not_null (actual);

    actual_value = mc_config_get_string_raw (actual, "group1", "param1", "");
    mctest_assert_str_eq (actual_value, "some value");
    g_free (actual_value);

    actual_value = mc_config_get_string (actual, "group1", "param2", "");
    mctest_assert_str_eq (actual_value, "some value ");
    g_free (actual_value);

    mctest_assert_int_eq (mc_config_get_bool (actual, "group2", "param1", FALSE), TRUE);

    mctest_assert_int_eq (mc_config_get_int (actual, "group2", "param2", 0), 123456);

    actual_value = mc_config_get_string_raw (actual, "group3", "param1", "");
    mctest_assert_str_eq (actual_value, "::bla-bla::");
    g_free (actual_value);

    actual_value = mc_config_get_string (actual, "group3", "param2", "");
    mctest_assert_str_eq (actual_value, "bla-:p1:w:v2:12:g3:123:bla-bla\n");
    g_free (actual_value);

    mctest_assert_int_eq (mc_config_get_bool (actual, "group4", "param1", TRUE), FALSE);

    mctest_assert_int_eq (mc_config_get_int (actual, "group4", "param2", 0), 654321);

    mc_config_deinit (actual);
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

#undef input_value
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
    mctest_add_parameterized_test (tc_core, test_serialize, test_serialize_ds);

    mctest_add_parameterized_test (tc_core, test_deserialize_incorrect,
                                   test_deserialize_incorrect_ds);

    mctest_add_parameterized_test (tc_core, test_deserialize, test_deserialize_ds);

    tcase_add_test (tc_core, test_serialize_config);

    mctest_add_parameterized_test (tc_core, test_deserialize_config_incorrect,
                                   test_deserialize_config_incorrect_ds);

    tcase_add_test (tc_core, test_deserialize_config);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "serialize.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
