/*
   lib/vfs - test vfs_prefix_to_class() functionality

   Copyright (C) 2011, 2013
   The Free Software Foundation, Inc.

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

#include "lib/strutil.h"
#include "lib/vfs/xdirentry.h"
#include "lib/vfs/vfs.c"        /* for testing static methods  */

#include "src/vfs/local/local.c"

struct vfs_s_subclass test_subclass1, test_subclass2, test_subclass3;
struct vfs_class vfs_test_ops1, vfs_test_ops2, vfs_test_ops3;

/* --------------------------------------------------------------------------------------------- */

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

/* --------------------------------------------------------------------------------------------- */

/* @Before */
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

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_vfs_prefix_to_class_ds") */
/* *INDENT-OFF* */
static const struct test_vfs_prefix_to_class_ds
{
    const char *input_string;
    const struct vfs_class *expected_result;
} test_vfs_prefix_to_class_ds[] =
{
    { /* 0 */
        "test_1:",
        &vfs_test_ops1
    },
    { /* 1 */
        "test_2:",
        &vfs_test_ops1
    },
    { /* 2 */
        "test_3:",
        &vfs_test_ops1
    },
    { /* 3 */
        "test_4:",
        &vfs_test_ops1
    },
    { /* 4 */
        "test2:",
        &vfs_test_ops2
    },
    { /* 5 */
        "test3:",
        &vfs_test_ops3
    },
    {
        "test1:",
        NULL
    },
    { /* 6 */
        "test_5:",
        NULL
    },
    { /* 7 */
        "test4:",
        NULL
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_vfs_prefix_to_class_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_vfs_prefix_to_class, test_vfs_prefix_to_class_ds)
/* *INDENT-ON* */
{
    /* given */
    struct vfs_class *actual_result;

    /* when */
    actual_result = vfs_prefix_to_class ((char *) data->input_string);

    /* then */
    mctest_assert_ptr_eq (actual_result, data->expected_result);
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
    mctest_add_parameterized_test (tc_core, test_vfs_prefix_to_class, test_vfs_prefix_to_class_ds);
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
