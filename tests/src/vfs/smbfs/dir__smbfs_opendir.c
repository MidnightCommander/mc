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

#include "lib/vfs/vfs.h"
#include "lib/strutil.h"

#include "src/vfs/local/local.h"
#include "src/vfs/smbfs/init.h"
#include "src/vfs/smbfs/internal.h"

/* --------------------------------------------------------------------------------------------- */


/* @CapturedValue */
static char *smbc_opendir__smb_url__captured;
/* @ThenReturnValue */
static int smbc_opendir__return_value;

/* @Mock */
int
smbc_opendir (const char *smb_url)
{
    smbc_opendir__smb_url__captured = g_strdup (smb_url);

    return smbc_opendir__return_value;
}

static void
smbc_opendir__init (void)
{
    smbc_opendir__smb_url__captured = NULL;
    smbc_opendir__return_value = -1;
}

static void
smbc_opendir__deinit (void)
{
    g_free (smbc_opendir__smb_url__captured);
}


/* --------------------------------------------------------------------------------------------- */

/* @CapturedValue */
static vfs_path_t *vfs_get_super_by_vpath__vpath__captured;
/* @CapturedValue */
static gboolean vfs_get_super_by_vpath__is_create_new__captured;

/* @ThenReturnValue */
static struct vfs_s_super *vfs_get_super_by_vpath__return_value;

/* @Mock */
struct vfs_s_super *
vfs_get_super_by_vpath (const vfs_path_t * vpath, gboolean is_create_new)
{
    vfs_get_super_by_vpath__vpath__captured = vfs_path_clone (vpath);
    vfs_get_super_by_vpath__is_create_new__captured = is_create_new;
    return vfs_get_super_by_vpath__return_value;
}

static void
vfs_get_super_by_vpath__init (void)
{
    vfs_get_super_by_vpath__vpath__captured = NULL;
    vfs_get_super_by_vpath__is_create_new__captured = FALSE;
    vfs_get_super_by_vpath__return_value = NULL;
}

static void
vfs_get_super_by_vpath__deinit (void)
{
    vfs_path_free (vfs_get_super_by_vpath__vpath__captured);
    g_free (vfs_get_super_by_vpath__return_value);
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

    vfs_get_super_by_vpath__init ();
    smbc_opendir__init ();
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    smbc_opendir__deinit ();
    vfs_get_super_by_vpath__deinit ();

    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* @Test */
/* *INDENT-OFF* */
START_TEST (test_smbfs_open_dir_fail)
/* *INDENT-ON* */
{
    /* given */
    void *actual_result;
    GError *error = NULL;
    vfs_path_t *input_vpath;

    input_vpath = vfs_path_from_str ("/smb:/someserver/path/to/file.ext");      /* wrong URL. Should be smb://... */

    /* prepare mocked functions */
    vfs_get_super_by_vpath__return_value = g_new0 (struct vfs_s_super, 1);
    smbc_opendir__return_value = -1;

    /* when */

    actual_result = smbfs_opendir (input_vpath, &error);

    /* then */

    /* checking mock calls */
    mctest_assert_ptr_ne (vfs_get_super_by_vpath__vpath__captured, NULL);
    mctest_assert_int_eq (vfs_get_super_by_vpath__is_create_new__captured, TRUE);
    mctest_assert_str_eq (vfs_path_as_str (vfs_get_super_by_vpath__vpath__captured),
                          vfs_path_as_str (input_vpath));

    mctest_assert_str_eq (smbc_opendir__smb_url__captured,
                          "smb:////smb:/someserver/path/to/file.ext");

    /* checking actual data */
    mctest_assert_ptr_eq (actual_result, NULL);

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
START_TEST (test_smbfs_open_dir_success)
/* *INDENT-ON* */
{
    /* given */
    void *actual_result;
    GError *error = NULL;
    vfs_path_t *input_vpath;

    input_vpath = vfs_path_from_str ("smb://someserver/path/to/file.ext");

    /* prepare mocked functions */
    vfs_get_super_by_vpath__return_value = g_new0 (struct vfs_s_super, 1);
    smbc_opendir__return_value = 0;

    /* when */

    actual_result = smbfs_opendir (input_vpath, &error);

    /* then */

    /* checking mock calls */
    mctest_assert_ptr_ne (vfs_get_super_by_vpath__vpath__captured, NULL);
    mctest_assert_int_eq (vfs_get_super_by_vpath__is_create_new__captured, TRUE);
    mctest_assert_str_eq (vfs_path_as_str (vfs_get_super_by_vpath__vpath__captured),
                          vfs_path_as_str (input_vpath));

    mctest_assert_str_eq (smbc_opendir__smb_url__captured, "smb://someserver/path/to/file.ext");

    /* checking actual data */
    mctest_assert_ptr_ne (actual_result, NULL);

    /* checking error object */
    mctest_assert_ptr_eq (error, NULL);

    vfs_path_free (input_vpath);
    g_free (actual_result);
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
    tcase_add_test (tc_core, test_smbfs_open_dir_fail);
    tcase_add_test (tc_core, test_smbfs_open_dir_success);
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
