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


static void
setup (void)
{
    str_init_strings (NULL);
}

static void
teardown (void)
{
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */
#define deserialize_check_incorrect( etalon_code, etalon_str ) { \
    if (actual != NULL) \
    { \
        fail("actual value is '%s', but should be NULL", actual); \
        g_free(actual); \
    } \
    else \
    { \
        fail_unless (error->code == etalon_code && strcmp(error->message, etalon_str) == 0, \
            "\nerror code is %d (should be %d);\nerror message is '%s' (should be '%s')", \
            error->code, etalon_code, error->message, etalon_str); \
        g_clear_error(&error); \
    } \
}

/* *INDENT-OFF* */
START_TEST (test_serialize_deserialize_str)
/* *INDENT-ON* */
{
    GError *error = NULL;
    char *actual;


    actual = mc_serialize_str ('s', "some test string", &error);

    if (actual == NULL)
    {
        fail ("actual value is NULL!\nError code is '%d'; error message is '%s'", error->code,
              error->message);
        g_clear_error (&error);
        return;
    }
    fail_unless (strcmp (actual, "s16:some test string") == 0,
                 "Actual value(%s) doesn't equal to etalon(s16:some test string)", actual);
    g_free (actual);

    actual = mc_deserialize_str ('s', NULL, &error);
    deserialize_check_incorrect (-1, "mc_serialize_str(): Input data is NULL or empty.");

    actual = mc_deserialize_str ('s', "incorrect string", &error);
    deserialize_check_incorrect (-2, "mc_serialize_str(): String prefix doesn't equal to 's'");

    actual = mc_deserialize_str ('s', "s12345string without delimiter", &error);
    deserialize_check_incorrect (-3, "mc_serialize_str(): Length delimiter ':' doesn't exists");

    actual =
        mc_deserialize_str ('s',
                            "s1234567890123456789012345678901234567890123456789012345678901234567890:too big number",
                            &error);
    deserialize_check_incorrect (-3, "mc_serialize_str(): Too big string length");

    actual =
        mc_deserialize_str ('s', "s500:actual string length less that specified length", &error);
    deserialize_check_incorrect (-3,
                                 "mc_serialize_str(): Specified data length (500) is greater than actual data length (47)");

    actual =
        mc_deserialize_str ('s', "s10:actual string length great that specified length", &error);
    fail_unless (actual != NULL
                 && strcmp (actual, "actual str") == 0,
                 "actual (%s) doesn't equal to etalon(actual str)", actual);
    g_free (actual);

    actual = mc_deserialize_str ('s', "s21:The right test string", &error);
    fail_unless (actual != NULL
                 && strcmp (actual, "The right test string") == 0,
                 "actual (%s) doesn't equal to etalon(The right test string)", actual);
    g_free (actual);
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

#define etalon_str "g6:group1p6:param1v10:some valuep6:param2v11:some value " \
   "g6:group2p6:param1v4:truep6:param2v6:123456" \
   "g6:group3p6:param1v11:::bla-bla::p6:param2v31:bla-:p1:w:v2:12:g3:123:bla-bla\n" \
   "g6:group4p6:param1v5:falsep6:param2v6:654321"

/* *INDENT-OFF* */
START_TEST (test_serialize_config)
/* *INDENT-ON* */
{
    mc_config_t *test_data;
    GError *error = NULL;
    char *actual;

    test_data = mc_config_init (NULL, FALSE);

    mc_config_set_string_raw (test_data, "group1", "param1", "some value");
    mc_config_set_string (test_data, "group1", "param2", "some value ");

    mc_config_set_bool (test_data, "group2", "param1", TRUE);
    mc_config_set_int (test_data, "group2", "param2", 123456);

    mc_config_set_string_raw (test_data, "group3", "param1", "::bla-bla::");
    mc_config_set_string (test_data, "group3", "param2", "bla-:p1:w:v2:12:g3:123:bla-bla\n");

    mc_config_set_bool (test_data, "group4", "param1", FALSE);
    mc_config_set_int (test_data, "group4", "param2", 654321);

    actual = mc_serialize_config (test_data, &error);
    mc_config_deinit (test_data);

    if (actual == NULL)
    {
        fail ("actual value is NULL!\nError code is '%d'; error message is '%s'", error->code,
              error->message);
        g_clear_error (&error);
        return;
    }

    fail_unless (strcmp (actual, etalon_str) == 0, "Not equal:\nactual (%s)\netalon (%s)", actual,
                 etalon_str);
    g_free (actual);
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

#undef deserialize_check_incorrect
#define deserialize_check_incorrect( etalon_code, etalon_str ) { \
    if (actual != NULL) \
    { \
        fail("actual value but should be NULL", actual); \
        mc_config_deinit(actual); \
    } \
    else \
    { \
        fail_unless (error->code == etalon_code && strcmp(error->message, etalon_str) == 0, \
            "\nerror code is %d (should be %d);\nerror message is '%s' (should be '%s')", \
            error->code, etalon_code, error->message, etalon_str); \
        g_clear_error(&error); \
    } \
}

/* *INDENT-OFF* */
START_TEST (test_deserialize_config)
/* *INDENT-ON* */
{
    mc_config_t *actual;
    GError *error = NULL;
    char *actual_value;

    actual = mc_deserialize_config ("g123error in group name", &error);
    deserialize_check_incorrect (-3,
                                 "mc_deserialize_config() at 1: mc_serialize_str(): Length delimiter ':' doesn't exists");

    actual = mc_deserialize_config ("p6:param1v10:some valuep6:param2v11:some value ", &error);
    deserialize_check_incorrect (-2,
                                 "mc_deserialize_config() at 1: mc_serialize_str(): String prefix doesn't equal to 'g'");

    actual = mc_deserialize_config ("g6:group1v10:some valuep6:param2v11:some value ", &error);
    deserialize_check_incorrect (-2,
                                 "mc_deserialize_config() at 10: mc_serialize_str(): String prefix doesn't equal to 'p'");

    actual = mc_deserialize_config ("g6:group1p6000:param2v11:some value ", &error);
    deserialize_check_incorrect (-3,
                                 "mc_deserialize_config() at 10: mc_serialize_str(): Specified data length (6000) is greater than actual data length (21)");

    actual = mc_deserialize_config (etalon_str, &error);

    if (actual == NULL)
    {
        fail ("actual value is NULL!\nError code is '%d'; error message is '%s'", error->code,
              error->message);
        g_clear_error (&error);
        return;
    }

    actual_value = mc_config_get_string_raw (actual, "group1", "param1", "");
    fail_unless (strcmp (actual_value, "some value") == 0,
                 "group1->param1(%s) should be equal to 'some value'", actual_value);
    g_free (actual_value);

    actual_value = mc_config_get_string (actual, "group1", "param2", "");
    fail_unless (strcmp (actual_value, "some value ") == 0,
                 "group1->param2(%s) should be equal to 'some value '", actual_value);
    g_free (actual_value);

    fail_unless (mc_config_get_bool (actual, "group2", "param1", FALSE) == TRUE,
                 "group2->param1(FALSE) should be equal to TRUE");

    fail_unless (mc_config_get_int (actual, "group2", "param2", 0) == 123456,
                 "group2->param2(%d) should be equal to 123456", mc_config_get_int (actual,
                                                                                    "group2",
                                                                                    "param2", 0));

    actual_value = mc_config_get_string_raw (actual, "group3", "param1", "");
    fail_unless (strcmp (actual_value, "::bla-bla::") == 0,
                 "group3->param1(%s) should be equal to '::bla-bla::'", actual_value);
    g_free (actual_value);

    actual_value = mc_config_get_string (actual, "group3", "param2", "");
    fail_unless (strcmp (actual_value, "bla-:p1:w:v2:12:g3:123:bla-bla\n") == 0,
                 "group3->param2(%s) should be equal to 'bla-:p1:w:v2:12:g3:123:bla-bla\n'",
                 actual_value);
    g_free (actual_value);

    fail_unless (mc_config_get_bool (actual, "group4", "param1", TRUE) == FALSE,
                 "group4->param1(TRUE) should be equal to FALSE");

    fail_unless (mc_config_get_int (actual, "group4", "param2", 0) == 654321,
                 "group4->param2(%d) should be equal to 654321", mc_config_get_int (actual,
                                                                                    "group4",
                                                                                    "param2", 0));

    mc_config_deinit (actual);
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
    tcase_add_test (tc_core, test_serialize_deserialize_str);
    tcase_add_test (tc_core, test_serialize_config);
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
