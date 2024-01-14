/*
   lib/vfs - test vfs_get_encoding() functionality

   Copyright (C) 2013-2024
   Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2013

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

#include "lib/vfs/path.c"       /* for testing of static vfs_get_encoding() */

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_vfs_get_encoding_ds") */
/* *INDENT-OFF* */
static const struct test_vfs_get_encoding_ds
{
    const char *path;
    const char *expected_encoding;
} test_vfs_get_encoding_ds[] =
{
    { /* 0 */
        "",
        NULL
    },
    { /* 1 */
        "aaaa",
        NULL
    },
    { /* 2 */
        "/aaaa",
        NULL
    },
    { /* 3 */
        "aaaa/bbbb",
        NULL
    },
    { /* 4 */
        "/aaaa/bbbb",
        NULL
    },
    { /* 5 */
        "#enc:UTF-8/aaaa",
        "UTF-8"
    },
    { /* 6 */
        "/#enc:UTF-8/aaaa",
        "UTF-8"
    },
    { /* 7 */
        "/aaaa/#enc:UTF-8/bbbb",
        "UTF-8"
    },
    { /* 8 */
        "/aaaa/#enc:UTF-8/bbbb/#enc:KOI8-R",
        "KOI8-R"
    },
    { /* 9 */
        "/aaaa/#enc:UTF-8/bbbb/#enc:KOI8-R/cccc",
        "KOI8-R"
    },
    { /* 10 */
        "/aaaa/#enc:UTF-8/bbbb/cccc#enc:KOI8-R/dddd",
        "UTF-8"
    },
    { /* 11 */
        "/#enc:UTF-8/bbbb/cccc#enc:KOI8-R/dddd",
        "UTF-8"
    },
    { /* 12 */
        "#enc:UTF-8/bbbb/cccc#enc:KOI8-R/dddd",
        "UTF-8"
    },
    { /* 13 */
        "aaaa#enc:UTF-8/bbbb/cccc#enc:KOI8-R/dddd",
        NULL
    },
    { /* 14 */
        "/aaaa/#enc:UTF-8/bbbb/#enc:KOI8-R#enc:IBM866/cccc",
        "KOI8-R#enc:IBM866"
    }
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_vfs_get_encoding_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_vfs_get_encoding, test_vfs_get_encoding_ds)
/* *INDENT-ON* */
{
    /* given */
    char *actual_encoding;

    /* when */
    actual_encoding = vfs_get_encoding (data->path, -1);

    /* then */
    mctest_assert_str_eq (actual_encoding, data->expected_encoding);

    g_free (actual_encoding);
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
    mctest_add_parameterized_test (tc_core, test_vfs_get_encoding, test_vfs_get_encoding_ds);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
