/*
   libmc - check mcconfig submodule. read and write config files

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

#define TEST_SUITE_NAME "lib/mcconfig"

#include <config.h>

#include <check.h>

#include "lib/global.h"
#include "lib/mcconfig.h"
#include "lib/strutil.h"
#include "lib/strescape.h"
#include "lib/vfs/vfs.h"
#include "src/vfs/local/local.c"



static void
setup (void)
{
    str_init_strings ("KOI8-R");
    vfs_init ();
    init_localfs ();
}

static void
teardown (void)
{
    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */
#define fail_unless_strcmp( etalon ) \
    fail_unless( \
        strcmp(actual_value, etalon) == 0, \
        "Actial value '%s' doesn't equal to etalon '%s'", actual_value, etalon \
    )

/* *INDENT-OFF* */
START_TEST (create_ini_file)
/* *INDENT-ON* */
{
    mc_config_t *mc_config;
    GError *error = NULL;
    char *actual_value;
    char *ini_filename = NULL;

    ini_filename = g_build_filename (WORKDIR, "test-create_ini_file.ini", NULL);
    unlink (ini_filename);

    mc_config = mc_config_init (ini_filename, FALSE);
    if (mc_config == NULL)
    {
        fail ("unable to create mc_congif_t object!");
        return;
    }

    mc_config_set_string (mc_config, "test-group1", "test-param1", " some value ");
    mc_config_set_string (mc_config, "test-group1", "test-param2", " \tkoi8-r: Тестовое значение ");
    mc_config_set_string (mc_config, "test-group1", "test-param3", " \tsome value2\n\nf\b\005fff ");

    mc_config_set_string_raw (mc_config, "test-group2", "test-param1", " some value ");
    mc_config_set_string_raw (mc_config, "test-group2", "test-param2",
                              " koi8-r: Тестовое значение");
    mc_config_set_string_raw (mc_config, "test-group2", "test-param3",
                              " \tsome value2\n\nf\b\005fff ");

    if (!mc_config_save_file (mc_config, &error))
    {
        fail ("Unable to save config file: %s", error->message);
        g_error_free (error);
    }

    mc_config_deinit (mc_config);
    mc_config = mc_config_init (ini_filename, FALSE);

    actual_value = mc_config_get_string (mc_config, "group-not-exists", "param-not_exists", NULL);
    fail_unless (actual_value == NULL,
                 "return value for nonexistent ini-parameters isn't NULL (default value)!");

    actual_value = mc_config_get_string (mc_config, "test-group1", "test-param1", "not-exists");
    fail_unless_strcmp (" some value ");
    g_free (actual_value);

    actual_value = mc_config_get_string (mc_config, "test-group1", "test-param2", "not-exists");
    fail_unless_strcmp (" \tkoi8-r: Тестовое значение ");
    g_free (actual_value);

    actual_value = mc_config_get_string (mc_config, "test-group1", "test-param3", "not-exists");
    fail_unless_strcmp (" \tsome value2\n\nf\b\005fff ");
    g_free (actual_value);


    actual_value = mc_config_get_string_raw (mc_config, "test-group2", "test-param1", "not-exists");
    fail_unless_strcmp (" some value ");
    g_free (actual_value);

    actual_value = mc_config_get_string_raw (mc_config, "test-group2", "test-param2", "not-exists");
    fail_unless_strcmp ("not-exists");
    g_free (actual_value);

    actual_value = mc_config_get_string_raw (mc_config, "test-group2", "test-param3", "not-exists");
    fail_unless_strcmp (" \tsome value2\n\nf\b\005fff ");
    g_free (actual_value);

    mc_config_deinit (mc_config);
    g_free (ini_filename);

}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */
START_TEST (emulate__learn_save)
/* *INDENT-ON* */
{
    mc_config_t *mc_config;
    char *actual_value, *esc_str;
    char *ini_filename = NULL;
    GError *error = NULL;

    ini_filename = g_build_filename (WORKDIR, "test-emulate__learn_save.ini", NULL);
    unlink (ini_filename);

    mc_config = mc_config_init (ini_filename, FALSE);
    if (mc_config == NULL)
    {
        fail ("unable to create mc_congif_t object!");
        return;
    }

    esc_str = strutils_escape ("T;E\\X;T-FOR-\\T;E;S\\TI;N'G", -1, ";", TRUE);
    mc_config_set_string_raw (mc_config, "test-group1", "test-param1", esc_str);
    g_free (esc_str);

    if (!mc_config_save_file (mc_config, &error))
    {
        fail ("Unable to save config file: %s", error->message);
        g_error_free (error);
    }

    mc_config_deinit (mc_config);
    mc_config = mc_config_init (ini_filename, FALSE);

    actual_value = mc_config_get_string_raw (mc_config, "test-group1", "test-param1", "not-exists");
    fail_unless_strcmp ("T\\;E\\X\\;T-FOR-\\T\\;E\\;S\\TI\\;N'G");
    g_free (actual_value);

    mc_config_deinit (mc_config);
    g_free (ini_filename);
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
    tcase_add_test (tc_core, create_ini_file);
    tcase_add_test (tc_core, emulate__learn_save);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    /* srunner_set_fork_status (sr, CK_NOFORK); */
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
