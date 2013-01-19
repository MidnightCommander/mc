/*
   src/filemanager - tests for do_cd_command() function

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

#define TEST_SUITE_NAME "/src/filemanager"

#include "tests/mctest.h"

#include "src/vfs/local/local.c"

#include "src/filemanager/command.c"


/* --------------------------------------------------------------------------------------------- */

/* @ThenReturnValue */
static panel_view_mode_t get_current_type__return_value;

/* @Mock */
panel_view_mode_t
get_current_type (void)
{
    return get_current_type__return_value;
}

/* --------------------------------------------------------------------------------------------- */

/* @CapturedValue */
static vfs_path_t *do_cd__new_dir_vpath__captured;
/* @CapturedValue */
static enum cd_enum do_cd__cd_type__captured;
/* @ThenReturnValue */
static gboolean do_cd__return_value;

/* @Mock */
gboolean
do_cd (const vfs_path_t * new_dir_vpath, enum cd_enum cd_type)
{
    do_cd__new_dir_vpath__captured = vfs_path_clone (new_dir_vpath);
    do_cd__cd_type__captured = cd_type;
    return do_cd__return_value;
}

/* --------------------------------------------------------------------------------------------- */

/* @ThenReturnValue */
static const char *mc_config_get_home_dir__return_value;

/* @Mock */
const char *
mc_config_get_home_dir (void)
{
    return mc_config_get_home_dir__return_value;
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
    do_cd__new_dir_vpath__captured = NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    vfs_path_free (do_cd__new_dir_vpath__captured);
    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_empty_mean_home_ds") */
/* *INDENT-OFF* */
static const struct test_empty_mean_home_ds
{
    const char *command;
} test_empty_mean_home_ds[] =
{
    {
        "cd"
    },
    {
        "cd                      "
    },
    {
        "cd\t\t\t\t\t\t\t\t\t\t\t"
    },
    {
        "cd  \t   \t  \t\t    \t    "
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_empty_mean_home_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_empty_mean_home, test_empty_mean_home_ds)
/* *INDENT-ON* */
{
    /* given */
    get_current_type__return_value = view_listing;
    do_cd__return_value = TRUE;
    mc_config_get_home_dir__return_value = "/home/test";

    /* when */
    {
        char *input_command;

        input_command = g_strdup (data->command);
        do_cd_command (input_command);
        g_free (input_command);
    }
    /* then */
    {
        char *actual_path;

        actual_path = vfs_path_to_str (do_cd__new_dir_vpath__captured);
        mctest_assert_str_eq (mc_config_get_home_dir__return_value, actual_path);
        g_free (actual_path);
    }
    mctest_assert_int_eq (do_cd__cd_type__captured, cd_parse_command);
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
    mctest_add_parameterized_test (tc_core, test_empty_mean_home, test_empty_mean_home_ds);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "do_cd_command.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
