/*
   lib/vfs - test vfs_split() functionality

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
#include "lib/vfs/path.c"       /* for testing static methods  */

#include "src/vfs/local/local.c"

struct vfs_s_subclass test_subclass1, test_subclass2, test_subclass3;
struct vfs_class vfs_test_ops1, vfs_test_ops2, vfs_test_ops3;

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

static void
teardown (void)
{
    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */
START_TEST (test_vfs_split)
/* *INDENT-ON* */
{
    char *path;
    const char *local, *op, *etalon_path, *etalon_local, *etalon_op;
    struct vfs_class *result;

    path = g_strdup ("#test1:/bla-bla/some/path/#test2:/bla-bla/some/path2/#test3:/qqq/www/eee.rr");

    etalon_path = "#test1:/bla-bla/some/path/#test2:/bla-bla/some/path2/";
    etalon_local = "qqq/www/eee.rr";
    etalon_op = "test3:";
    result = _vfs_split_with_semi_skip_count (path, &local, &op, 0);
    fail_unless (result == &vfs_test_ops3, "Result(%p) doesn't match to vfs_test_ops3(%p)", result,
                 &vfs_test_ops3);
    fail_unless (strcmp (path, etalon_path) == 0, "path('%s') doesn't match to '%s'", path,
                 etalon_path);
    fail_unless (strcmp (local, etalon_local) == 0, "parsed local path('%s') doesn't match to '%s'",
                 local, etalon_local);
    fail_unless (strcmp (op, etalon_op) == 0, "parsed VFS name ('%s') doesn't match to '%s'", op,
                 etalon_op);

    etalon_path = "#test1:/bla-bla/some/path/";
    etalon_local = "bla-bla/some/path2/";
    etalon_op = "test2:";
    result = _vfs_split_with_semi_skip_count (path, &local, &op, 0);
    fail_unless (result == &vfs_test_ops2, "Result(%p) doesn't match to vfs_test_ops2(%p)", result,
                 &vfs_test_ops2);
    fail_unless (strcmp (path, etalon_path) == 0, "path('%s') doesn't match to '%s'", path,
                 etalon_path);
    fail_unless (strcmp (local, etalon_local) == 0, "parsed local path('%s') doesn't match to '%s'",
                 local, etalon_local);
    fail_unless (strcmp (op, etalon_op) == 0, "parsed VFS name ('%s') doesn't match to '%s'", op,
                 etalon_op);

    etalon_path = "";
    etalon_local = "bla-bla/some/path/";
    etalon_op = "test1:";
    result = _vfs_split_with_semi_skip_count (path, &local, &op, 0);
    fail_unless (result == &vfs_test_ops1, "Result(%p) doesn't match to vfs_test_ops1(%p)", result,
                 &vfs_test_ops2);
    fail_unless (strcmp (path, etalon_path) == 0, "path('%s') doesn't match to '%s'", path,
                 etalon_path);
    fail_unless (strcmp (local, etalon_local) == 0, "parsed local path('%s') doesn't match to '%s'",
                 local, etalon_local);
    fail_unless (strcmp (op, etalon_op) == 0, "parsed VFS name ('%s') doesn't match to '%s'", op,
                 etalon_op);

    result = _vfs_split_with_semi_skip_count (path, &local, &op, 0);
    fail_unless (result == NULL, "Result(%p) doesn't match to vfs_test_ops1(NULL)", result);
    fail_unless (strcmp (path, etalon_path) == 0, "path('%s') doesn't match to '%s'", path,
                 etalon_path);
    fail_unless (strcmp (local, etalon_local) == 0, "parsed local path('%s') doesn't match to '%s'",
                 local, etalon_local);
    fail_unless (strcmp (op, etalon_op) == 0, "parsed VFS name ('%s') doesn't match to '%s'", op,
                 etalon_op);

    g_free (path);
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */
START_TEST (test_vfs_split_with_local)
/* *INDENT-ON* */
{
    char *path;
    const char *local, *op, *etalon_path, *etalon_local, *etalon_op;
    struct vfs_class *result;

    path =
        g_strdup
        ("/local/path/#test1:/bla-bla/some/path/#test2:/bla-bla/some/path2#test3:/qqq/www/eee.rr");

    etalon_path = "/local/path/#test1:/bla-bla/some/path/#test2:/bla-bla/some/path2";
    etalon_local = "qqq/www/eee.rr";
    etalon_op = "test3:";
    result = _vfs_split_with_semi_skip_count (path, &local, &op, 0);
    fail_unless (result == &vfs_test_ops3, "Result(%p) doesn't match to vfs_test_ops3(%p)", result,
                 &vfs_test_ops3);
    fail_unless (strcmp (path, etalon_path) == 0, "path('%s') doesn't match to '%s'", path,
                 etalon_path);
    fail_unless (strcmp (local, etalon_local) == 0, "parsed local path('%s') doesn't match to '%s'",
                 local, etalon_local);
    fail_unless (strcmp (op, etalon_op) == 0, "parsed VFS name ('%s') doesn't match to '%s'", op,
                 etalon_op);

    etalon_path = "/local/path/#test1:/bla-bla/some/path/";
    etalon_local = "bla-bla/some/path2";
    etalon_op = "test2:";
    result = _vfs_split_with_semi_skip_count (path, &local, &op, 0);
    fail_unless (result == &vfs_test_ops2, "Result(%p) doesn't match to vfs_test_ops2(%p)", result,
                 &vfs_test_ops2);
    fail_unless (strcmp (path, etalon_path) == 0, "path('%s') doesn't match to '%s'", path,
                 etalon_path);
    fail_unless (strcmp (local, etalon_local) == 0, "parsed local path('%s') doesn't match to '%s'",
                 local, etalon_local);
    fail_unless (strcmp (op, etalon_op) == 0, "parsed VFS name ('%s') doesn't match to '%s'", op,
                 etalon_op);

    etalon_path = "/local/path/";
    etalon_local = "bla-bla/some/path/";
    etalon_op = "test1:";
    result = _vfs_split_with_semi_skip_count (path, &local, &op, 0);
    fail_unless (result == &vfs_test_ops1, "Result(%p) doesn't match to vfs_test_ops1(%p)", result,
                 &vfs_test_ops2);
    fail_unless (strcmp (path, etalon_path) == 0, "path('%s') doesn't match to '%s'", path,
                 etalon_path);
    fail_unless (strcmp (local, etalon_local) == 0, "parsed local path('%s') doesn't match to '%s'",
                 local, etalon_local);
    fail_unless (strcmp (op, etalon_op) == 0, "parsed VFS name ('%s') doesn't match to '%s'", op,
                 etalon_op);

    result = _vfs_split_with_semi_skip_count (path, &local, &op, 0);
    fail_unless (result == NULL, "Result(%p) doesn't match to vfs_test_ops1(NULL)", result);

    g_free (path);
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */
/* *INDENT-OFF* */
START_TEST (test_vfs_split_url)
/* *INDENT-ON* */
{
    char *path;
    const char *local, *op, *etalon_path, *etalon_local, *etalon_op;
    struct vfs_class *result;

    path = g_strdup ("#test2:username:passwd@somehost.net/bla-bla/some/path2");

    etalon_path = "";
    etalon_local = "bla-bla/some/path2";
    etalon_op = "test2:username:passwd@somehost.net";
    result = _vfs_split_with_semi_skip_count (path, &local, &op, 0);
    fail_unless (result == &vfs_test_ops2, "Result(%p) doesn't match to vfs_test_ops2(%p)", result,
                 &vfs_test_ops2);
    fail_unless (path != NULL
                 && strcmp (path, etalon_path) == 0, "path('%s') doesn't match to '%s'", path,
                 etalon_path);
    fail_unless (local != NULL
                 && strcmp (local, etalon_local) == 0,
                 "parsed local path('%s') doesn't match to '%s'", local, etalon_local);
    fail_unless (op != NULL
                 && strcmp (op, etalon_op) == 0, "parsed VFS name ('%s') doesn't match to '%s'", op,
                 etalon_op);

    g_free (path);
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */
START_TEST (test_vfs_split_url_with_semi)
/* *INDENT-ON* */
{
    char *path;
    const char *local, *op, *etalon_path, *etalon_local, *etalon_op;
    struct vfs_class *result;


    path =
        g_strdup
        ("/local/path/#test1:/bla-bla/some/path/#test2:username:p!a@s#s$w%d@somehost.net/bla-bla/some/path2");

    etalon_path = "/local/path/#test1:/bla-bla/some/path/";
    etalon_local = "bla-bla/some/path2";
    etalon_op = "test2:username:p!a@s#s$w%d@somehost.net";
    result = _vfs_split_with_semi_skip_count (path, &local, &op, 0);
    fail_unless (result == &vfs_test_ops2, "Result(%p) doesn't match to vfs_test_ops2(%p)", result,
                 &vfs_test_ops2);
    fail_unless (path != NULL
                 && strcmp (path, etalon_path) == 0, "path('%s') doesn't match to '%s'", path,
                 etalon_path);
    fail_unless (local != NULL
                 && strcmp (local, etalon_local) == 0,
                 "parsed local path('%s') doesn't match to '%s'", local, etalon_local);
    fail_unless (op != NULL
                 && strcmp (op, etalon_op) == 0, "parsed VFS name ('%s') doesn't match to '%s'", op,
                 etalon_op);

    g_free (path);

}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */
START_TEST (test_vfs_split_with_semi_in_path)
/* *INDENT-ON* */
{
    char *path;
    const char *local, *op, *etalon_path, *etalon_local, *etalon_op;
    struct vfs_class *result;

    path = g_strdup ("#test2:/bl#a-bl#a/so#me/pa#th2");

    etalon_path = "";
    etalon_local = "bl#a-bl#a/so#me/pa#th2";
    etalon_op = "test2:";
    result = _vfs_split_with_semi_skip_count (path, &local, &op, 0);
    fail_unless (result == &vfs_test_ops2, "Result(%p) doesn't match to vfs_test_ops2(%p)", result,
                 &vfs_test_ops2);
    fail_unless (path != NULL
                 && strcmp (path, etalon_path) == 0, "path('%s') doesn't match to '%s'", path,
                 etalon_path);
    fail_unless (local != NULL
                 && strcmp (local, etalon_local) == 0,
                 "parsed local path('%s') doesn't match to '%s'", local, etalon_local);
    fail_unless (op != NULL
                 && strcmp (op, etalon_op) == 0, "parsed VFS name ('%s') doesn't match to '%s'", op,
                 etalon_op);

    g_free (path);
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
    tcase_add_test (tc_core, test_vfs_split);
    tcase_add_test (tc_core, test_vfs_split_with_local);
    tcase_add_test (tc_core, test_vfs_split_url);
    tcase_add_test (tc_core, test_vfs_split_url_with_semi);
    tcase_add_test (tc_core, test_vfs_split_with_semi_in_path);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "vfs_split.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
