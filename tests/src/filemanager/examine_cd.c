/*
   src/filemanager - examine_cd() function testing

   Copyright (C) 2012-2024
   Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2012, 2020
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

#define TEST_SUITE_NAME "/src/filemanager"

#include "tests/mctest.h"

#include <stdio.h>

#include "src/filemanager/cd.c"

/* --------------------------------------------------------------------------------------------- */

WPanel *current_panel = NULL;

panel_view_mode_t
get_current_type (void)
{
    return view_listing;
}

gboolean
panel_cd (WPanel *panel, const vfs_path_t *new_dir_vpath, enum cd_enum cd_type)
{
    (void) panel;
    (void) new_dir_vpath;
    (void) cd_type;

    return TRUE;
}

void
sync_tree (const vfs_path_t *vpath)
{
    (void) vpath;
}

/* --------------------------------------------------------------------------------------------- */

static void
setup (void)
{
}

static void
teardown (void)
{
}

/* --------------------------------------------------------------------------------------------- */

#define check_examine_cd(input, etalon) \
{ \
    result = examine_cd (input); \
    ck_assert_msg (strcmp (result->str, etalon) == 0, \
    "\ninput (%s)\nactial (%s) not equal to\netalon (%s)", input, result->str, etalon); \
    g_string_free (result, TRUE); \
}

/* *INDENT-OFF* */
START_TEST (test_examine_cd)
/* *INDENT-ON* */
{
    GString *result;

    g_setenv ("AAA", "aaa", TRUE);

    check_examine_cd ("/test/path", "/test/path");

    check_examine_cd ("$AAA", "aaa");
    check_examine_cd ("${AAA}", "aaa");
    check_examine_cd ("$AAA/test", "aaa/test");
    check_examine_cd ("${AAA}/test", "aaa/test");

    check_examine_cd ("/$AAA", "/aaa");
    check_examine_cd ("/${AAA}", "/aaa");
    check_examine_cd ("/$AAA/test", "/aaa/test");
    check_examine_cd ("/${AAA}/test", "/aaa/test");

    check_examine_cd ("/test/path/$AAA", "/test/path/aaa");
    check_examine_cd ("/test/path/$AAA/test2", "/test/path/aaa/test2");
    check_examine_cd ("/test/path/test1$AAA/test2", "/test/path/test1aaa/test2");

    check_examine_cd ("/test/path/${AAA}", "/test/path/aaa");
    check_examine_cd ("/test/path/${AAA}/test2", "/test/path/aaa/test2");
    check_examine_cd ("/test/path/${AAA}test2", "/test/path/aaatest2");
    check_examine_cd ("/test/path/test1${AAA}", "/test/path/test1aaa");
    check_examine_cd ("/test/path/test1${AAA}test2", "/test/path/test1aaatest2");

    check_examine_cd ("/test/path/\\$AAA", "/test/path/$AAA");
    check_examine_cd ("/test/path/\\$AAA/test2", "/test/path/$AAA/test2");
    check_examine_cd ("/test/path/test1\\$AAA", "/test/path/test1$AAA");

    check_examine_cd ("/test/path/\\${AAA}", "/test/path/${AAA}");
    check_examine_cd ("/test/path/\\${AAA}/test2", "/test/path/${AAA}/test2");
    check_examine_cd ("/test/path/\\${AAA}test2", "/test/path/${AAA}test2");
    check_examine_cd ("/test/path/test1\\${AAA}test2", "/test/path/test1${AAA}test2");
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
    tcase_add_test (tc_core, test_examine_cd);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
