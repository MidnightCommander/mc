/*
   libmc - check mcconfig submodule. read and write config files

   Copyright (C) 2011-2025
   Free Software Foundation, Inc.

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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define TEST_SUITE_NAME "lib/mcconfig"

#include "tests/mctest.h"

#include "lib/mcconfig.h"
#include "lib/strutil.h"
#include "lib/vfs/vfs.h"
#include "src/vfs/local/local.c"

static mc_config_t *mc_config;
static char *ini_filename;

/* --------------------------------------------------------------------------------------------- */

static void
config_object__init (void)
{
    ini_filename = g_build_filename (WORKDIR, "config_string.ini", (char *) NULL);
    unlink (ini_filename);

    mc_config = mc_config_init (ini_filename, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static void
config_object__reopen (void)
{
    GError *error = NULL;

    if (!mc_config_save_file (mc_config, &error))
    {
        ck_abort_msg ("Unable to save config file: %s", error->message);
    }

    mc_config_deinit (mc_config);
    mc_config = mc_config_init (ini_filename, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static void
config_object__deinit (void)
{
    mc_config_deinit (mc_config);
    g_free (ini_filename);
}

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    str_init_strings ("KOI8-R");
    vfs_init ();
    vfs_init_localfs ();

    config_object__init ();
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    config_object__deinit ();

    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_create_ini_file_ds") */
static const struct test_create_ini_file_ds
{
    const char *input_group;
    const char *input_param;
    const char *input_default_value;
    const char *expected_value;
    const char *expected_raw_value;
} test_create_ini_file_ds[] = {
    {
        // 0.
        "group-not-exists",
        "param-not_exists",
        NULL,
        NULL,
        NULL,
    },
    {
        // 1.
        "test-group1",
        "test-param1",
        "not-exists",
        " some value ",
        " some value ",
    },
    {
        // 2.
        "test-group1",
        "test-param2",
        "not-exists",
        // Should be represented as KOI8-R
        " \t \xF4\xC5\xD3\xD4\xCF\xD7\xCF\xC5 \xDA\xCE\xC1\xDE\xC5\xCE\xC9\xC5 ",
        // Should be stored as UTF-8
        " \t Тестовое значение ",
    },
    {
        // 3.
        "test-group1",
        "test-param3",
        "not-exists",
        " \tsome value2\n\nf\b\005fff ",
        " \tsome value2\n\nf\b\005fff ",
    },
    {
        // 4.
        "test-group2",
        "test-param1",
        "not-exists",
        " some value ",
        " some value ",
    },
    {
        // 5.
        "test-group2",
        "test-param2",
        "not-exists",
        "not-exists",
        "not-exists",
    },
    {
        // 6.
        "test-group2",
        "test-param3",
        "not-exists",
        " \tsome value2\n\nf\b\005fff ",
        " \tsome value2\n\nf\b\005fff ",
    },
};

/* @Test(dataSource = "test_create_ini_file_ds") */
START_PARAMETRIZED_TEST (test_create_ini_file_paths, test_create_ini_file_ds)
{
    // given
    char *actual_value, *actual_raw_value;

    mc_config_set_string (mc_config, "test-group1", "test-param1", " some value ");
    mc_config_set_string (mc_config, "test-group1", "test-param2",
                          " \t \xF4\xC5\xD3\xD4\xCF\xD7\xCF\xC5 \xDA\xCE\xC1\xDE\xC5\xCE\xC9\xC5 ");
    mc_config_set_string (mc_config, "test-group1", "test-param3", " \tsome value2\n\nf\b\005fff ");
    mc_config_set_string_raw (mc_config, "test-group2", "test-param1", " some value ");
    mc_config_set_string_raw (mc_config, "test-group2", "test-param2",
                              " \xF4\xC5\xD3\xD4\xCF\xD7\xCF\xC5 \xDA\xCE\xC1\xDE\xC5\xCE\xC9\xC5");
    mc_config_set_string_raw (mc_config, "test-group2", "test-param3",
                              " \tsome value2\n\nf\b\005fff ");

    config_object__reopen ();

    // when
    actual_value = mc_config_get_string (mc_config, data->input_group, data->input_param,
                                         data->input_default_value);
    actual_raw_value = mc_config_get_string_raw (mc_config, data->input_group, data->input_param,
                                                 data->input_default_value);

    // then
    mctest_assert_str_eq (actual_value, data->expected_value);
    mctest_assert_str_eq (actual_raw_value, data->expected_raw_value);

    g_free (actual_value);
    g_free (actual_raw_value);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test(group='Integration') */
START_TEST (emulate__learn_save)
{
    // given
    char *actual_value;

    {
        char *esc_str;

        esc_str = str_escape ("T;E\\X;T-FOR-\\T;E;S\\TI;N'G", -1, ";", TRUE);
        mc_config_set_string_raw (mc_config, "test-group1", "test-param1", esc_str);
        g_free (esc_str);
    }

    config_object__reopen ();

    // when
    actual_value = mc_config_get_string_raw (mc_config, "test-group1", "test-param1", "not-exists");

    // then
    mctest_assert_str_eq (actual_value, "T\\;E\\X\\;T-FOR-\\T\\;E\\;S\\TI\\;N'G");
    g_free (actual_value);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    // Add new tests here: ***************
    mctest_add_parameterized_test (tc_core, test_create_ini_file_paths, test_create_ini_file_ds);
    tcase_add_test (tc_core, emulate__learn_save);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
