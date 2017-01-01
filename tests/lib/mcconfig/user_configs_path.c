/*
   libmc - check mcconfig submodule. Get full paths to user's config files.

   Copyright (C) 2011-2017
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

#if MC_HOMEDIR_XDG
#define CONF_MAIN HOME_DIR PATH_SEP_STR ".config"
#define CONF_DATA HOME_DIR PATH_SEP_STR ".local" PATH_SEP_STR "share"
#define CONF_CACHE HOME_DIR PATH_SEP_STR ".cache"
#else
#define CONF_MAIN HOME_DIR
#define CONF_DATA CONF_MAIN
#define CONF_CACHE CONF_MAIN
#endif

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    g_setenv ("HOME", HOME_DIR, TRUE);
#if MC_HOMEDIR_XDG
    g_setenv ("XDG_CONFIG_HOME", CONF_MAIN, TRUE);
    g_setenv ("XDG_DATA_HOME", CONF_DATA, TRUE);
    g_setenv ("XDG_CACHE_HOME", CONF_CACHE, TRUE);
#endif
    str_init_strings ("UTF-8");
    vfs_init ();
    init_localfs ();
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
/* *INDENT-OFF* */
static const struct test_user_config_paths_ds
{
    const char *input_base_dir;
    const char *input_file_name;
} test_user_config_paths_ds[] =
{
    { /* 0. */
        CONF_MAIN,
        MC_CONFIG_FILE
    },
    { /* 1. */
        CONF_MAIN,
        MC_FHL_INI_FILE
    },
    { /* 2. */
        CONF_DATA,
        MC_HOTLIST_FILE
    },
    { /* 3. */
        CONF_MAIN,
        GLOBAL_KEYMAP_FILE
    },
    { /* 4. */
        CONF_MAIN,
        MC_USERMENU_FILE
    },
    { /* 5. */
        CONF_MAIN,
        EDIT_SYNTAX_FILE
    },
    { /* 6. */
        CONF_MAIN,
        EDIT_HOME_MENU
    },
    { /* 7. */
        CONF_MAIN,
        EDIT_DIR PATH_SEP_STR "edit.indent.rc"
    },
    { /* 8. */
        CONF_MAIN,
        EDIT_DIR PATH_SEP_STR "edit.spell.rc"
    },
    { /* 9. */
        CONF_MAIN,
        MC_PANELS_FILE
    },
    { /* 10. */
        CONF_MAIN,
        MC_FILEBIND_FILE
    },
    { /* 11. */
        CONF_DATA,
        MC_SKINS_SUBDIR
    },
    { /* 12. */
        CONF_DATA,
        FISH_PREFIX
    },
    { /* 13. */
        CONF_DATA,
        "ashrc"
    },
    { /* 14. */
        CONF_DATA,
        "bashrc"
    },
    { /* 15. */
        CONF_DATA,
        "inputrc"
    },
    { /* 16. */
        CONF_DATA,
        MC_EXTFS_DIR
    },
    { /* 17. */
        CONF_DATA,
        MC_HISTORY_FILE
    },
    { /* 18. */
        CONF_DATA,
        MC_FILEPOS_FILE
    },
    { /* 19. */
        CONF_DATA,
        EDIT_CLIP_FILE
    },
    { /* 20. */
        CONF_DATA,
        MC_MACRO_FILE
    },
    { /* 21. */
        CONF_CACHE,
        "mc.log"
    },
    { /* 22. */
        CONF_CACHE,
        MC_TREESTORE_FILE
    },
    { /* 23. */
        CONF_CACHE,
        EDIT_TEMP_FILE
    },
    { /* 24. */
        CONF_CACHE,
        EDIT_BLOCK_FILE
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_user_config_paths_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_user_config_paths, test_user_config_paths_ds)
/* *INDENT-ON* */
{
    /* given */
    char *actual_result;

    /* when */
    actual_result = mc_config_get_full_path (data->input_file_name);

    /* then */
    {
        char *expected_file_path;

        expected_file_path =
            g_build_filename (data->input_base_dir, MC_USERCONF_DIR, data->input_file_name,
                              (char *) NULL);
        mctest_assert_str_eq (actual_result, expected_file_path);
        g_free (expected_file_path);
    }
    g_free (actual_result);
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
    mctest_add_parameterized_test (tc_core, test_user_config_paths, test_user_config_paths_ds);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "user_configs_path.log");
    srunner_run_all (sr, CK_ENV);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* --------------------------------------------------------------------------------------------- */
