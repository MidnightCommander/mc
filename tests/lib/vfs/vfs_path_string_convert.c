/*
   lib/vfs - get vfs_path_t from string

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
#include "lib/vfs/path.c"       /* for testing static methods  */

#include "src/vfs/local/local.c"

static struct vfs_class vfs_test_ops1, vfs_test_ops2, vfs_test_ops3;

#define ETALON_PATH_STR "/#test1/bla-bla/some/path/#test2/bla-bla/some/path#test3/111/22/33"
#define ETALON_PATH_URL_STR "/test1://bla-bla/some/path/test2://bla-bla/some/path/test3://111/22/33"

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    str_init_strings (NULL);

    vfs_init ();
    vfs_init_localfs ();
    vfs_setup_work_dir ();

    vfs_init_class (&vfs_test_ops1, "testfs1", VFSF_NOLINKS, "test1");
    vfs_register_class (&vfs_test_ops1);

    vfs_init_class (&vfs_test_ops2, "testfs2", VFSF_REMOTE, "test2");
    vfs_register_class (&vfs_test_ops2);

    vfs_init_class (&vfs_test_ops3, "testfs3", VFSF_UNKNOWN, "test3");
    vfs_register_class (&vfs_test_ops3);

#ifdef HAVE_CHARSET
    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;
    load_codepages_list ();
#endif /* HAVE_CHARSET */
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
#ifdef HAVE_CHARSET
    free_codepages_list ();
#endif /* HAVE_CHARSET */

    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */
/* @DataSource("test_from_to_string_ds") */
/* *INDENT-OFF* */
static const struct test_from_to_string_ds
{
    const char *input_string;
    const char *expected_result;
    const char *expected_element_path;
    const size_t expected_elements_count;
    struct vfs_class *expected_vfs_class;
} test_from_to_string_ds[] =
{
    { /* 0. */
        ETALON_PATH_STR,
        ETALON_PATH_URL_STR,
        "111/22/33",
        4,
        &vfs_test_ops3
    },
    { /* 1. */
        "/",
        "/",
        "/",
        1,
        VFS_CLASS (&local_subclass)
    },
    { /* 2. */
        "/test1://bla-bla/some/path/test2://user:passwd@some.host:1234/bla-bla/some/path/test3://111/22/33",
        "/test1://bla-bla/some/path/test2://user:passwd@some.host:1234/bla-bla/some/path/test3://111/22/33",
        "111/22/33",
        4,
        &vfs_test_ops3
    },
#ifdef HAVE_CHARSET
    { /* 3. */
        "/#test1/bla-bla1/some/path/#test2/bla-bla2/#enc:KOI8-R/some/path#test3/111/22/33",
        "/test1://bla-bla1/some/path/test2://#enc:KOI8-R/bla-bla2/some/path/test3://111/22/33",
        "111/22/33",
        4,
        &vfs_test_ops3
    },
    { /* 4. */
        "/#test1/bla-bla1/#enc:IBM866/some/path/#test2/bla-bla2/#enc:KOI8-R/some/path#test3/111/22/33",
        "/test1://#enc:IBM866/bla-bla1/some/path/test2://#enc:KOI8-R/bla-bla2/some/path/test3://111/22/33",
        "111/22/33",
        4,
        &vfs_test_ops3
    },
    {  /* 5. */
        "/#test1/bla-bla1/some/path/#test2/bla-bla2/#enc:IBM866/#enc:KOI8-R/some/path#test3/111/22/33",
        "/test1://bla-bla1/some/path/test2://#enc:KOI8-R/bla-bla2/some/path/test3://111/22/33",
        "111/22/33",
        4,
        &vfs_test_ops3
    },
    { /* 6. */
        "/#test1/bla-bla1/some/path/#test2/bla-bla2/#enc:IBM866/some/#enc:KOI8-R/path#test3/111/22/33",
        "/test1://bla-bla1/some/path/test2://#enc:KOI8-R/bla-bla2/some/path/test3://111/22/33",
        "111/22/33",
        4,
        &vfs_test_ops3
    },
    { /* 7. */
        "/#test1/bla-bla1/some/path/#test2/#enc:IBM866/bla-bla2/#enc:KOI8-R/some/path#test3/111/22/33",
        "/test1://bla-bla1/some/path/test2://#enc:KOI8-R/bla-bla2/some/path/test3://111/22/33",
        "111/22/33",
        4,
        &vfs_test_ops3
    },
    { /* 8. */
        "/#test1/bla-bla1/some/path/#enc:IBM866/#test2/bla-bla2/#enc:KOI8-R/some/path#test3/111/22/33",
        "/test1://#enc:IBM866/bla-bla1/some/path/test2://#enc:KOI8-R/bla-bla2/some/path/test3://111/22/33",
        "111/22/33",
        4,
        &vfs_test_ops3
    },
#endif /* HAVE_CHARSET */
};
/* *INDENT-ON* */

