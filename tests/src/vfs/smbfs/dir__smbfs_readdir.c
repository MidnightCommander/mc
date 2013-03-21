/*
   src/vfs/smbfs - tests for readdir function

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

#include "lib/vfs/vfs.h"
#include "lib/strutil.h"

#include "src/vfs/local/local.h"
#include "src/vfs/smbfs/init.h"
#include "src/vfs/smbfs/internal.h"

typedef struct
{
    int handle;
    smbfs_super_data_t *super_data;
} smbfs_dir_data_t;

/* --------------------------------------------------------------------------------------------- */

static int smbfs_readdir__errno;

/* @CapturedValue */
static int smbc_readdir__handle__captured;
/* @ThenReturnValue */
static struct smbc_dirent *smbc_readdir__return_value;

/* @Mock */
struct smbc_dirent *
smbc_readdir (unsigned int handle)
{
    smbc_readdir__handle__captured = handle;
    errno = smbfs_readdir__errno;

    return smbc_readdir__return_value;
}

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    str_init_strings (NULL);

    vfs_init ();
    init_localfs ();
    init_smbfs ();

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

/* @Test */
/* *INDENT-OFF* */
START_TEST (test_smbfs_read_dir_fail)
/* *INDENT-ON* */
{
    /* given */
    union vfs_dirent *actual_result;
    GError *error = NULL;
    smbfs_dir_data_t input_smbfs_dir_data;

    input_smbfs_dir_data.handle = 12345;

    /* prepare mocked functions */
    smbc_readdir__return_value = NULL;

    /* when */
    smbfs_readdir__errno = ENODEV;
    actual_result = smbfs_readdir (&input_smbfs_dir_data, &error);

    /* then */

    /* checking mock calls */
    mctest_assert_int_eq (smbc_readdir__handle__captured, 12345);

    /* checking actual data */
    mctest_assert_ptr_eq (actual_result, NULL);

    /* checking error object */
    mctest_assert_ptr_ne (error, NULL);

    g_error_free (error);
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* @Test */
/* *INDENT-OFF* */
START_TEST (test_smbfs_read_dir_success)
/* *INDENT-ON* */
{
    /* given */
    union vfs_dirent *actual_result;
    GError *error = NULL;
    smbfs_dir_data_t input_smbfs_dir_data;
    struct smbc_dirent return_value;

    input_smbfs_dir_data.handle = 12345;
    strcpy (return_value.name, "file.ext");

    /* prepare mocked functions */
    smbc_readdir__return_value = &return_value;

    /* when */

    smbfs_readdir__errno = 0;
    actual_result = smbfs_readdir (&input_smbfs_dir_data, &error);

    /* then */

    /* checking mock calls */
    mctest_assert_int_eq (smbc_readdir__handle__captured, 12345);

    /* checking actual data */
    mctest_assert_ptr_ne (actual_result, NULL);
    mctest_assert_str_eq (actual_result->dent.d_name, "file.ext");

    /* checking error object */
    mctest_assert_ptr_eq (error, NULL);
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
    tcase_add_test (tc_core, test_smbfs_read_dir_fail);
    tcase_add_test (tc_core, test_smbfs_read_dir_success);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "smbfs_readdir.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
