/*
   lib/vfs - vfs_path_t charset recode functions

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

#include "lib/charsets.h"

#include "lib/strutil.h"
#include "lib/vfs/xdirentry.h"
#include "lib/vfs/path.h"

#include "src/vfs/local/local.c"

const char *mc_config_get_home_dir (void);

/* --------------------------------------------------------------------------------------------- */

/* @Mock */
const char *
mc_config_get_home_dir (void)
{
    return "/mock/home";
}

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
}

/* --------------------------------------------------------------------------------------------- */

static void
test_init_vfs (const char *encoding)
{
    str_init_strings (encoding);

    vfs_init ();
    init_localfs ();
    vfs_setup_work_dir ();

    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;

    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;
    load_codepages_list ();
}

/* --------------------------------------------------------------------------------------------- */

static void
test_deinit_vfs ()
{
    free_codepages_list ();
    str_uninit_strings ();
    vfs_shut ();
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_path_recode_ds") */
/* *INDENT-OFF* */
static const struct test_path_recode_ds
{
    const char *input_codepage;
    const char *input_path;
    const char *expected_element_path;
    const char *expected_recoded_path;
} test_path_recode_ds[] =
{
    { /* 0. */
        "UTF-8",
        "/ÔÅÓÔÏ×ÙÊ/ÐÕÔØ",
        "/ÔÅÓÔÏ×ÙÊ/ÐÕÔØ",
        "/ÔÅÓÔÏ×ÙÊ/ÐÕÔØ"
    },
    { /* 1. */
        "UTF-8",
        "/#enc:KOI8-R/Ñ‚ÐµÑÑ‚Ð¾Ð²Ñ‹Ð¹/Ð¿ÑƒÑ‚ÑŒ",
        "/ÔÅÓÔÏ×ÙÊ/ÐÕÔØ",
        "/#enc:KOI8-R/Ñ‚ÐµÑÑ‚Ð¾Ð²Ñ‹Ð¹/Ð¿ÑƒÑ‚ÑŒ"
    },
    { /* 2. */
        "KOI8-R",
        "/Ñ‚ÐµÑÑ‚Ð¾Ð²Ñ‹Ð¹/Ð¿ÑƒÑ‚ÑŒ",
        "/Ñ‚ÐµÑÑ‚Ð¾Ð²Ñ‹Ð¹/Ð¿ÑƒÑ‚ÑŒ",
        "/Ñ‚ÐµÑÑ‚Ð¾Ð²Ñ‹Ð¹/Ð¿ÑƒÑ‚ÑŒ"
    },
    { /* 3. */
        "KOI8-R",
        "/#enc:UTF-8/ÔÅÓÔÏ×ÙÊ/ÐÕÔØ",
        "/Ñ‚ÐµÑÑ‚Ð¾Ð²Ñ‹Ð¹/Ð¿ÑƒÑ‚ÑŒ",
        "/#enc:UTF-8/ÔÅÓÔÏ×ÙÊ/ÐÕÔØ"
    },
    { /* 4. Test encode info at start */
        "UTF-8",
        "#enc:KOI8-R/bla-bla/some/path",
        "/bla-bla/some/path",
        "/#enc:KOI8-R/bla-bla/some/path"
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_path_recode_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_path_recode, test_path_recode_ds)
/* *INDENT-ON* */
{
    /* given */
    vfs_path_t *vpath;
    char *result;
    const vfs_path_element_t *element;

    test_init_vfs (data->input_codepage);

    /* when */
    vpath = vfs_path_from_str (data->input_path);
    element = vfs_path_get_by_index (vpath, -1);
    result = vfs_path_to_str (vpath);

    /* then */
    mctest_assert_str_eq (element->path, data->expected_element_path);
    mctest_assert_str_eq (result, data->expected_recoded_path);

    g_free (result);
    vfs_path_free (vpath);
    test_deinit_vfs ();
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
/* *INDENT-ON* */


/* --------------------------------------------------------------------------------------------- */

static struct vfs_s_subclass test_subclass1;
static struct vfs_class vfs_test_ops1;


/* @DataSource("test_path_to_str_flags_ds") */
/* *INDENT-OFF* */
static const struct test_path_to_str_flags_ds
{
    const char *input_path;
    const vfs_path_flag_t input_from_str_flags;
    const vfs_path_flag_t input_to_str_flags;
    const char *expected_path;
} test_path_to_str_flags_ds[] =
{
    { /* 0. */
        "test1://user:passwd@127.0.0.1",
        VPF_NO_CANON,
        VPF_STRIP_PASSWORD,
        "test1://user@127.0.0.1/"
    },
    { /* 1. */
        "/test1://user:passwd@host.name/#enc:KOI8-R/Ñ‚ÐµÑÑ‚Ð¾Ð²Ñ‹Ð¹/Ð¿ÑƒÑ‚ÑŒ",
        VPF_NONE,
        VPF_STRIP_PASSWORD,
        "/test1://user@host.name/#enc:KOI8-R/Ñ‚ÐµÑÑ‚Ð¾Ð²Ñ‹Ð¹/Ð¿ÑƒÑ‚ÑŒ"
    },
    { /* 2. */
        "/test1://user:passwd@host.name/#enc:KOI8-R/Ñ‚ÐµÑÑ‚Ð¾Ð²Ñ‹Ð¹/Ð¿ÑƒÑ‚ÑŒ",
        VPF_NONE,
        VPF_RECODE,
        "/test1://user:passwd@host.name/ÔÅÓÔÏ×ÙÊ/ÐÕÔØ"
    },
    { /* 3. */
        "/test1://user:passwd@host.name/#enc:KOI8-R/Ñ‚ÐµÑÑ‚Ð¾Ð²Ñ‹Ð¹/Ð¿ÑƒÑ‚ÑŒ",
        VPF_NONE,
        VPF_RECODE | VPF_STRIP_PASSWORD,
        "/test1://user@host.name/ÔÅÓÔÏ×ÙÊ/ÐÕÔØ"
    },
    { /* 4. */
        "/mock/home/test/dir",
        VPF_NONE,
        VPF_STRIP_HOME,
        "~/test/dir"
    },
    { /* 5. */
        "/mock/home/test1://user:passwd@host.name/#enc:KOI8-R/Ñ‚ÐµÑÑ‚Ð¾Ð²Ñ‹Ð¹/Ð¿ÑƒÑ‚ÑŒ",
        VPF_NONE,
        VPF_STRIP_HOME | VPF_STRIP_PASSWORD,
        "~/test1://user@host.name/#enc:KOI8-R/Ñ‚ÐµÑÑ‚Ð¾Ð²Ñ‹Ð¹/Ð¿ÑƒÑ‚ÑŒ"
    },
    { /* 6. */
        "/mock/home/test1://user:passwd@host.name/#enc:KOI8-R/Ñ‚ÐµÑÑ‚Ð¾Ð²Ñ‹Ð¹/Ð¿ÑƒÑ‚ÑŒ",
        VPF_NONE,
        VPF_STRIP_HOME | VPF_STRIP_PASSWORD | VPF_HIDE_CHARSET,
        "~/test1://user@host.name/Ñ‚ÐµÑÑ‚Ð¾Ð²Ñ‹Ð¹/Ð¿ÑƒÑ‚ÑŒ"
    },
    { /* 7. */
        "/mock/home/test1://user:passwd@host.name/#enc:KOI8-R/Ñ‚ÐµÑÑ‚Ð¾Ð²Ñ‹Ð¹/Ð¿ÑƒÑ‚ÑŒ",
        VPF_NONE,
        VPF_STRIP_HOME | VPF_RECODE,
        "~/test1://user:passwd@host.name/ÔÅÓÔÏ×ÙÊ/ÐÕÔØ"
    },
    { /* 8. */
        "/mock/home/test1://user:passwd@host.name/#enc:KOI8-R/Ñ‚ÐµÑÑ‚Ð¾Ð²Ñ‹Ð¹/Ð¿ÑƒÑ‚ÑŒ",
        VPF_NONE,
        VPF_STRIP_HOME | VPF_RECODE | VPF_STRIP_PASSWORD,
        "~/test1://user@host.name/ÔÅÓÔÏ×ÙÊ/ÐÕÔØ"
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_path_to_str_flags_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_path_to_str_flags, test_path_to_str_flags_ds)
/* *INDENT-ON* */
{
    /* given */
    vfs_path_t *vpath;
    char *str_path;

    test_init_vfs ("UTF-8");

    test_subclass1.flags = VFS_S_REMOTE;
    vfs_s_init_class (&vfs_test_ops1, &test_subclass1);
    vfs_test_ops1.name = "testfs1";
    vfs_test_ops1.flags = VFSF_NOLINKS;
    vfs_test_ops1.prefix = "test1";
    vfs_register_class (&vfs_test_ops1);

    /* when */

    vpath = vfs_path_from_str_flags (data->input_path, data->input_from_str_flags);
    str_path = vfs_path_to_str_flags (vpath, 0, data->input_to_str_flags);

    /* then */
    mctest_assert_str_eq (str_path, data->expected_path);

    g_free (str_path);
    vfs_path_free (vpath);
    test_deinit_vfs ();
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
    mctest_add_parameterized_test (tc_core, test_path_recode, test_path_recode_ds);
    mctest_add_parameterized_test (tc_core, test_path_to_str_flags, test_path_to_str_flags_ds);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_print (sr, CK_VERBOSE);
    srunner_set_log (sr, "path_recode.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
