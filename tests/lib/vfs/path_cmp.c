/* lib/vfs - vfs_path_t compare functions

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

#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#endif

#include "lib/strutil.h"
#include "lib/vfs/xdirentry.h"
#include "lib/vfs/path.h"

#include "src/vfs/local/local.c"

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    str_init_strings (NULL);

    vfs_init ();
    init_localfs ();
    vfs_setup_work_dir ();

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

/* @DataSource("test_path_equal_ds") */
/* *INDENT-OFF* */
static const struct test_path_equal_ds
{
    const char *input_path1;
    const char *input_path2;
    const gboolean expected_result;
} test_path_equal_ds[] =
{
    { /* 0. */
        NULL,
        NULL,
        FALSE
    },
    { /* 1. */
        NULL,
        "/test/path",
        FALSE
    },
    { /* 2. */
        "/test/path",
        NULL,
        FALSE
    },
    { /* 3. */
        "/test/path",
        "/test/path",
        TRUE
    },
#ifdef HAVE_CHARSET
    { /* 4. */
        "/#enc:KOI8-R/тестовый/путь",
        "/тестовый/путь",
        FALSE
    },
    { /* 5. */
        "/тестовый/путь",
        "/#enc:KOI8-R/тестовый/путь",
        FALSE
    },
    { /* 6. */
        "/#enc:KOI8-R/тестовый/путь",
        "/#enc:KOI8-R/тестовый/путь",
        TRUE
    },
#endif
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_path_equal_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_path_equal, test_path_equal_ds)
/* *INDENT-ON* */
{
    /* given */
    vfs_path_t *vpath1, *vpath2;
    gboolean actual_result;

    vpath1 = vfs_path_from_str (data->input_path1);
    vpath2 = vfs_path_from_str (data->input_path2);

    /* when */
    actual_result = vfs_path_equal (vpath1, vpath2);

    /* then */
    mctest_assert_int_eq (actual_result, data->expected_result);

    vfs_path_free (vpath1);
    vfs_path_free (vpath2);
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_path_equal_len_ds") */
/* *INDENT-OFF* */
static const struct test_path_equal_len_ds
{
    const char *input_path1;
    const char *input_path2;
    const size_t input_length;
    const gboolean expected_result;
} test_path_equal_len_ds[] =
{
    { /* 0. */
        NULL,
        NULL,
        0,
        FALSE
    },
    { /* 1. */
        NULL,
        NULL,
        100,
        FALSE
    },
    { /* 2. */
        NULL,
        "/тестовый/путь",
        10,
        FALSE
    },
    { /* 3. */
        "/тестовый/путь",
        NULL,
        10,
        FALSE
    },
    { /* 4. */
        "/тестовый/путь",
        "/тестовый/путь",
        10,
        TRUE
    },
    { /* 5. */
        "/тест/овый/путь",
        "/тестовый/путь",
        8,
        TRUE
    },
    { /* 6. */
        "/тест/овый/путь",
        "/тестовый/путь",
        10,
        FALSE
    },
    { /* 7. */
        "/тестовый/путь",
        "/тест/овый/путь",
        10,
        FALSE
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_path_equal_len_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_path_equal_len, test_path_equal_len_ds)
/* *INDENT-ON* */
{
    /* given */
    vfs_path_t *vpath1, *vpath2;
    gboolean actual_result;

    vpath1 = vfs_path_from_str (data->input_path1);
    vpath2 = vfs_path_from_str (data->input_path2);

    /* when */
    actual_result = vfs_path_equal_len (vpath1, vpath2, data->input_length);

    /* then */
    mctest_assert_int_eq (actual_result, data->expected_result);

    vfs_path_free (vpath1);
    vfs_path_free (vpath2);
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
    mctest_add_parameterized_test (tc_core, test_path_equal, test_path_equal_ds);
    mctest_add_parameterized_test (tc_core, test_path_equal_len, test_path_equal_len_ds);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "path_cmp.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
