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

static struct vfs_s_subclass test_subclass;
static struct vfs_class vfs_test_ops;

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    str_init_strings (NULL);

    vfs_init ();
    init_localfs ();
    vfs_setup_work_dir ();

    vfs_s_init_class (&vfs_test_ops, &test_subclass);

    vfs_test_ops.name = "testfs";
    vfs_test_ops.flags = VFSF_NOLINKS;
    vfs_test_ops.prefix = "ftp";
    test_subclass.flags = VFS_S_REMOTE;
    vfs_register_class (&vfs_test_ops);
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

/* @DataSource("test_canonicalize_path_ds") */
/* *INDENT-OFF* */
static const struct test_canonicalize_path_ds
{
    const char *input_path;
    const char *expected_path;
} test_canonicalize_path_ds[] =
{
    { /* 0. UNC path */
        "//some_server/ww",
        "//some_server/ww"
    },
    { /* 1. join slashes */
        "///some_server/////////ww",
        "/some_server/ww"
    },
    { /* 2. Collapse "/./" -> "/" */
        "//some_server//.///////ww/./././.",
        "//some_server/ww"
    },
    {/* 3. Remove leading "./" */
        "./some_server/ww",
        "some_server/ww"
    },
    { /* 4. some/.. -> . */
        "some_server/..",
        "."
    },
    { /* 5. Collapse "/.." with the previous part of path */
        "/some_server/ww/some_server/../ww/../some_server/..//ww/some_server/ww",
        "/some_server/ww/ww/some_server/ww"
    },
    { /* 6. URI style */
        "/some_server/ww/ftp://user:pass@host.net/path/",
        "/some_server/ww/ftp://user:pass@host.net/path"
    },
    { /* 7. */
        "/some_server/ww/ftp://user:pass@host.net/path/../../",
        "/some_server/ww"
    },
    { /* 8. */
        "ftp://user:pass@host.net/path/../../",
        "."
    },
    { /* 9. */
        "ftp://user/../../",
        ".."
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_canonicalize_path_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_canonicalize_path, test_canonicalize_path_ds)
/* *INDENT-ON* */
{
    /* given */
    char *actual_path;

    actual_path = g_strdup (data->input_path);

    /* when */
    canonicalize_pathname (actual_path);

    /* then */
    mctest_assert_str_eq (actual_path, data->expected_path) g_free (actual_path);
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
    mctest_add_parameterized_test (tc_core, test_canonicalize_path, test_canonicalize_path_ds);
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
