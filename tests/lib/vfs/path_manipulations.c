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

#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#endif

#include "lib/strutil.h"
#include "lib/vfs/xdirentry.h"
#include "lib/vfs/path.h"

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
    vfs_test_ops1.prefix = "test1";
    vfs_register_class (&vfs_test_ops1);

    vfs_s_init_class (&vfs_test_ops2, &test_subclass2);
    vfs_test_ops2.name = "testfs2";
    vfs_test_ops2.prefix = "test2";
    vfs_register_class (&vfs_test_ops2);

    vfs_s_init_class (&vfs_test_ops3, &test_subclass3);
    vfs_test_ops3.name = "testfs3";
    vfs_test_ops3.prefix = "test3";
    vfs_test_ops3.flags = VFSF_LOCAL;
    vfs_register_class (&vfs_test_ops3);

    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;
#ifdef HAVE_CHARSET
    load_codepages_list ();
#endif
}

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

/* *INDENT-OFF* */
START_TEST (test_vfs_path_tokens_count)
/* *INDENT-ON* */
{
    size_t tokens_count;
    vfs_path_t *vpath;

    vpath = vfs_path_from_str ("/");
    tokens_count = vfs_path_tokens_count (vpath);
    fail_unless (tokens_count == 0, "actual: %zu; expected: 0\n", tokens_count);
    vfs_path_free (vpath);

    vpath = vfs_path_from_str ("/path");
    tokens_count = vfs_path_tokens_count (vpath);
    fail_unless (tokens_count == 1, "actual: %zu; expected: 1\n", tokens_count);
    vfs_path_free (vpath);

    vpath = vfs_path_from_str ("/path1/path2/path3");
    tokens_count = vfs_path_tokens_count (vpath);
    fail_unless (tokens_count == 3, "actual: %zu; expected: 3\n", tokens_count);
    vfs_path_free (vpath);

    vpath = vfs_path_from_str_flags ("test3://path1/path2/path3/path4", VPF_NO_CANON);
    tokens_count = vfs_path_tokens_count (vpath);
    fail_unless (tokens_count == 4, "actual: %zu; expected: 4\n", tokens_count);
    vfs_path_free (vpath);

    vpath = vfs_path_from_str_flags ("path1/path2/path3", VPF_NO_CANON);
    tokens_count = vfs_path_tokens_count (vpath);
    fail_unless (tokens_count == 3, "actual: %zu; expected: 3\n", tokens_count);
    vfs_path_free (vpath);

    vpath = vfs_path_from_str ("/path1/path2/path3/");
    tokens_count = vfs_path_tokens_count (vpath);
    fail_unless (tokens_count == 3, "actual: %zu; expected: 3\n", tokens_count);
    vfs_path_free (vpath);

    vpath = vfs_path_from_str ("/local/path/test1://user:pass@some.host:12345/bla-bla/some/path/");
    tokens_count = vfs_path_tokens_count (vpath);
    fail_unless (tokens_count == 5, "actual: %zu; expected: 5\n", tokens_count);
    vfs_path_free (vpath);

#ifdef HAVE_CHARSET
    vpath =
        vfs_path_from_str
        ("/local/path/test1://user:pass@some.host:12345/bla-bla/some/path/test2://#enc:KOI8-R/bla-bla/some/path/test3://111/22/33");
    tokens_count = vfs_path_tokens_count (vpath);
    fail_unless (tokens_count == 11, "actual: %zu; expected: 11\n", tokens_count);
    vfs_path_free (vpath);
#endif
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

#define check_invalid_token_str(input, start, length) { \
    vpath = vfs_path_from_str (input); \
    path_tokens = vfs_path_tokens_get(vpath, start, length); \
    fail_unless (path_tokens == NULL, "path_tokens should be NULL!\n"); \
    g_free (path_tokens); \
    vfs_path_free (vpath); \
}

#define check_token_str(input, start, length, etalon) { \
    vpath = vfs_path_from_str_flags (input, VPF_NO_CANON); \
    path_tokens = vfs_path_tokens_get(vpath, start, length); \
    fail_unless (path_tokens != NULL, "path_tokens shouldn't equal to  NULL!\n"); \
    if (path_tokens != NULL) \
        fail_unless (strcmp(path_tokens, etalon) == 0, "\nactual: '%s'\netalon: '%s'", path_tokens, etalon); \
    g_free (path_tokens); \
    vfs_path_free (vpath); \
}

