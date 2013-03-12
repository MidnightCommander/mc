/*
   src - tests for execute_external_editor_or_viewer() function

   Copyright (C) 2013
   The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2013

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

#define TEST_SUITE_NAME "/src"

#include "tests/mctest.h"

#include "lib/mcconfig.h"
#include "lib/strutil.h"
#include "lib/vfs/vfs.h"
#include "src/vfs/local/local.c"

char *execute_get_external_cmd_opts_from_config (const char *command,
                                                 const vfs_path_t * filename_vpath, int start_line);

/* --------------------------------------------------------------------------------------------- */

/* @CapturedValue */
static GPtrArray *mc_config_get_string__group__captured;
/* @CapturedValue */
static GPtrArray *mc_config_get_string__param__captured;
/* @CapturedValue */
static GPtrArray *mc_config_get_string__default_value__captured;
/* @ThenReturnValue */
static GPtrArray *mc_config_get_string__return_value;

/* @Mock */
gchar *
mc_config_get_string_raw (const mc_config_t * config_ignored, const gchar * group,
                          const gchar * param, const gchar * default_value)
{
    char *return_value;
    (void) config_ignored;

    g_ptr_array_add (mc_config_get_string__group__captured, g_strdup (group));
    g_ptr_array_add (mc_config_get_string__param__captured, g_strdup (param));
    g_ptr_array_add (mc_config_get_string__default_value__captured, g_strdup (default_value));

    return_value = g_ptr_array_index (mc_config_get_string__return_value, 0);
    g_ptr_array_remove_index (mc_config_get_string__return_value, 0);
    return return_value;
}

static void
mc_config_get_string__init (void)
{
    mc_config_get_string__group__captured = g_ptr_array_new ();
    mc_config_get_string__param__captured = g_ptr_array_new ();
    mc_config_get_string__default_value__captured = g_ptr_array_new ();

    mc_config_get_string__return_value = g_ptr_array_new ();
}

static void
mc_config_get_string__deinit (void)
{
    g_ptr_array_foreach (mc_config_get_string__group__captured, (GFunc) g_free, NULL);
    g_ptr_array_free (mc_config_get_string__group__captured, TRUE);

    g_ptr_array_foreach (mc_config_get_string__param__captured, (GFunc) g_free, NULL);
    g_ptr_array_free (mc_config_get_string__param__captured, TRUE);

    g_ptr_array_foreach (mc_config_get_string__default_value__captured, (GFunc) g_free, NULL);
    g_ptr_array_free (mc_config_get_string__default_value__captured, TRUE);

    g_ptr_array_foreach (mc_config_get_string__return_value, (GFunc) g_free, NULL);
    g_ptr_array_free (mc_config_get_string__return_value, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    str_init_strings (NULL);
    vfs_init ();
    init_localfs ();
    vfs_setup_work_dir ();

    mc_config_get_string__init ();
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    mc_config_get_string__deinit ();

    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("check_subtitute_ds") */
/* *INDENT-OFF* */
static const struct check_subtitute_ds
{
    const char *config_opts_string;
    const char *app_name;
    const char *file_name;
    int  start_line;
    const char *expected_result;
} check_subtitute_ds[] =
{
    {
        "-a -b -c %filename \\%filename %filename:%lineno \\%lineno +%lineno",
        "some-editor",
        "/path/to/file",
        1234,
        "-a -b -c '/path/to/file' %filename '/path/to/file':1234 %lineno +1234",
    },
    {
        "%filename:\\\\\\\\\\\\%lineno",
        "some-editor",
        "/path/to/'f i\" l e \t\t\n",
        1234,
        "'/path/to/'\\''f i\" l e \t\t\n':\\\\\\\\\\\\1234",
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "check_subtitute_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (check_if_filename_and_lineno_will_be_subtituted, check_subtitute_ds)
/* *INDENT-ON* */
{
    /* given */
    char *actual_result;
    vfs_path_t *filename_vpath;

    g_ptr_array_add (mc_config_get_string__return_value, g_strdup (data->config_opts_string));
    filename_vpath = vfs_path_from_str (data->file_name);

    /* when */
    actual_result =
        execute_get_external_cmd_opts_from_config (data->app_name, filename_vpath,
                                                   data->start_line);

    /* then */

    /* check returned value */
    mctest_assert_str_eq (actual_result, data->expected_result);

    /* check calls to mc_config_get_string() function */
    mctest_assert_str_eq (g_ptr_array_index (mc_config_get_string__group__captured, 0),
                          CONFIG_EXT_EDITOR_VIEWER_SECTION);
    mctest_assert_str_eq (g_ptr_array_index (mc_config_get_string__param__captured, 0),
                          data->app_name);
    mctest_assert_str_eq (g_ptr_array_index (mc_config_get_string__default_value__captured, 0),
                          NULL);

    vfs_path_free (filename_vpath);

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

    tcase_add_checked_fixture (tc_core, setup, teardown);

    /* Add new tests here: *************** */
    mctest_add_parameterized_test (tc_core, check_if_filename_and_lineno_will_be_subtituted,
                                   check_subtitute_ds);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "execute__execute_get_external_cmd_opts_from_config.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
