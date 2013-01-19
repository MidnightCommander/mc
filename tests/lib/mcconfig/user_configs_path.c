/*
   libmc - check mcconfig submodule. Get full paths to user's config files.

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

#define TEST_SUITE_NAME "lib/mcconfig"

#include "tests/mctest.h"

#include "lib/strutil.h"
#include "lib/strescape.h"
#include "lib/vfs/vfs.h"
#include "lib/fileloc.h"

#include "lib/mcconfig.h"
#include "src/vfs/local/local.c"

#define HOME_DIR "/home/testuser"

#ifdef MC_HOMEDIR_XDG
#define CONF_MAIN HOME_DIR PATH_SEP_STR ".config"
#define CONF_DATA HOME_DIR PATH_SEP_STR ".local" PATH_SEP_STR "share"
#define CONF_CACHE HOME_DIR PATH_SEP_STR ".cache"
#else
#define CONF_MAIN HOME_DIR
#define CONF_DATA CONF_MAIN
#define CONF_CACHE CONF_MAIN
#endif


static void
setup (void)
{
    g_setenv ("HOME", HOME_DIR, TRUE);
#ifdef MC_HOMEDIR_XDG
    g_setenv ("XDG_CONFIG_HOME", CONF_MAIN, TRUE);
    g_setenv ("XDG_DATA_HOME", CONF_DATA, TRUE);
    g_setenv ("XDG_CACHE_HOME", CONF_CACHE, TRUE);
#endif
    str_init_strings ("UTF-8");
    vfs_init ();
    init_localfs ();
}

static void
teardown (void)
{
    vfs_shut ();
    str_uninit_strings ();
}

#define path_fail_unless(conf_dir, conf_name) {\
    result = mc_config_get_full_path (conf_name); \
    fail_unless (strcmp( conf_dir PATH_SEP_STR MC_USERCONF_DIR PATH_SEP_STR conf_name, result) == 0); \
    g_free (result); \
}
/* --------------------------------------------------------------------------------------------- */
/* *INDENT-OFF* */
START_TEST (user_configs_path_test)
/* *INDENT-ON* */
{
    char *result;

    path_fail_unless (CONF_MAIN, MC_CONFIG_FILE);

    path_fail_unless (CONF_MAIN, MC_FHL_INI_FILE);
    path_fail_unless (CONF_MAIN, MC_HOTLIST_FILE);
    path_fail_unless (CONF_MAIN, GLOBAL_KEYMAP_FILE);
    path_fail_unless (CONF_MAIN, MC_USERMENU_FILE);
    path_fail_unless (CONF_MAIN, EDIT_SYNTAX_FILE);
    path_fail_unless (CONF_MAIN, EDIT_HOME_MENU);
    path_fail_unless (CONF_MAIN, EDIT_DIR PATH_SEP_STR "edit.indent.rc");
    path_fail_unless (CONF_MAIN, EDIT_DIR PATH_SEP_STR "edit.spell.rc");
    path_fail_unless (CONF_MAIN, MC_PANELS_FILE);
    path_fail_unless (CONF_MAIN, MC_FILEBIND_FILE);

    path_fail_unless (CONF_DATA, MC_SKINS_SUBDIR);
    path_fail_unless (CONF_DATA, FISH_PREFIX);
    path_fail_unless (CONF_DATA, "bashrc");
    path_fail_unless (CONF_DATA, "inputrc");
    path_fail_unless (CONF_DATA, MC_EXTFS_DIR);
    path_fail_unless (CONF_DATA, MC_HISTORY_FILE);
    path_fail_unless (CONF_DATA, MC_FILEPOS_FILE);
    path_fail_unless (CONF_DATA, EDIT_CLIP_FILE);
    path_fail_unless (CONF_DATA, MC_MACRO_FILE);

    path_fail_unless (CONF_CACHE, "mc.log");
    path_fail_unless (CONF_CACHE, MC_TREESTORE_FILE);
    path_fail_unless (CONF_CACHE, EDIT_TEMP_FILE);
    path_fail_unless (CONF_CACHE, EDIT_BLOCK_FILE);

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
    tcase_add_test (tc_core, user_configs_path_test);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "user_configs_path.log");
    /* srunner_set_fork_status (sr, CK_NOFORK); */
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
