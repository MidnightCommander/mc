/*
   src/filemanager - examine_cd() function testing

   Copyright (C) 2012, 2013
   The Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2012
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

#include "lib/vfs/path.h"
#include "src/filemanager/layout.h"
#include "src/filemanager/midnight.h"
#include "src/filemanager/tree.h"
#ifdef ENABLE_SUBSHELL
#include "src/subshell.h"
#endif /* ENABLE_SUBSHELL */

#include "src/filemanager/command.c"

/* --------------------------------------------------------------------------------------------- */

gboolean command_prompt = FALSE;
#ifdef ENABLE_SUBSHELL
enum subshell_state_enum subshell_state = INACTIVE;
#endif /* ENABLE_SUBSHELL */
int quit = 0;
WPanel *current_panel = NULL;

panel_view_mode_t
get_current_type (void)
{
    return view_listing;
}

gboolean
do_cd (const vfs_path_t * new_dir_vpath, enum cd_enum cd_type)
{
    (void) new_dir_vpath;
    (void) cd_type;

    return TRUE;
}

gboolean
quiet_quit_cmd (void)
{
    return FALSE;
}

char *
expand_format (struct WEdit *edit_widget, char c, gboolean do_quote)
{
    (void) edit_widget;
    (void) c;
    (void) do_quote;

    return NULL;
}

#ifdef ENABLE_SUBSHELL
void
init_subshell (void)
{
}

gboolean
do_load_prompt (void)
{
    return TRUE;
}
#endif /* ENABLE_SUBSHELL */

void
shell_execute (const char *command, int flags)
{
    (void) command;
    (void) flags;
}

void
sync_tree (const char *pathname)
{
    (void) pathname;
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
    fail_unless (strcmp (result, etalon) == 0, \
    "\ninput (%s)\nactial (%s) not equal to\netalon (%s)", input, result, etalon); \
    g_free (result); \
}

/* *INDENT-OFF* */
START_TEST (test_examine_cd)
/* *INDENT-ON* */
{
    char *result;

    g_setenv ("AAA", "aaa", TRUE);

    check_examine_cd ("/test/path", "/test/path");

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
    int number_failed;

    Suite *s = suite_create (TEST_SUITE_NAME);
    TCase *tc_core = tcase_create ("Core");
    SRunner *sr;

    tcase_add_checked_fixture (tc_core, setup, teardown);

    /* Add new tests here: *************** */
    tcase_add_test (tc_core, test_examine_cd);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "examine_cd.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);

    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
