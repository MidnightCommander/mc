/*
   lib/vfs - test vfs_split() functionality

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
#include "lib/vfs/path.c"       /* for testing static methods  */

#include "src/vfs/local/local.c"

struct vfs_s_subclass test_subclass1, test_subclass2, test_subclass3;
struct vfs_class vfs_test_ops1, vfs_test_ops2, vfs_test_ops3;

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

/* @DataSource("test_vfs_split_ds") */
/* *INDENT-OFF* */
static const struct test_vfs_split_ds
{
    const char *input_string;
    const char *expected_path;
    const char *expected_local;
    const char *expected_op;
    const struct vfs_class *expected_result;
} test_vfs_split_ds[] =
{
    { /* 0. */
        "#test1:/bla-bla/some/path/#test2:/bla-bla/some/path2/#test3:/qqq/www/eee.rr",
        "#test1:/bla-bla/some/path/#test2:/bla-bla/some/path2/",
        "qqq/www/eee.rr",
        "test3:",
        &vfs_test_ops3
    },
    { /* 1. */
        "#test1:/bla-bla/some/path/#test2:/bla-bla/some/path2/",
        "#test1:/bla-bla/some/path/",
        "bla-bla/some/path2/",
        "test2:",
        &vfs_test_ops2
    },
    { /* 2. */
        "#test1:/bla-bla/some/path/",
        "",
        "bla-bla/some/path/",
        "test1:",
        &vfs_test_ops1
    },
    { /* 3. */
        "",
        "",
        NULL,
        NULL,
        NULL
    },
    { /* 4. split with local */
        "/local/path/#test1:/bla-bla/some/path/#test2:/bla-bla/some/path2#test3:/qqq/www/eee.rr",
        "/local/path/#test1:/bla-bla/some/path/#test2:/bla-bla/some/path2",
        "qqq/www/eee.rr",
        "test3:",
        &vfs_test_ops3
    },
    { /* 5. split with local */
        "/local/path/#test1:/bla-bla/some/path/#test2:/bla-bla/some/path2",
        "/local/path/#test1:/bla-bla/some/path/",
        "bla-bla/some/path2",
        "test2:",
        &vfs_test_ops2,
    },
    { /* 6. split with local */
        "/local/path/#test1:/bla-bla/some/path/",
        "/local/path/",
        "bla-bla/some/path/",
        "test1:",
        &vfs_test_ops1
    },
    { /* 7. split with local */
        "/local/path/",
        "/local/path/",
        NULL,
        NULL,
        NULL
    },
    { /* 8. split with URL */
        "#test2:username:passwd@somehost.net/bla-bla/some/path2",
        "",
        "bla-bla/some/path2",
        "test2:username:passwd@somehost.net",
        &vfs_test_ops2
    },
    { /* 9. split URL with semi */
        "/local/path/#test1:/bla-bla/some/path/#test2:username:p!a@s#s$w%d@somehost.net/bla-bla/some/path2",
        "/local/path/#test1:/bla-bla/some/path/",
        "bla-bla/some/path2",
        "test2:username:p!a@s#s$w%d@somehost.net",
        &vfs_test_ops2
    },
    { /* 10. split with semi in path */
        "#test2:/bl#a-bl#a/so#me/pa#th2",
        "",
        "bl#a-bl#a/so#me/pa#th2",
        "test2:",
        &vfs_test_ops2
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_vfs_split_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_vfs_split, test_vfs_split_ds)
/* *INDENT-ON* */
{
    /* given */
    const char *local = NULL, *op = NULL;
    struct vfs_class *actual_result;
    char *path;

    path = g_strdup (data->input_string);

    /* when */
    actual_result = _vfs_split_with_semi_skip_count (path, &local, &op, 0);

    /* then */
    mctest_assert_ptr_eq (actual_result, data->expected_result);
    mctest_assert_str_eq (path, data->expected_path);
    mctest_assert_str_eq (local, data->expected_local);
    mctest_assert_str_eq (op, data->expected_op);
    g_free (path);
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
    mctest_add_parameterized_test (tc_core, test_vfs_split, test_vfs_split_ds);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "vfs_split.log");
    srunner_run_all (sr, CK_NOFORK);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
