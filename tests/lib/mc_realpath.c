/*
   lib - realpath

   Copyright (C) 2017-2020
   Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2017

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

#define TEST_SUITE_NAME "/lib/util"

#include "tests/mctest.h"

#include "lib/strutil.h"
#include "lib/vfs/vfs.h"        /* VFS_ENCODING_PREFIX, vfs_init(), vfs_shut() */
#include "src/vfs/local/local.c"

#include "lib/util.h"           /* mc_realpath() */

/* --------------------------------------------------------------------------------------------- */

static char resolved_path[PATH_MAX];

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    mc_global.timer = mc_timer_new ();
    str_init_strings (NULL);
    vfs_init ();
    vfs_init_localfs ();
    vfs_setup_work_dir ();
}

/* @After */
static void
teardown (void)
{
    vfs_shut ();
    str_uninit_strings ();
    mc_timer_destroy (mc_global.timer);
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("data_source") */
/* *INDENT-OFF* */
static const struct data_source
{
    const char *input_string;
    const char *expected_string;
} data_source[] =
{
    /* absolute paths */
    { "/", "/"},
    { "/" VFS_ENCODING_PREFIX "UTF-8/", "/" },
    { "/usr/bin", "/usr/bin" },
    { "/" VFS_ENCODING_PREFIX "UTF-8/usr/bin", "/usr/bin" },

    /* relative paths are relative to / */
    { VFS_ENCODING_PREFIX "UTF-8/", "/" },
    { "usr/bin", "/usr/bin" },
    { VFS_ENCODING_PREFIX "UTF-8/usr/bin", "/usr/bin" }
};
/* *INDENT-ON* */

/* @Test(dataSource = "data_source") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (realpath_test, data_source)
/* *INDENT-ON* */
{
    int ret;

    /* realpath(3) produces a canonicalized absolute pathname using curent directory.
     * Change the current directory to produce correct pathname. */
    ret = chdir ("/");

    /* when */
    if (mc_realpath (data->input_string, resolved_path) == NULL)
        resolved_path[0] = '\0';

    /* then */
    mctest_assert_str_eq (resolved_path, data->expected_string);

    (void) ret;
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    int number_failed;
    char *cwd;

    Suite *s = suite_create (TEST_SUITE_NAME);
    TCase *tc_core = tcase_create ("Core");
    SRunner *sr;

    /* writable directory where check creates temporary files */
    cwd = g_get_current_dir ();
    g_setenv ("TEMP", cwd, TRUE);
    g_free (cwd);

    tcase_add_checked_fixture (tc_core, setup, teardown);

    /* Add new tests here: *************** */
    mctest_add_parameterized_test (tc_core, realpath_test, data_source);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "mc_realpath.log");
    srunner_run_all (sr, CK_ENV);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* --------------------------------------------------------------------------------------------- */
