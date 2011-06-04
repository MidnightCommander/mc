/* lib/vfs - get vfs class from string

   Copyright (C) 2011 Free Software Foundation, Inc.

   Written by:
    Slava Zanko <slavazanko@gmail.com>, 2011

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#define TEST_SUITE_NAME "/lib/vfs"

#include <check.h>


#include "lib/global.h"
#include "lib/strutil.h"
#include "lib/vfs/xdirentry.h"
#include "lib/vfs/vfs.c" /* for testing static methods  */

#include "src/vfs/local/local.c"

static void
setup (void)
{
    str_init_strings (NULL);

    vfs_init ();
    init_localfs ();
    vfs_setup_work_dir ();
}

static void
teardown (void)
{
    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_register_vfs_class)
{
    static struct vfs_s_subclass test_subclass;
    static struct vfs_class vfs_test_ops;

    test_subclass.flags = VFS_S_REMOTE;
    vfs_s_init_class (&vfs_test_ops, &test_subclass);

    vfs_test_ops.name = "testfs";
    vfs_test_ops.flags = VFSF_NOLINKS;
    vfs_test_ops.prefix = "test:";
    vfs_register_class (&vfs_test_ops);

    fail_if (vfs__classes_list->len != 2, "Failed to register test VFS module");;

    {
        struct vfs_class *result;
        result = vfs_get_class("/#test://bla-bla/some/path");
        fail_if(result == NULL, "VFS module not found!");
        fail_unless(result == &vfs_test_ops, "Result(%p)  don't match to vfs_test_ops(%p)!", result, &vfs_test_ops);
    }


    {
        struct vfs_class *result;
        result = vfs_get_class("/#test://bla-bla/some/path/#test_not_exists://bla-bla/some/path");
        fail_if(result == NULL, "VFS module not found!");
        fail_unless(result == &vfs_test_ops, "Result(%p)  don't match to vfs_test_ops(%p)!", result, &vfs_test_ops);
    }

}
END_TEST

/* --------------------------------------------------------------------------------------------- */
START_TEST (test_register_vfs_class2)
{
    static struct vfs_s_subclass test_subclass1, test_subclass2;
    static struct vfs_class vfs_test_ops1, vfs_test_ops2;

    test_subclass1.flags = VFS_S_REMOTE;
    vfs_s_init_class (&vfs_test_ops1, &test_subclass1);

    vfs_test_ops1.name = "testfs1";
    vfs_test_ops1.flags = VFSF_NOLINKS;
    vfs_test_ops1.prefix = "test1:";
    vfs_register_class (&vfs_test_ops1);

    vfs_s_init_class (&vfs_test_ops2, &test_subclass2);

    vfs_test_ops2.name = "testfs2";
    vfs_test_ops2.prefix = "test2:";
    vfs_register_class (&vfs_test_ops2);


    fail_if (vfs__classes_list->len != 3, "Failed to register test VFS module");;

    {
        struct vfs_class *result;
        result = vfs_get_class("/#test1://bla-bla/some/path");
        fail_if(result == NULL, "VFS module not found!");
        fail_unless(result == &vfs_test_ops1, "Result(%p)  don't match to vfs_test_ops1(%p)!", result, &vfs_test_ops1);
    }


    {
        struct vfs_class *result;
        result = vfs_get_class("/#test2://bla-bla/some/path/#test1://bla-bla/some/path");
        fail_if(result == NULL, "VFS module not found!");
        fail_unless(result == &vfs_test_ops1, "Result(%p)  don't match to vfs_test_ops1(%p)!", result, &vfs_test_ops1);
    }

    {
        struct vfs_class *result;
        result = vfs_get_class("/#test1://bla-bla/some/path/#test2://bla-bla/some/path");
        fail_if(result == NULL, "VFS module not found!");
        fail_unless(result == &vfs_test_ops2, "Result(%p)  don't match to vfs_test_ops2(%p)!", result, &vfs_test_ops2);
    }

}
END_TEST

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
    tcase_add_test (tc_core, test_register_vfs_class);
    tcase_add_test (tc_core, test_register_vfs_class2);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "get_vfs_class.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
