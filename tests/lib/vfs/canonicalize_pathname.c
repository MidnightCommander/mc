/*
   lib - canonicalize path

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
#include "lib/util.h"
#include "lib/vfs/xdirentry.h"

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

#define check_canonicalize( input, etalon ) { \
    path = g_strdup(input); \
    canonicalize_pathname (path); \
    fail_unless ( \
        strcmp(path, etalon) == 0, \
        "\nactual value (%s)\nnot equal to etalon (%s)", path, etalon \
    ); \
    g_free(path); \
}

/* *INDENT-OFF* */
START_TEST (test_canonicalize_path)
/* *INDENT-ON* */
{
    char *path;
    static struct vfs_s_subclass test_subclass;
    static struct vfs_class vfs_test_ops;

    vfs_s_init_class (&vfs_test_ops, &test_subclass);

    vfs_test_ops.name = "testfs";
    vfs_test_ops.flags = VFSF_NOLINKS;
    vfs_test_ops.prefix = "ftp";
    test_subclass.flags = VFS_S_REMOTE;
    vfs_register_class (&vfs_test_ops);

    /* UNC path */
    check_canonicalize ("//some_server/ww", "//some_server/ww");

    /* join slashes */
    check_canonicalize ("///some_server/////////ww", "/some_server/ww");

    /* Collapse "/./" -> "/" */
    check_canonicalize ("//some_server//.///////ww/./././.", "//some_server/ww");

    /* Remove leading "./" */
    check_canonicalize ("./some_server/ww", "some_server/ww");

    /* some/.. -> . */
    check_canonicalize ("some_server/..", ".");

    /* Collapse "/.." with the previous part of path */
    check_canonicalize ("/some_server/ww/some_server/../ww/../some_server/..//ww/some_server/ww",
                        "/some_server/ww/ww/some_server/ww");

    /* URI style */
    check_canonicalize ("/some_server/ww/ftp://user:pass@host.net/path/",
                        "/some_server/ww/ftp://user:pass@host.net/path");

    check_canonicalize ("/some_server/ww/ftp://user:pass@host.net/path/../../", "/some_server/ww");

    check_canonicalize ("ftp://user:pass@host.net/path/../../", ".");

    check_canonicalize ("ftp://user/../../", "..");
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
    tcase_add_test (tc_core, test_canonicalize_path);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "canonicalize_pathname.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
