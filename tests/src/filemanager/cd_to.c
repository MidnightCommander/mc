/*
   src/filemanager - tests for cd_to() function

   Copyright (C) 2011-2024
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2011, 2013
   Andrew Borodin <aborodin@vmail.ru>, 2019, 2020

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

#include "src/filemanager/cd.c"
#include "src/filemanager/panel.h"

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
panel_cd (WPanel * panel, const vfs_path_t * new_dir_vpath, enum cd_enum cd_type)
{
    (void) panel;

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
    vfs_init_localfs ();
    vfs_setup_work_dir ();
    do_cd__new_dir_vpath__captured = NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    vfs_path_free (do_cd__new_dir_vpath__captured, TRUE);
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
        ""
    },
    {
        "                      "
    },
    {
        "\t\t\t\t\t\t\t\t\t\t\t"
    },
    {
        "  \t   \t  \t\t    \t    "
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
        cd_to (input_command);
        g_free (input_command);
    }
    /* then */
    mctest_assert_str_eq (mc_config_get_home_dir__return_value,
                          vfs_path_as_str (do_cd__new_dir_vpath__captured));
    ck_assert_int_eq (do_cd__cd_type__captured, cd_parse_command);
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    /* Add new tests here: *************** */
    mctest_add_parameterized_test (tc_core, test_empty_mean_home, test_empty_mean_home_ds);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
