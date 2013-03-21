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
START_TEST (test_mk_rm_dir)
/* *INDENT-ON* */
{
    /* given */
    int actual_result;
    vfs_path_t *input_dir;

    input_dir =
        vfs_path_from_str ("smb://smbUser:smbPass@MCTESTSERVER/SHARE/mk_rm_dir_test");

    /* when */
    actual_result = mc_mkdir (input_dir, 0755);

    /* then */
    mc_rmdir (input_dir);
    vfs_path_free (input_dir);

    mctest_assert_int_eq (actual_result, 0);
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* @Test */
/* *INDENT-OFF* */
START_TEST (test_stat_dir)
/* *INDENT-ON* */
{
    /* given */
    int actual_result;
    vfs_path_t *input_dir;
    struct stat actual_stat;

    input_dir =
        vfs_path_from_str ("smb://smbUser:smbPass@MCTESTSERVER/SHARE/stat_dir_test");

    mc_mkdir (input_dir, 0755);

    /* when */
    actual_result = mc_lstat (input_dir, &actual_stat);

    /* then */
    mc_rmdir (input_dir);
    vfs_path_free (input_dir);

    mctest_assert_int_eq (actual_result, 0);
    mctest_assert_int_eq (S_ISDIR (actual_stat.st_mode), 1);
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
    tcase_add_test (tc_core, test_mk_rm_dir);
    tcase_add_test (tc_core, test_stat_dir);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "mkdir_rmdir_stat_test.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
