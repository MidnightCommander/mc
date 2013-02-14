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

#include "execute__common.c"

/* --------------------------------------------------------------------------------------------- */

char *execute_get_external_cmd_opts_from_config (const char *command,
                                                 const vfs_path_t * filename_vpath, int start_line);

/* @CapturedValue */
static char *execute_external_cmd_opts__command__captured;
/* @CapturedValue */
static vfs_path_t *execute_external_cmd_opts__filename_vpath__captured;
/* @CapturedValue */
static int execute_external_cmd_opts__start_line__captured;

/* @ThenReturnValue */
static char *execute_external_cmd_opts__return_value;

/* @Mock */
char *
execute_get_external_cmd_opts_from_config (const char *command, const vfs_path_t * filename_vpath,
                                           int start_line)
{
    execute_external_cmd_opts__command__captured = g_strdup (command);
    execute_external_cmd_opts__filename_vpath__captured = vfs_path_clone (filename_vpath);
    execute_external_cmd_opts__start_line__captured = start_line;

    return execute_external_cmd_opts__return_value;
}

static void
execute_get_external_cmd_opts_from_config__init (void)
{
    execute_external_cmd_opts__command__captured = NULL;
    execute_external_cmd_opts__filename_vpath__captured = NULL;
    execute_external_cmd_opts__start_line__captured = 0;
}

static void
execute_get_external_cmd_opts_from_config__deinit (void)
{
    g_free (execute_external_cmd_opts__command__captured);
    vfs_path_free (execute_external_cmd_opts__filename_vpath__captured);
}

/* --------------------------------------------------------------------------------------------- */
void do_executev (const char *lc_shell, int flags, char *const argv[]);

/* @CapturedValue */
static char *do_executev__lc_shell__captured;
/* @CapturedValue */
static int do_executev__flags__captured;
/* @CapturedValue */
static GPtrArray *do_executev__argv__captured;

/* @Mock */
void
do_executev (const char *lc_shell, int flags, char *const argv[])
{
    do_executev__lc_shell__captured = g_strdup (lc_shell);
    do_executev__flags__captured = flags;

    for (; argv != NULL && *argv != NULL; argv++)
        g_ptr_array_add (do_executev__argv__captured, g_strdup (*argv));
}

static void
do_executev__init (void)
{
    do_executev__lc_shell__captured = NULL;
    do_executev__argv__captured = g_ptr_array_new ();
    do_executev__flags__captured = 0;
}

static void
do_executev__deinit (void)
{
    g_free (do_executev__lc_shell__captured);
    g_ptr_array_foreach (do_executev__argv__captured, (GFunc) g_free, NULL);
    g_ptr_array_free (do_executev__argv__captured, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
my_setup (void)
{
    setup ();

    execute_get_external_cmd_opts_from_config__init ();
    do_executev__init ();
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
my_teardown (void)
{
    do_executev__deinit ();
    execute_get_external_cmd_opts_from_config__deinit ();

    teardown ();
}

/* --------------------------------------------------------------------------------------------- */

/* @Test */
/* *INDENT-OFF* */
START_TEST (do_open_external_editor_or_viewer)
/* *INDENT-ON* */
{
    /* given */
    vfs_path_t *filename_vpath;
    filename_vpath = vfs_path_from_str ("/path/to/file.txt");

    vfs_file_is_local__return_value = TRUE;
    execute_external_cmd_opts__return_value =
        g_strdup
        (" 'param 1 with spaces' \"param 2\"           -a -b -cdef /path/to/file.txt +123");

    /* when */
    execute_external_editor_or_viewer ("editor_or_viewer", filename_vpath, 123);

    /* then */

    /* check call to execute_get_external_cmd_opts_from_config() */
    mctest_assert_str_eq (execute_external_cmd_opts__command__captured, "editor_or_viewer");
    mctest_assert_int_eq (vfs_path_equal
                          (execute_external_cmd_opts__filename_vpath__captured, filename_vpath),
                          TRUE);
    mctest_assert_int_eq (execute_external_cmd_opts__start_line__captured, 123);

    /* check call to do_executev() */
    mctest_assert_str_eq (do_executev__lc_shell__captured, "editor_or_viewer");
    mctest_assert_int_eq (do_executev__flags__captured, EXECUTE_INTERNAL);
    mctest_assert_int_eq (do_executev__argv__captured->len, 7);

    mctest_assert_str_eq (g_ptr_array_index (do_executev__argv__captured, 0),
                          "param 1 with spaces");
    mctest_assert_str_eq (g_ptr_array_index (do_executev__argv__captured, 1), "param 2");
    mctest_assert_str_eq (g_ptr_array_index (do_executev__argv__captured, 2), "-a");
    mctest_assert_str_eq (g_ptr_array_index (do_executev__argv__captured, 3), "-b");
    mctest_assert_str_eq (g_ptr_array_index (do_executev__argv__captured, 4), "-cdef");
    mctest_assert_str_eq (g_ptr_array_index (do_executev__argv__captured, 5), "/path/to/file.txt");
    mctest_assert_str_eq (g_ptr_array_index (do_executev__argv__captured, 6), "+123");

    vfs_path_free (filename_vpath);
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

    tcase_add_checked_fixture (tc_core, my_setup, my_teardown);

    /* Add new tests here: *************** */
    tcase_add_test (tc_core, do_open_external_editor_or_viewer);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "execute__execute_external_editor_or_viewer.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
