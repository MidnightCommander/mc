/*
   lib - realpath

   Copyright (C) 2017-2024
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

    /* realpath(3) produces a canonicalized absolute pathname using current directory.
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
    TCase *tc_core;
    char *cwd;

    tc_core = tcase_create ("Core");

    /* writable directory where check creates temporary files */
    cwd = g_get_current_dir ();
    g_setenv ("TEMP", cwd, TRUE);
    g_free (cwd);

    tcase_add_checked_fixture (tc_core, setup, teardown);

    /* Add new tests here: *************** */
    mctest_add_parameterized_test (tc_core, realpath_test, data_source);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
