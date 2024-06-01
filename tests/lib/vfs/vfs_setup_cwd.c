/*
   lib/vfs - test vfs_setup_cwd() functionality

   Copyright (C) 2013-2024
   Free Software Foundation, Inc.

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

#define TEST_SUITE_NAME "/lib/vfs"

#include "tests/mctest.h"

#include <stdlib.h>

#include "lib/strutil.h"
#include "lib/vfs/xdirentry.h"
#include "src/vfs/local/local.c"

/* --------------------------------------------------------------------------------------------- */

/* @Mock */
char *
g_get_current_dir (void)
{
    return g_strdup ("/some/path");
}

/* --------------------------------------------------------------------------------------------- */

static gboolean mc_stat__is_2nd_call_different = FALSE;
static gboolean mc_stat__call_count = 0;

/* @Mock */
int
mc_stat (const vfs_path_t *vpath, struct stat *my_stat)
{
    (void) vpath;

    if (mc_stat__call_count++ > 1 && mc_stat__is_2nd_call_different)
    {
        my_stat->st_ino = 2;
        my_stat->st_dev = 22;
    }
    else
    {
        my_stat->st_ino = 1;
        my_stat->st_dev = 11;
    }
    if (mc_stat__call_count > 2)
    {
        mc_stat__call_count = 0;
    }
    return 0;
}

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

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_vfs_setup_cwd_symlink_ds") */
/* *INDENT-OFF* */
static const struct test_vfs_setup_cwd_symlink_ds
{
    gboolean is_2nd_call_different;
    const char *expected_result;
} test_vfs_setup_cwd_symlink_ds[] =
{
    { /* 0. */
        TRUE,
        "/some/path"
    },
    { /* 1. */
        FALSE,
        "/some/path2"
    },
};
/* *INDENT-ON* */

/* @Test */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_vfs_setup_cwd_symlink, test_vfs_setup_cwd_symlink_ds)
/* *INDENT-ON* */
{
    /* given */
    vfs_set_raw_current_dir (NULL);
    mc_stat__is_2nd_call_different = data->is_2nd_call_different;
    mc_stat__call_count = 0;
    setenv ("PWD", "/some/path2", 1);

    /* when */
    vfs_setup_cwd ();

    /* then */
    mctest_assert_str_eq (vfs_get_current_dir (), data->expected_result);
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    /* Add new tests here: *************** */
    mctest_add_parameterized_test (tc_core, test_vfs_setup_cwd_symlink,
                                   test_vfs_setup_cwd_symlink_ds);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
