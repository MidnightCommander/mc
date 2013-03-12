/* lib/vfs - test vfs_path_t manipulation functions

   Copyright (C) 2011, 2013
   The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2011, 2013

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

#include "tests/mctest.h"

#include "lib/strutil.h"
#include "lib/vfs/xdirentry.h"
#include "lib/vfs/path.h"

#include "src/vfs/local/local.c"


struct vfs_s_subclass test_subclass1;
struct vfs_class vfs_test_ops1;

static int test_chdir (const vfs_path_t * vpath);

/* --------------------------------------------------------------------------------------------- */

/* @CapturedValue */
static vfs_path_t *test_chdir__vpath__captured;
/* @ThenReturnValue */
static int test_chdir__return_value;

/* @Mock */
static int
test_chdir (const vfs_path_t * vpath)
{
    test_chdir__vpath__captured = vfs_path_clone (vpath);
    return test_chdir__return_value;
}

static void
test_chdir__init (void)
{
    test_chdir__vpath__captured = NULL;
}

static void
test_chdir__deinit (void)
{
    vfs_path_free (test_chdir__vpath__captured);
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
    vfs_test_ops1.prefix = "test1";
    vfs_test_ops1.chdir = test_chdir;
    vfs_register_class (&vfs_test_ops1);

    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;

    vfs_local_ops.chdir = test_chdir;

    test_chdir__init ();
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    test_chdir__deinit ();

    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */
/* @DataSource("test_relative_cd_ds") */
/* *INDENT-OFF* */
static const struct test_relative_cd_ds
{
    const char *input_string;
    const vfs_path_flag_t input_flags;
    const char *expected_element_path;
} test_relative_cd_ds[] =
{
    { /* 0. */
        "/test1://user:pass@some.host:12345/path/to/dir",
        VPF_NONE,
        "path/to/dir"
    },
    { /* 1. */
        "some-non-exists-dir",
        VPF_NO_CANON,
        "some-non-exists-dir"
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_relative_cd_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_relative_cd, test_relative_cd_ds)
/* *INDENT-ON* */
{
    /* given */
    vfs_path_t *vpath;
    int actual_result;

    test_chdir__return_value = 0;

    vpath = vfs_path_from_str_flags (data->input_string, data->input_flags);

    /* when */
    actual_result = mc_chdir (vpath);

    /* then */
    {
        const vfs_path_element_t *element;

        mctest_assert_int_eq (actual_result, 0);
        element = vfs_path_get_by_index (vpath, -1);
        mctest_assert_str_eq (element->path, data->expected_element_path);
        vfs_path_free (vpath);
    }
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* Relative to panel_correct_path_to_show()  */

/* @Test */
/* *INDENT-OFF* */
START_TEST (test_vpath_to_str_filter)
/* *INDENT-ON* */
{
    /* given */
    vfs_path_t *vpath, *last_vpath;
    char *filtered_path;
    const vfs_path_element_t *path_element;

    /* when */
    vpath = vfs_path_from_str ("/test1://some.host/dir");
    path_element = vfs_path_element_clone (vfs_path_get_by_index (vpath, -1));
    vfs_path_free (vpath);

    last_vpath = vfs_path_new ();
    last_vpath->relative = TRUE;

    vfs_path_add_element (last_vpath, path_element);

    filtered_path = vfs_path_to_str_flags (last_vpath, 0,
                                           VPF_STRIP_HOME | VPF_STRIP_PASSWORD | VPF_HIDE_CHARSET);

    /* then */
    mctest_assert_str_eq (filtered_path, "test1://some.host/dir");

    vfs_path_free (last_vpath);
    g_free (filtered_path);
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
    mctest_add_parameterized_test (tc_core, test_relative_cd, test_relative_cd_ds);
    tcase_add_test (tc_core, test_vpath_to_str_filter);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "relative_cd.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
