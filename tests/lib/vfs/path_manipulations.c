/* lib/vfs - test vfs_path_t manipulation functions

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

#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#endif

#include "lib/strutil.h"
#include "lib/vfs/xdirentry.h"
#include "lib/vfs/path.h"

#include "src/vfs/local/local.c"


static struct vfs_class vfs_test_ops1, vfs_test_ops2, vfs_test_ops3;

/* --------------------------------------------------------------------------------------------- */

static void
init_test_classes (void)
{
    vfs_init_class (&vfs_test_ops1, "testfs1", VFSF_NOLINKS | VFSF_REMOTE, "test1");
    vfs_register_class (&vfs_test_ops1);

    vfs_init_class (&vfs_test_ops2, "testfs2", VFSF_UNKNOWN, "test2");
    vfs_register_class (&vfs_test_ops2);

    vfs_init_class (&vfs_test_ops3, "testfs3", VFSF_LOCAL, "test3");
    vfs_register_class (&vfs_test_ops3);
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

    init_test_classes ();

    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;
#ifdef HAVE_CHARSET
    load_codepages_list ();
#endif
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
#ifdef HAVE_CHARSET
    free_codepages_list ();
#endif

    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_vfs_path_tokens_count_ds") */
/* *INDENT-OFF* */
static const struct test_vfs_path_tokens_count_ds
{
    const char *input_path;
    const vfs_path_flag_t input_flags;
    const size_t expected_token_count;
} test_vfs_path_tokens_count_ds[] =
{
    { /* 0. */
        "/",
        VPF_NONE,
        0
    },
    { /* 1. */
        "/path",
        VPF_NONE,
        1
    },
    { /* 2. */
        "/path1/path2/path3",
        VPF_NONE,
        3
    },
    { /* 3. */
        "test3://path1/path2/path3/path4",
        VPF_NO_CANON,
        4
    },
    { /* 4. */
        "path1/path2/path3",
        VPF_NO_CANON,
        3
    },
    { /* 5. */
        "/path1/path2/path3/",
        VPF_NONE,
        3
    },
    { /* 6. */
        "/local/path/test1://user:pass@some.host:12345/bla-bla/some/path/",
        VPF_NONE,
        5
    },
#ifdef HAVE_CHARSET
    { /* 7. */
        "/local/path/test1://user:pass@some.host:12345/bla-bla/some/path/"
        "test2://#enc:KOI8-R/bla-bla/some/path/test3://111/22/33",
        VPF_NONE,
        11
    },
#endif
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_vfs_path_tokens_count_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_vfs_path_tokens_count, test_vfs_path_tokens_count_ds)
/* *INDENT-ON* */
{
    /* given */
    size_t tokens_count;
    vfs_path_t *vpath;

    vpath = vfs_path_from_str_flags (data->input_path, data->input_flags);

    /* when */
    tokens_count = vfs_path_tokens_count (vpath);

    /* then */
    ck_assert_int_eq (tokens_count, data->expected_token_count);

    vfs_path_free (vpath, TRUE);
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_vfs_path_tokens_get_ds") */
/* *INDENT-OFF* */
static const struct test_vfs_path_tokens_get_ds
{
    const char *input_path;
    const ssize_t input_start_position;
    const ssize_t input_length;

    const char *expected_path;
} test_vfs_path_tokens_get_ds[] =
{
    { /* 0. Invalid start position  */
        "/",
        2,
        1,
        NULL
    },
    { /* 1. Invalid negative position */
        "/path",
        -3,
        1,
        NULL
    },
    { /* 2. Count of tokens is zero. Count should be autocorrected */
        "/path",
        0,
        0,
        "path"
    },
    { /* 3. get 'path2/path3' by 1,2  */
        "/path1/path2/path3/path4",
        1,
        2,
        "path2/path3"
    },
    { /* 4. get 'path2/path3' by 1,2  from LOCAL VFS */
        "test3://path1/path2/path3/path4",
        1,
        2,
        "path2/path3"
    },
    { /* 5. get 'path2/path3' by 1,2  from non-LOCAL VFS */
        "test2://path1/path2/path3/path4",
        1,
        2,
        "test2://path2/path3"
    },
    { /* 6. get 'path2/path3' by 1,2  through non-LOCAL VFS */
        "/path1/path2/test1://user:pass@some.host:12345/path3/path4",
        1,
        2,
        "path2/test1://user:pass@some.host:12345/path3"
    },
    { /* 7. get 'path2/path3' by 1,2  where path2 it's LOCAL VFS */
        "test3://path1/path2/test2://path3/path4",
        1,
        2,
        "path2/test2://path3"
    },
    { /* 8. get 'path2/path3' by 1,2  where path3 it's LOCAL VFS */
        "test2://path1/path2/test3://path3/path4",
        1,
        2,
        "test2://path2/test3://path3"
    },
    { /* 9. get 'path4' by -1,1  */
        "/path1/path2/path3/path4",
        -1,
        1,
        "path4"
    },
    { /* 10. get 'path2/path3/path4' by -3,0  */
        "/path1/path2/path3/path4",
        -3,
        0,
        "path2/path3/path4"
    },
#ifdef HAVE_CHARSET
    { /* 11. get 'path2/path3' by 1,2  from LOCAL VFS with encoding */
        "test3://path1/path2/test3://#enc:KOI8-R/path3/path4",
        1,
        2,
        "path2/test3://#enc:KOI8-R/path3"
    },
    { /* 12. get 'path2/path3' by 1,2  with encoding */
        "#enc:KOI8-R/path1/path2/path3/path4",
        1,
        2,
        "#enc:KOI8-R/path2/path3"
    },
#endif
/* TODO: currently this test don't passed. Probably broken string URI parser
 { *//* 13. get 'path2/path3' by 1,2  from LOCAL VFS *//*

        "test3://path1/path2/test2://test3://path3/path4",
        1,
        2,
        "path2/path3"
    },
*/
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_vfs_path_tokens_get_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_vfs_path_tokens_get, test_vfs_path_tokens_get_ds)
/* *INDENT-ON* */
{
    /* given */
    vfs_path_t *vpath;
    char *actual_path;

    vpath = vfs_path_from_str_flags (data->input_path, VPF_NO_CANON);

    /* when */
    actual_path = vfs_path_tokens_get (vpath, data->input_start_position, data->input_length);

    /* then */
    mctest_assert_str_eq (actual_path, data->expected_path);

    g_free (actual_path);
    vfs_path_free (vpath, TRUE);
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_vfs_path_append_vpath_ds") */
/* *INDENT-OFF* */
static const struct test_vfs_path_append_vpath_ds
{
    const char *input_path1;
    const char *input_path2;
    const int expected_element_count;
    const char *expected_path;
} test_vfs_path_append_vpath_ds[] =
{
    { /* 0. */
        "/local/path/test1://user:pass@some.host:12345/bla-bla/some/path/test2://bla-bla/some/path/test3://111/22/33",
        "/local/path/test1://user:pass@some.host:12345/bla-bla/some/path/",
        6,
        "/local/path/test1://user:pass@some.host:12345/bla-bla/some/path/test2://bla-bla/some/path/test3://111/22/33"
        "/local/path/test1://user:pass@some.host:12345/bla-bla/some/path",
    },
#ifdef HAVE_CHARSET
    { /* 1. */
        "/local/path/test1://user:pass@some.host:12345/bla-bla/some/path/test2://#enc:KOI8-R/bla-bla/some/path/test3://111/22/33",
        "/local/path/test1://user:pass@some.host:12345/bla-bla/some/path/",
        6,
        "/local/path/test1://user:pass@some.host:12345/bla-bla/some/path/test2://#enc:KOI8-R/bla-bla/some/path/test3://111/22/33"
        "/local/path/test1://user:pass@some.host:12345/bla-bla/some/path",
    },
#endif /* HAVE_CHARSET */
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_vfs_path_append_vpath_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_vfs_path_append_vpath, test_vfs_path_append_vpath_ds)
/* *INDENT-ON* */
{
    /* given */
    vfs_path_t *vpath1, *vpath2, *vpath3;

    vpath1 = vfs_path_from_str (data->input_path1);
    vpath2 = vfs_path_from_str (data->input_path2);

    /* when */
    vpath3 = vfs_path_append_vpath_new (vpath1, vpath2, NULL);

    /* then */
    ck_assert_int_eq (vfs_path_elements_count (vpath3), data->expected_element_count);
    mctest_assert_str_eq (vfs_path_as_str (vpath3), data->expected_path);

    vfs_path_free (vpath1, TRUE);
    vfs_path_free (vpath2, TRUE);
    vfs_path_free (vpath3, TRUE);
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_vfs_path_relative_ds") */
/* *INDENT-OFF* */
static const struct test_vfs_path_relative_ds
{
    const char *input_path;
    const char *expected_path;
    const char *expected_last_path_in_element;
} test_vfs_path_relative_ds[] =
{
    { /* 0. */
        "../bla-bla",
        "../bla-bla",
        "../bla-bla"
    },
    { /* 1. */
        "../path/test1://user:pass@some.host:12345/bla-bla/some/path/",
        "../path/test1://user:pass@some.host:12345/bla-bla/some/path/",
        "bla-bla/some/path/"
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_vfs_path_relative_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_vfs_path_relative, test_vfs_path_relative_ds)
/* *INDENT-ON* */
{
    /* given */
    vfs_path_t *vpath;

    /* when */

    vpath = vfs_path_from_str_flags (data->input_path, VPF_NO_CANON);

    /* then */
    mctest_assert_true (vpath->relative);
    mctest_assert_str_eq (vfs_path_get_last_path_str (vpath), data->expected_last_path_in_element);
    mctest_assert_str_eq (vfs_path_as_str (vpath), data->expected_path);

    vfs_path_free (vpath, TRUE);
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* @Test(dataSource = "test_vfs_path_relative_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_vfs_path_relative_clone, test_vfs_path_relative_ds)
/* *INDENT-ON* */
{
    /* given */
    vfs_path_t *vpath, *cloned_vpath;

    vpath = vfs_path_from_str_flags (data->input_path, VPF_NO_CANON);

    /* when */

    cloned_vpath = vfs_path_clone (vpath);

    /* then */
    mctest_assert_true (cloned_vpath->relative);
    mctest_assert_str_eq (vfs_path_get_last_path_str (cloned_vpath),
                          data->expected_last_path_in_element);
    mctest_assert_str_eq (vfs_path_as_str (cloned_vpath), data->expected_path);

    vfs_path_free (vpath, TRUE);
    vfs_path_free (cloned_vpath, TRUE);
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    /* Add new tests here: *************** */
    mctest_add_parameterized_test (tc_core, test_vfs_path_tokens_count,
                                   test_vfs_path_tokens_count_ds);
    mctest_add_parameterized_test (tc_core, test_vfs_path_tokens_get, test_vfs_path_tokens_get_ds);
    mctest_add_parameterized_test (tc_core, test_vfs_path_append_vpath,
                                   test_vfs_path_append_vpath_ds);
    mctest_add_parameterized_test (tc_core, test_vfs_path_relative, test_vfs_path_relative_ds);
    mctest_add_parameterized_test (tc_core, test_vfs_path_relative_clone,
                                   test_vfs_path_relative_ds);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