/* *INDENT-OFF* */
START_TEST (test_vfs_path_tokens_get)
/* *INDENT-ON* */
{
    vfs_path_t *vpath;
    char *path_tokens;

    /* Invalid start position */
    check_invalid_token_str ("/", 2, 1);

    /* Invalid negative position */
    check_invalid_token_str ("/path", -3, 1);

    /* Count of tokens is zero. Count should be autocorrected */
    check_token_str ("/path", 0, 0, "path");

    /* get 'path2/path3' by 1,2  */
    check_token_str ("/path1/path2/path3/path4", 1, 2, "path2/path3");

    /* get 'path2/path3' by 1,2  from LOCAL VFS */
    check_token_str ("test3://path1/path2/path3/path4", 1, 2, "path2/path3");

#ifdef HAVE_CHARSET
    /* get 'path2/path3' by 1,2  from LOCAL VFS with encoding */
    check_token_str ("test3://path1/path2/test3://#enc:KOI8-R/path3/path4", 1, 2,
                     "path2/test3://#enc:KOI8-R/path3");

    /* get 'path2/path3' by 1,2  with encoding */
    check_token_str ("#enc:KOI8-R/path1/path2/path3/path4", 1, 2, "#enc:KOI8-R/path2/path3");
#endif

    /* get 'path2/path3' by 1,2  from non-LOCAL VFS */
    check_token_str ("test2://path1/path2/path3/path4", 1, 2, "test2://path2/path3");

    /* get 'path2/path3' by 1,2  throught non-LOCAL VFS */
    check_token_str ("/path1/path2/test1://user:pass@some.host:12345/path3/path4", 1, 2,
                     "path2/test1://user:pass@some.host:12345/path3");

    /* get 'path2/path3' by 1,2  from LOCAL VFS */
    /* TODO: currently this test don't passed. Probably broken string URI parser */
    /*    check_token_str ("test3://path1/path2/test2://test3://path3/path4", 1, 2, "path2/path3"); */

    /* get 'path2/path3' by 1,2  where path2 it's LOCAL VFS */
    check_token_str ("test3://path1/path2/test2://path3/path4", 1, 2, "path2/test2://path3");

    /* get 'path2/path3' by 1,2  where path3 it's LOCAL VFS */
    check_token_str ("test2://path1/path2/test3://path3/path4", 1, 2,
                     "test2://path2/test3://path3");

    /* get 'path4' by -1,1  */
    check_token_str ("/path1/path2/path3/path4", -1, 1, "path4");

    /* get 'path2/path3/path4' by -3,0  */
    check_token_str ("/path1/path2/path3/path4", -3, 0, "path2/path3/path4");

}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */
START_TEST (test_vfs_path_append_vpath)
/* *INDENT-ON* */
{
    vfs_path_t *vpath1, *vpath2, *vpath3;

#ifdef HAVE_CHARSET
    vpath1 =
        vfs_path_from_str
        ("/local/path/test1://user:pass@some.host:12345/bla-bla/some/path/test2://#enc:KOI8-R/bla-bla/some/path/test3://111/22/33");
#else
    vpath1 =
        vfs_path_from_str
        ("/local/path/test1://user:pass@some.host:12345/bla-bla/some/path/test2://bla-bla/some/path/test3://111/22/33");
#endif
    vpath2 = vfs_path_from_str ("/local/path/test1://user:pass@some.host:12345/bla-bla/some/path/");

    vpath3 = vfs_path_append_vpath_new (vpath1, vpath2, NULL);

    fail_unless (vfs_path_elements_count (vpath3) == 6,
                 "\nvpath elements count should be %d, actial is %d\n",
                 6, vfs_path_elements_count (vpath3));
    vfs_path_free (vpath1);
    vfs_path_free (vpath2);
    vfs_path_free (vpath3);

}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */
START_TEST (test_vfs_path_relative)
/* *INDENT-ON* */
{
    vfs_path_t *vpath, *copy_vpath;
    char *path_str;

    vpath = vfs_path_from_str_flags ("../bla-bla", VPF_NO_CANON);
    fail_unless (vpath->relative, "relative flag fail!\n");

    path_str = vfs_path_to_str (vpath);

    fail_unless (strcmp (path_str, "../bla-bla") == 0, "relative fail!\nactual: [%s]\n", path_str);
    g_free (path_str);

    path_str = (char *) vfs_path_get_last_path_str (vpath);
    fail_unless (strcmp (path_str, "../bla-bla") == 0,
                 "relative fail!\nactual: element->path=[%s]\n", path_str);

    copy_vpath = vfs_path_clone (vpath);

    path_str = vfs_path_to_str (copy_vpath);

    fail_unless (strcmp (path_str, "../bla-bla") == 0, "relative fail!\nactual: [%s]\n", path_str);
    g_free (path_str);

    vfs_path_free (copy_vpath);
    vfs_path_free (vpath);

    vpath =
        vfs_path_from_str_flags ("../path/test1://user:pass@some.host:12345/bla-bla/some/path/",
                                 VPF_NO_CANON);
    path_str = vfs_path_to_str (vpath);
    fail_unless (strcmp (path_str, "../path/test1://user:pass@some.host:12345/bla-bla/some/path/")
                 == 0, "relative fail!\nactual: [%s]\n", path_str);
    g_free (path_str);
    vfs_path_free (vpath);

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
    tcase_add_test (tc_core, test_vfs_path_tokens_count);
    tcase_add_test (tc_core, test_vfs_path_tokens_get);
    tcase_add_test (tc_core, test_vfs_path_append_vpath);
    tcase_add_test (tc_core, test_vfs_path_relative);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "path_manipulations.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
