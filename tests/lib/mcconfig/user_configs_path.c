/*
   libmc - check mcconfig submodule. Get full paths to user's config files.

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

#include "lib/strutil.h"
#include "lib/vfs/vfs.h"
#include "lib/fileloc.h"

#include "lib/mcconfig.h"
#include "src/vfs/local/local.c"

#define HOME_DIR   "/home/testuser"

#define CONF_MAIN  HOME_DIR PATH_SEP_STR ".config"
#define CONF_DATA  HOME_DIR PATH_SEP_STR ".local" PATH_SEP_STR "share"
#define CONF_CACHE HOME_DIR PATH_SEP_STR ".cache"

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    g_setenv ("HOME", HOME_DIR, TRUE);
    g_setenv ("XDG_CONFIG_HOME", CONF_MAIN, TRUE);
    g_setenv ("XDG_DATA_HOME", CONF_DATA, TRUE);
    g_setenv ("XDG_CACHE_HOME", CONF_CACHE, TRUE);
    str_init_strings ("UTF-8");
    vfs_init ();
    vfs_init_localfs ();
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_user_config_paths_ds") */
static const struct test_user_config_paths_ds
{
    const char *input_base_dir;
    const char *input_file_name;
} test_user_config_paths_ds[] = {
    { CONF_MAIN, MC_CONFIG_FILE },
    { CONF_MAIN, MC_FHL_INI_FILE },
    { CONF_MAIN, MC_HOTLIST_FILE },
    { CONF_MAIN, GLOBAL_KEYMAP_FILE },
    { CONF_MAIN, MC_USERMENU_FILE },
    { CONF_DATA, EDIT_SYNTAX_FILE },
    { CONF_MAIN, EDIT_HOME_MENU },
    { CONF_MAIN, MC_PANELS_FILE },
    { CONF_MAIN, MC_EXT_FILE },
    { CONF_DATA, MC_SKINS_DIR },
    { CONF_DATA, VFS_SHELL_PREFIX },

    { CONF_DATA, MC_ASHRC_CUSTOM_PROFILE_FILE },
    { CONF_DATA, MC_BASHRC_CUSTOM_PROFILE_FILE },
    { CONF_DATA, MC_KSHRC_CUSTOM_PROFILE_FILE },
    { CONF_DATA, MC_MKSHRC_CUSTOM_PROFILE_FILE },
    { CONF_DATA, MC_ZSHRC_CUSTOM_PROFILE_FILE },

    { CONF_DATA, MC_INPUTRC_FILE },

    { CONF_DATA, MC_EXTFS_DIR },
    { CONF_DATA, MC_HISTORY_FILE },
    { CONF_DATA, MC_FILEPOS_FILE },
    { CONF_DATA, EDIT_HOME_CLIP_FILE },
    { CONF_DATA, MC_MACRO_FILE },
    { CONF_CACHE, "mc.log" },
    { CONF_CACHE, MC_TREESTORE_FILE },
    { CONF_CACHE, EDIT_HOME_TEMP_FILE },
    { CONF_CACHE, EDIT_HOME_BLOCK_FILE },
};

/* @Test(dataSource = "test_user_config_paths_ds") */
START_PARAMETRIZED_TEST (test_user_config_paths, test_user_config_paths_ds)
{
    // given
    char *actual_result;

    // when
    actual_result = mc_config_get_full_path (data->input_file_name);

    // then
    {
        char *expected_file_path;

        expected_file_path = g_build_filename (data->input_base_dir, MC_USERCONF_DIR,
                                               data->input_file_name, (char *) NULL);
        mctest_assert_str_eq (actual_result, expected_file_path);
        g_free (expected_file_path);
    }
    g_free (actual_result);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    // Add new tests here: ***************
    mctest_add_parameterized_test (tc_core, test_user_config_paths, test_user_config_paths_ds);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
