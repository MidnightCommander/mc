/*
   lib/vfs - test vfs_prefix_to_class() functionality

   Copyright (C) 2011
   The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2011

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

#include <config.h>

#include <check.h>

#include "lib/global.h"
#include "lib/strutil.h"
#include "lib/vfs/xdirentry.h"
#include "lib/vfs/vfs.c"        /* for testing static methods  */

#include "src/vfs/local/local.c"

struct vfs_s_subclass test_subclass1, test_subclass2, test_subclass3;
struct vfs_class vfs_test_ops1, vfs_test_ops2, vfs_test_ops3;


static int
test_which (struct vfs_class *me, const char *path)
{
    (void) me;

    if ((strcmp (path, "test_1:") == 0) ||
        (strcmp (path, "test_2:") == 0) ||
        (strcmp (path, "test_3:") == 0) || (strcmp (path, "test_4:") == 0))
        return 1;
    return -1;
}

static void
setup (void)
{

    str_init_strings (NULL);

    vfs_init ();
    init_localfs ();
    vfs_setup_work_dir ();


    test_subclass1.flags = VFS_S_REMOTE;
    vfs_s_init_class (&vfs_test_ops1, &test_subclass1);
    vfs_test_ops1.name = "testfs1";
    vfs_test_ops1.flags = VFSF_NOLINKS;
    vfs_test_ops1.prefix = "test1:";
    vfs_test_ops1.which = test_which;
    vfs_register_class (&vfs_test_ops1);

    vfs_s_init_class (&vfs_test_ops2, &test_subclass2);
    vfs_test_ops2.name = "testfs2";
    vfs_test_ops2.prefix = "test2:";
    vfs_register_class (&vfs_test_ops2);

    vfs_s_init_class (&vfs_test_ops3, &test_subclass3);
    vfs_test_ops3.name = "testfs3";
    vfs_test_ops3.prefix = "test3:";
    vfs_register_class (&vfs_test_ops3);

}

static void
teardown (void)
{
    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */
START_TEST (test_vfs_prefix_to_class_valid)
/* *INDENT-ON* */
{
    fail_unless (vfs_prefix_to_class ((char *) "test_1:") == &vfs_test_ops1,
                 "'test_1:' doesn't transform to vfs_test_ops1");
    fail_unless (vfs_prefix_to_class ((char *) "test_2:") == &vfs_test_ops1,
                 "'test_2:' doesn't transform to vfs_test_ops1");
    fail_unless (vfs_prefix_to_class ((char *) "test_3:") == &vfs_test_ops1,
                 "'test_3:' doesn't transform to vfs_test_ops1");
    fail_unless (vfs_prefix_to_class ((char *) "test_4:") == &vfs_test_ops1,
                 "'test_4:' doesn't transform to vfs_test_ops1");

    fail_unless (vfs_prefix_to_class ((char *) "test2:") == &vfs_test_ops2,
                 "'test2:' doesn't transform to vfs_test_ops2");

    fail_unless (vfs_prefix_to_class ((char *) "test3:") == &vfs_test_ops3,
                 "'test3:' doesn't transform to vfs_test_ops3");
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */
START_TEST (test_vfs_prefix_to_class_invalid)
/* *INDENT-ON* */
{
    fail_unless (vfs_prefix_to_class ((char *) "test1:") == NULL,
                 "'test1:' doesn't transform to NULL");
    fail_unless (vfs_prefix_to_class ((char *) "test_5:") == NULL,
                 "'test_5:' doesn't transform to NULL");
    fail_unless (vfs_prefix_to_class ((char *) "test4:") == NULL,
                 "'test4:' doesn't transform to NULL");
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
    tcase_add_test (tc_core, test_vfs_prefix_to_class_valid);
    tcase_add_test (tc_core, test_vfs_prefix_to_class_invalid);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "vfs_prefix_to_class.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
