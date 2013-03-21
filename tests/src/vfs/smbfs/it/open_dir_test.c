/*
   src/vfs/smbfs - integration tests for smbfs.
   You should set up your environment as it described in samba.stories
   file (in the Background section).

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

#define TEST_SUITE_NAME "/src/vfs/smbfs/it"

#include "tests/mctest.h"

#include "lib/vfs/vfs.h"
#include "lib/strutil.h"

#include "src/vfs/local/local.h"
#include "src/vfs/smbfs/init.h"

gboolean vfs_show_gerror (GError ** error);

gboolean
vfs_show_gerror (GError ** error)
{
    if (error && *error)
        fprintf (stderr, "ERROR: %s\n", (*error)->message);
    return TRUE;
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

/* @DataSource("test_open_dir_ds") */
/* *INDENT-OFF* */
static const struct test_open_dir_ds
{
    const char *cd_directory;
} test_open_dir_ds[] =
{
    { /* 0. */
        "smb://MCTESTSERVER"
    },
    { /* 1. */
        "smb://WORKGROUP"
    },
    { /* 2. */
        "smb://MCTESTSERVER/SHARE"
    },
    { /* 3. */
        "smb://smbUser:smbPass@MCTESTSERVER/RESTRICTED_SHARE"
    },
    { /* 4. */
        "smb://smbUser:smbPass@MCTESTSERVER/SHARE"
    },
    { /* 5. */
        "smb://WORKGROUP;smbUser:smbPass@MCTESTSERVER/RESTRICTED_SHARE"
    },
    { /* 6. */
        "smb://WORKGROUP;smbUser:smbPass@MCTESTSERVER/SHARE"
    },
/* TODO: investigate why an error occured while I tried to see all samba resources
    {
        "smb://"
    },
*/
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_open_dir_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_open_dir, test_open_dir_ds)
/* *INDENT-ON* */
{
    /* given */
    DIR *dir_handle;

    /* when */
    dir_handle = mc_opendir (vfs_path_from_str (data->cd_directory));

    /* then */
    mctest_assert_ptr_ne (dir_handle, NULL);
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
    mctest_add_parameterized_test (tc_core, test_open_dir, test_open_dir_ds);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "open_dir_test.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
