/*
   src/vfs/smbfs - tests for stat functions

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

#include "lib/vfs/vfs.h"
#include "lib/strutil.h"

#include "src/vfs/local/local.h"
#include "src/vfs/smbfs/init.h"
#include "src/vfs/smbfs/internal.h"

/* --------------------------------------------------------------------------------------------- */


/* @CapturedValue */
static char *smbc_stat__smb_url__captured;
/* @CapturedValue */
static struct stat *smbc_stat__st__captured;
/* @ThenReturnValue */
static int smbc_stat__return_value;

/* @Mock */
int
smbc_stat (const char *smb_url, struct stat *st)
{
    smbc_stat__smb_url__captured = g_strdup (smb_url);
    smbc_stat__st__captured = st;

    return smbc_stat__return_value;
}

static void
smbc_stat__init (void)
{
    smbc_stat__smb_url__captured = NULL;
}

static void
smbc_stat__deinit (void)
{
    g_free (smbc_stat__smb_url__captured);
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

    smbc_stat__init ();
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    smbc_stat__deinit ();

    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* @Test */
/* *INDENT-OFF* */
START_TEST (test_smbfs_stat_fail)
/* *INDENT-ON* */
{
    /* given */
    int actual_result;
    GError *error = NULL;
    vfs_path_t *input_vpath;
    struct stat st;

    input_vpath = vfs_path_from_str ("/smb:/someserver/path/to/file.ext");      /* wrong URL. Should be smb://... */

    /* prepare mocked functions */
    smbc_stat__return_value = -1;

    /* when */

    actual_result = smbfs_stat (input_vpath, &st, &error);

    /* then */

    /* checking mock calls */
    mctest_assert_str_eq (smbc_stat__smb_url__captured, "smb:////smb:/someserver/path/to/file.ext");
    mctest_assert_ptr_eq (smbc_stat__st__captured, &st);

    /* checking actual data */
    mctest_assert_int_ne (actual_result, 0);

    /* checking error object */
    mctest_assert_ptr_ne (error, NULL);

    g_error_free (error);
    vfs_path_free (input_vpath);
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* @Test */
/* *INDENT-OFF* */
START_TEST (test_smbfs_stat_success)
/* *INDENT-ON* */
{
    /* given */
    int actual_result;
    GError *error = NULL;
    vfs_path_t *input_vpath;
    struct stat st;

    input_vpath = vfs_path_from_str ("smb://someserver/path/to/file.ext");

    /* prepare mocked functions */
    smbc_stat__return_value = 0;

    /* when */

    actual_result = smbfs_stat (input_vpath, &st, &error);

    /* then */

    /* checking mock calls */
    mctest_assert_str_eq (smbc_stat__smb_url__captured, "smb://someserver/path/to/file.ext");
    mctest_assert_ptr_eq (smbc_stat__st__captured, &st);

    /* checking actual data */
    mctest_assert_int_eq (actual_result, 0);

    /* checking error object */
    mctest_assert_ptr_eq (error, NULL);

    vfs_path_free (input_vpath);
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
    tcase_add_test (tc_core, test_smbfs_stat_fail);
    tcase_add_test (tc_core, test_smbfs_stat_success);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "smbfs_opendir.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