/* @Test */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_from_to_string, test_from_to_string_ds)
/* *INDENT-ON* */
{
    /* given */
    vfs_path_t *vpath;
    size_t vpath_len;
    const vfs_path_element_t *path_element;
    const char *actual_result;

    vpath = vfs_path_from_str_flags (data->input_string, VPF_USE_DEPRECATED_PARSER);

    /* when */
    vpath_len = vfs_path_elements_count (vpath);
    actual_result = vfs_path_as_str (vpath);
    path_element = vfs_path_get_by_index (vpath, -1);

    /* then */
    ck_assert_int_eq (vpath_len, data->expected_elements_count);
    mctest_assert_str_eq (actual_result, data->expected_result);
    mctest_assert_ptr_eq (path_element->class, data->expected_vfs_class);
    mctest_assert_str_eq (path_element->path, data->expected_element_path);

    vfs_path_free (vpath, TRUE);
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_partial_string_by_index_ds") */
/* *INDENT-OFF* */
static const struct test_partial_string_by_index_ds
{
    const char *input_string;
    const off_t element_index;
    const char *expected_result;
} test_partial_string_by_index_ds[] =
{
    { /* 0. */
        ETALON_PATH_STR,
        -1,
        "/test1://bla-bla/some/path/test2://bla-bla/some/path"
    },
    { /* 1. */
        ETALON_PATH_STR,
        -2,
        "/test1://bla-bla/some/path/"
    },
    { /* 2. */
        ETALON_PATH_STR,
        -3,
        "/"
    },
    { /* 3. Index out of bound */
        ETALON_PATH_STR,
        -4,
        ""
    },
    { /* 4. */
        ETALON_PATH_STR,
        1,
        "/"
    },
    { /* 5. */
        ETALON_PATH_STR,
        2,
        "/test1://bla-bla/some/path/"
    },
    { /* 6. */
        ETALON_PATH_STR,
        3,
        "/test1://bla-bla/some/path/test2://bla-bla/some/path"
    },
    { /* 6. */
        ETALON_PATH_STR,
        4,
        ETALON_PATH_URL_STR
    },
    { /* 7. Index out of bound */
        ETALON_PATH_STR,
        5,
        ETALON_PATH_URL_STR
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_partial_string_by_index_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_partial_string_by_index, test_partial_string_by_index_ds)
/* *INDENT-ON* */
{
    /* given */
    vfs_path_t *vpath;
    char *actual_result;
    vpath = vfs_path_from_str_flags (data->input_string, VPF_USE_DEPRECATED_PARSER);

    /* when */
    actual_result = vfs_path_to_str_elements_count (vpath, data->element_index);

    /* then */
    mctest_assert_str_eq (actual_result, data->expected_result);
    g_free (actual_result);

    vfs_path_free (vpath, TRUE);
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */
#ifdef HAVE_CHARSET
/* --------------------------------------------------------------------------------------------- */

#define ETALON_STR "/path/to/file.ext/test1://#enc:KOI8-R"
/* @Test */
/* *INDENT-OFF* */
START_TEST (test_vfs_path_encoding_at_end)
/* *INDENT-ON* */
{
    /* given */
    vfs_path_t *vpath;
    const char *result;
    const vfs_path_element_t *element;

    vpath =
        vfs_path_from_str_flags ("/path/to/file.ext#test1:/#enc:KOI8-R", VPF_USE_DEPRECATED_PARSER);

    /* when */
    result = vfs_path_as_str (vpath);
    element = vfs_path_get_by_index (vpath, -1);

    /* then */
    mctest_assert_str_eq (element->path, "");
    mctest_assert_not_null (element->encoding);
    mctest_assert_str_eq (result, ETALON_STR);

    vfs_path_free (vpath, TRUE);
}

/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */
#endif /* HAVE_CHARSET */
/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    /* Add new tests here: *************** */
    mctest_add_parameterized_test (tc_core, test_from_to_string, test_from_to_string_ds);
    mctest_add_parameterized_test (tc_core, test_partial_string_by_index,
                                   test_partial_string_by_index_ds);
#ifdef HAVE_CHARSET
    tcase_add_test (tc_core, test_vfs_path_encoding_at_end);
#endif
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
