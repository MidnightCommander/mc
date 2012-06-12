/*
   lib/vfs - manipulate with current directory

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

#define TEST_SUITE_NAME "/src/filemanager"

#include <config.h>

#include <check.h>

#include "lib/global.h"
#include "src/main.h"
#include "src/vfs/local/local.c"

#include "src/filemanager/panel.c"

#include "do_panel_cd_stub_env.c"

static void
setup (void)
{
    str_init_strings (NULL);

    vfs_init ();
    init_localfs ();
    vfs_setup_work_dir ();
}

static void
teardown (void)
{
    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_do_panel_cd_empty_mean_home)
{
    char *cwd;
    char *home_wd;
    const char *home_directory;
    struct WPanel *panel;
    gboolean ret;
    vfs_path_t *empty_path;

    cmdline = command_new (0, 0, 0);

    panel = g_new0(struct WPanel, 1);
    panel->cwd_vpath = vfs_path_from_str("/home");
    panel->lwd_vpath = vfs_path_from_str("/");
    panel->sort_info.sort_field = g_new0(panel_field_t,1);

    home_directory = mc_config_get_home_dir();
    if (home_directory == NULL)
        home_directory = "/home/test";

    empty_path = vfs_path_from_str (home_directory);

    /*
     * normalize path to handle HOME with trailing slashes:
     *     HOME=/home/slyfox///////// ./do_panel_cd
     */
    home_wd = vfs_path_to_str (empty_path);
    ret = do_panel_cd (panel, empty_path, cd_parse_command);
    vfs_path_free (empty_path);

    fail_unless(ret);
    cwd = vfs_path_to_str (panel->cwd_vpath);

    printf ("mc_config_get_home_dir ()=%s\n", mc_config_get_home_dir ());
    printf ("cwd=%s\n", cwd);
    printf ("home_wd=%s\n", home_wd);
    fail_unless(strcmp(cwd, home_wd) == 0);

    g_free (cwd);
    g_free (home_wd);
    vfs_path_free (panel->cwd_vpath);
    vfs_path_free (panel->lwd_vpath);
    g_free ((gpointer) panel->sort_info.sort_field);
    g_free (panel);
}

END_TEST

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
    tcase_add_test (tc_core, test_do_panel_cd_empty_mean_home);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "do_panel_cd.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
