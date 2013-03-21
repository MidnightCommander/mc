/*
   src/vfs/smbfs - tests for opendir function

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

#define TEST_SUITE_NAME "/src/vfs/smbfs"

#include "tests/mctest.h"
#include <errno.h>
#include <locale.h>

#include "lib/vfs/vfs.h"
#include "lib/strutil.h"

#include "src/vfs/smbfs/internal.h"

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    str_init_strings (NULL);
    setlocale (LC_MESSAGES, "POSIX");
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_smbfs_strerror_transform_ds") */
/* *INDENT-OFF* */
static const struct test_smbfs_strerror_transform_ds
{
    const int input_errno;
} test_smbfs_strerror_transform_ds[] =
{
    { /* 0. */
        EDEADLK
    },
    { /* 1. */
        ENAMETOOLONG
    },
    { /* 2. */
        EAGAIN
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_smbfs_strerror_transform_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_smbfs_strerror_transform, test_smbfs_strerror_transform_ds)
/* *INDENT-ON* */
{
    /* given */
    const char *actual_result;

    /* when */
    actual_result = smbfs_strerror (data->input_errno);

    /* then */

    fail_unless (strncmp (actual_result, "smbfs: ", 7) == 0,
                 "actual_result(%s) pointer should be started with the 'smbfs: ' string\n",
                 actual_result);
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_smbfs_strerror_ds") */
/* *INDENT-OFF* */
static const struct test_smbfs_strerror_ds
{
    const int input_errno;
    const char *expected_result;
} test_smbfs_strerror_ds[] =
{
    { /* 0. */
        EINVAL,
        "smbfs: an incorrect form of file/URL was passed"
    },
    { /* 1. */
        EPERM,
        "smbfs: the workgroup or server could not be found"
    },
    { /* 2. */
        ENODEV,
        "smbfs: the workgroup or server could not be found"
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_smbfs_strerror_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_smbfs_strerror, test_smbfs_strerror_ds)
/* *INDENT-ON* */
{
    /* given */
    const char *actual_result;

    /* when */
    actual_result = smbfs_strerror (data->input_errno);

    /* then */
    mctest_assert_str_eq (actual_result, data->expected_result);
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
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
    mctest_add_parameterized_test (tc_core, test_smbfs_strerror_transform,
                                   test_smbfs_strerror_transform_ds);
    mctest_add_parameterized_test (tc_core, test_smbfs_strerror, test_smbfs_strerror_ds);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "smbfs_strerror.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
