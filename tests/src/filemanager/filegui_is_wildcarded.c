/*
   src/filemanager - tests for is_wildcarded() function

   Copyright (C) 2011-2024
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2015

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

#include "src/vfs/local/local.c"

#include "src/filemanager/filegui.c"


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

/* @DataSource("test_is_wildcarded_ds") */
/* *INDENT-OFF* */
static const struct test_is_wildcarded_ds
{
    const char *input_value;
    gboolean expected_result;
} test_is_wildcarded_ds[] =
{
    { /* 0 */
        "blabla",
        FALSE
    },
    { /* 1 */
        "bla?bla",
        TRUE
    },
    { /* 2 */
        "bla*bla",
        TRUE
    },
    { /* 3 */
        "bla\\*bla",
        FALSE
    },

    { /* 4 */
        "bla\\\\*bla",
        TRUE
    },
    { /* 5 */
        "bla\\1bla",
        TRUE
    },
    { /* 6 */
        "bla\\\\1bla",
        FALSE
    },
    { /* 7 */
        "bla\\\t\\\\1bla",
        FALSE
    },
    { /* 8 */
        "bla\\\t\\\\\\1bla",
        TRUE
    },
    { /* 9 */
        "bla\\9bla",
        TRUE
    },
    { /* 10 */
        "blabla\\",
        FALSE
    },
    { /* 11 */
        "blab\\?la",
        FALSE
    },
    { /* 12 */
        "blab\\\\?la",
        TRUE
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_is_wildcarded_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_is_wildcarded, test_is_wildcarded_ds)
/* *INDENT-ON* */
{
    /* given */
    gboolean actual_result;

    /* when */
    actual_result = is_wildcarded (data->input_value);
    /* then */
    ck_assert_int_eq (actual_result, data->expected_result);
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
    mctest_add_parameterized_test (tc_core, test_is_wildcarded, test_is_wildcarded_ds);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
