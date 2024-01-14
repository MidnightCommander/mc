/*
   src/filemanager - filemanager functions

   Copyright (C) 2011-2024
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

#define TEST_SUITE_NAME "/src/filemanager"

#include "tests/mctest.h"

#include "lib/file-entry.h"

#include "src/vfs/local/local.c"

#include "src/filemanager/filemanager.c"

#include "src/filemanager/ext.c"

/* --------------------------------------------------------------------------------------------- */
/* mocked functions */


/* --------------------------------------------------------------------------------------------- */

static void
setup (void)
{
    str_init_strings (NULL);

    vfs_init ();
    vfs_init_localfs ();
    vfs_setup_work_dir ();

    mc_global.mc_run_mode = MC_RUN_FULL;
    current_panel = g_new0 (WPanel, 1);
    current_panel->cwd_vpath = vfs_path_from_str ("/home");
    current_panel->dir.size = DIR_LIST_MIN_SIZE;
    current_panel->dir.list = g_new0 (file_entry_t, current_panel->dir.size);
    current_panel->dir.len = 0;
}

static void
teardown (void)
{
    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */
START_TEST (sanitize_variables)
/* *INDENT-ON* */
{
    /* given */
    vfs_path_t *filename_vpath;
    GString *actual_string;
    const char *expected_string;

    current_panel->current = 0;
    current_panel->dir.len = 3;
    current_panel->dir.list[0].fname = g_string_new ("selected file.txt");
    current_panel->dir.list[1].fname = g_string_new ("tagged file1.txt");
    current_panel->dir.list[1].f.marked = 1;
    current_panel->dir.list[2].fname = g_string_new ("tagged file2.txt");
    current_panel->dir.list[2].f.marked = 1;

    /* when */
    filename_vpath = vfs_path_from_str ("/tmp/blabla.txt");
    actual_string = exec_get_export_variables (filename_vpath);
    vfs_path_free (filename_vpath, TRUE);

    /* then */
    expected_string = "\
MC_EXT_FILENAME=/tmp/blabla.txt\n\
export MC_EXT_FILENAME\n\
MC_EXT_BASENAME=selected\\ file.txt\n\
export MC_EXT_BASENAME\n\
MC_EXT_CURRENTDIR=/home\n\
export MC_EXT_CURRENTDIR\n\
MC_EXT_SELECTED=\"selected\\ file.txt\"\n\
export MC_EXT_SELECTED\n\
MC_EXT_ONLYTAGGED=\"tagged\\ file1.txt tagged\\ file2.txt \"\n\
export MC_EXT_ONLYTAGGED\n";

    mctest_assert_str_eq (actual_string->str, expected_string);

    g_string_free (actual_string, TRUE);
    g_string_free (current_panel->dir.list[0].fname, TRUE);
    g_string_free (current_panel->dir.list[1].fname, TRUE);
    g_string_free (current_panel->dir.list[2].fname, TRUE);
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    /* Add new tests here: *************** */
    tcase_add_test (tc_core, sanitize_variables);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
