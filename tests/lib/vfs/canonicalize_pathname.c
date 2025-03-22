/*
   lib - canonicalize path

   Copyright (C) 2011-2025
   Free Software Foundation, Inc.

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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define TEST_SUITE_NAME "/lib/vfs"

#include "tests/mctest.h"

#include "lib/charsets.h"
#include "lib/strutil.h"
#include "lib/util.h"
#include "lib/vfs/xdirentry.h"

#include "src/vfs/local/local.c"

static struct vfs_class vfs_test_ops;

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    str_init_strings (NULL);

    vfs_init ();
    vfs_init_localfs ();
    vfs_setup_work_dir ();

    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;
    load_codepages_list ();

    vfs_init_class (&vfs_test_ops, "testfs", VFSF_NOLINKS | VFSF_REMOTE, "ftp");
    vfs_register_class (&vfs_test_ops);
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    free_codepages_list ();
    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_canonicalize_path_ds") */
static const struct test_canonicalize_path_ds
{
    const char *input_path;
    const char *expected_path;
} test_canonicalize_path_ds[] = {
    {
        // 0. UNC path
        "//some_server/ww",
        "//some_server/ww",
    },
    {
        // 1. join slashes
        "///some_server/////////ww",
        "/some_server/ww",
    },
    {
        // 2. Collapse "/./" -> "/"
        "//some_server//.///////ww/./././.",
        "//some_server/ww",
    },
    {
        // 3. Remove leading "./"
        "./some_server/ww",
        "some_server/ww",
    },
    {
        // 4. some/.. -> .
        "some_server/..",
        ".",
    },
    {
        // 5. Collapse "/.." with the previous part of path
        "/some_server/ww/some_server/../ww/../some_server/..//ww/some_server/ww",
        "/some_server/ww/ww/some_server/ww",
    },
    {
        // 6. URI style
        "/some_server/ww/ftp://user:pass@host.net/path/",
        "/some_server/ww/ftp://user:pass@host.net/path",
    },
    {
        // 7.
        "/some_server/ww/ftp://user:pass@host.net/path/../../",
        "/some_server/ww",
    },
    {
        // 8.
        "ftp://user:pass@host.net/path/../../",
        ".",
    },
    {
        // 9.
        "ftp://user/../../",
        "..",
    },
    {
        // 10. Supported encoding
        "/b/#enc:utf-8/../c",
        "/c",
    },
    {
        // 11. Unsupported encoding
        "/b/#enc:aaaa/../c",
        "/b/c",
    },
    {
        // 12. Supported encoding
        "/b/../#enc:utf-8/c",
        "/#enc:utf-8/c",
    },
    {
        // 13. Unsupported encoding
        "/b/../#enc:aaaa/c",
        "/#enc:aaaa/c",
    },
    {
        // 14.  Supported encoding
        "/b/c/#enc:utf-8/..",
        "/b",
    },
    {
        // 15.  Unsupported encoding
        "/b/c/#enc:aaaa/..",
        "/b/c",
    },
    {
        // 16.  Supported encoding
        "/b/c/../#enc:utf-8",
        "/b/#enc:utf-8",
    },
    {
        // 17.  Unsupported encoding
        "/b/c/../#enc:aaaa",
        "/b/#enc:aaaa",
    },
};

/* @Test(dataSource = "test_canonicalize_path_ds") */
START_PARAMETRIZED_TEST (test_canonicalize_path, test_canonicalize_path_ds)
{
    // given
    char *actual_path;

    actual_path = g_strdup (data->input_path);

    // when
    canonicalize_pathname (actual_path);

    // then
    mctest_assert_str_eq (actual_path, data->expected_path) g_free (actual_path);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    // Add new tests here: ***************
    mctest_add_parameterized_test (tc_core, test_canonicalize_path, test_canonicalize_path_ds);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
