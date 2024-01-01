/* lib/vfs - tests for vfspath_len() function.

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

#define TEST_SUITE_NAME "/lib/vfs"

#include "tests/mctest.h"

#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#endif

#include "lib/strutil.h"
#include "lib/vfs/xdirentry.h"
#include "lib/vfs/path.h"

#include "src/vfs/local/local.c"

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    str_init_strings (NULL);

    vfs_init ();
    vfs_init_localfs ();
    vfs_setup_work_dir ();

    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;
#ifdef HAVE_CHARSET
    load_codepages_list ();
#endif
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
#ifdef HAVE_CHARSET
    free_codepages_list ();
#endif

    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */
/* @DataSource("test_path_length_ds") */
/* *INDENT-OFF* */
static const struct test_path_length_ds
{
    const char *input_path;
    const size_t expected_length;
} test_path_length_ds[] =
{
    { /* 0. */
        NULL,
        0
    },
    { /* 1. */
        "/",
        1
    },
    { /* 2. */
        "/тестовый/путь",
        26
    },
#ifdef HAVE_CHARSET
    { /* 3. */
        "/#enc:KOI8-R/тестовый/путь",
        38
    },
#endif /* HAVE_CHARSET */
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_path_length_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_path_length, test_path_length_ds)
/* *INDENT-ON* */
{
    /* given */
    vfs_path_t *vpath;
    size_t actual_length;

    vpath = vfs_path_from_str (data->input_path);

    /* when */
    actual_length = vfs_path_len (vpath);

    /* then */
    ck_assert_int_eq (actual_length, data->expected_length);

    vfs_path_free (vpath, TRUE);
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
    mctest_add_parameterized_test (tc_core, test_path_length, test_path_length_ds);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
