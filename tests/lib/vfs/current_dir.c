/*
   lib/vfs - manipulate with current directory

   Copyright (C) 2011-2024
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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define TEST_SUITE_NAME "/lib/vfs"

#include "tests/mctest.h"

#include <string.h>             /* memset() */

#include "lib/global.h"
#include "lib/strutil.h"
#include "lib/vfs/xdirentry.h"

#include "src/vfs/local/local.c"

static struct vfs_s_subclass vfs_test_subclass;
static struct vfs_class *vfs_test_ops = VFS_CLASS (&vfs_test_subclass);

/* --------------------------------------------------------------------------------------------- */

/* @Mock */
static int
test_chdir (const vfs_path_t * vpath)
{
    (void) vpath;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    str_init_strings (NULL);

    vfs_init ();
    vfs_init_localfs ();
    vfs_setup_work_dir ();

    memset (&vfs_test_subclass, 0, sizeof (vfs_test_subclass));
    vfs_init_class (vfs_test_ops, "testfs", VFSF_UNKNOWN, "test");
    vfs_test_ops->chdir = test_chdir;
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

/* @DataSource("test_cd_ds") */
/* *INDENT-OFF* */
static const struct test_cd_ds
{
    const char *input_initial_path;
    const char *input_cd_path;
    const vfs_flags_t input_class_flags;

    const char *expected_cd_path;
} test_cd_ds[] =
{
    { /* 0. */
        "/",
        "/dev/some.file/test://",
        VFSF_NOLINKS,
        "/dev/some.file/test://"
    },
    { /* 1. */
        "/",
        "/dev/some.file/test://bla-bla",
        VFSF_NOLINKS,
        "/dev/some.file/test://bla-bla"
    },
    { /* 2. */
        "/dev/some.file/test://bla-bla",
        "..",
        VFSF_NOLINKS,
        "/dev/some.file/test://"
    },
    { /* 3. */
        "/dev/some.file/test://",
        "..",
        VFSF_NOLINKS,
        "/dev"
    },
    { /* 4. */
        "/dev",
        "..",
        VFSF_NOLINKS,
        "/"
    },
    { /* 5. */
        "/",
        "..",
        VFSF_NOLINKS,
        "/"
    },
    { /* 6. */
        "/",
        "/test://user:pass@host.net/path",
        VFSF_NOLINKS | VFSF_REMOTE,
        "/test://user:pass@host.net/path"
    },
    { /* 7. */
        "/test://user:pass@host.net/path",
        "..",
        VFSF_NOLINKS | VFSF_REMOTE,
        "/test://user:pass@host.net/"
    },
    { /* 8. */
        "/test://user:pass@host.net/",
        "..",
        VFSF_NOLINKS | VFSF_REMOTE,
        "/"
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_cd_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_cd, test_cd_ds)
/* *INDENT-ON* */
{
    /* given */
    vfs_path_t *vpath;

    vfs_test_ops->flags = data->input_class_flags;
    vfs_register_class (vfs_test_ops);

    vfs_set_raw_current_dir (vfs_path_from_str (data->input_initial_path));

    vpath = vfs_path_from_str (data->input_cd_path);

    /* when */
    mc_chdir (vpath);

    /* then */
    {
        char *actual_cd_path;

        actual_cd_path = vfs_get_cwd ();
        mctest_assert_str_eq (actual_cd_path, data->expected_cd_path);
        g_free (actual_cd_path);
    }
    vfs_path_free (vpath, TRUE);

    vfs_unregister_class (vfs_test_ops);
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;
    char *cwd;

    tc_core = tcase_create ("Core");

    /* writable directory where check creates temporary files */
    cwd = g_get_current_dir ();
    g_setenv ("TEMP", cwd, TRUE);
    g_free (cwd);

    tcase_add_checked_fixture (tc_core, setup, teardown);

    /* Add new tests here: *************** */
    mctest_add_parameterized_test (tc_core, test_cd, test_cd_ds);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
