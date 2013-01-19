/*
   lib/vfs - get vfs_path_t from string

   Copyright (C) 2011
   The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2011

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

#include <config.h>

#include <check.h>

#include "lib/global.c"

#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#endif

#include "lib/strutil.h"
#include "lib/vfs/xdirentry.h"
#include "lib/vfs/path.c"       /* for testing static methods  */

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


    vfs_s_init_class (&vfs_test_ops1, &test_subclass1);

    vfs_test_ops1.name = "testfs1";
    vfs_test_ops1.flags = VFSF_NOLINKS;
    vfs_test_ops1.prefix = "test1";
    vfs_register_class (&vfs_test_ops1);

    test_subclass2.flags = VFS_S_REMOTE;
    vfs_s_init_class (&vfs_test_ops2, &test_subclass2);
    vfs_test_ops2.name = "testfs2";
    vfs_test_ops2.prefix = "test2";
    vfs_register_class (&vfs_test_ops2);

    vfs_s_init_class (&vfs_test_ops3, &test_subclass3);
    vfs_test_ops3.name = "testfs3";
    vfs_test_ops3.prefix = "test3";
    vfs_register_class (&vfs_test_ops3);

}

static void
teardown (void)
{
    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */
#define ETALON_PATH_STR "/#test1/bla-bla/some/path/#test2/bla-bla/some/path#test3/111/22/33"
#define ETALON_PATH_URL_STR "/test1://bla-bla/some/path/test2://bla-bla/some/path/test3://111/22/33"
/* *INDENT-OFF* */
START_TEST (test_vfs_path_from_to_string)
/* *INDENT-ON* */
{
    vfs_path_t *vpath;
    size_t vpath_len;
    char *result;
    vpath = vfs_path_from_str_flags (ETALON_PATH_STR, VPF_USE_DEPRECATED_PARSER);


    vpath_len = vfs_path_elements_count (vpath);
    fail_unless (vpath_len == 4, "vpath length should be 4 (actial: %d)", vpath_len);

    result = vfs_path_to_str (vpath);
    fail_unless (strcmp (ETALON_PATH_URL_STR, result) == 0,
                 "expected(%s) doesn't equal to actual(%s)", ETALON_PATH_URL_STR, result);
    g_free (result);

    vfs_path_free (vpath);
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */
START_TEST (test_vfs_path_from_to_string2)
/* *INDENT-ON* */
{
    vfs_path_t *vpath;
    size_t vpath_len;
    char *result;
    const vfs_path_element_t *path_element;

    vpath = vfs_path_from_str ("/");

    vpath_len = vfs_path_elements_count (vpath);
    fail_unless (vpath_len == 1, "vpath length should be 1 (actial: %d)", vpath_len);

    result = vfs_path_to_str (vpath);
    fail_unless (strcmp ("/", result) == 0, "expected(%s) doesn't equal to actual(%s)", "/",
                 result);
    g_free (result);
    path_element = vfs_path_get_by_index (vpath, -1);
    fail_unless (strcmp ("/", path_element->path) == 0, "expected(%s) doesn't equal to actual(%s)",
                 "/", path_element->path);

    fail_unless (path_element->class == &vfs_local_ops,
                 "actual vfs-class doesn't equal to localfs");

    vfs_path_free (vpath);
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */
/* *INDENT-OFF* */
START_TEST (test_vfs_path_from_to_partial_string_by_class)
/* *INDENT-ON* */
{
    vfs_path_t *vpath;
    char *result;
    vpath = vfs_path_from_str_flags (ETALON_PATH_STR, VPF_USE_DEPRECATED_PARSER);


    result = vfs_path_to_str_elements_count (vpath, -1);
    fail_unless (strcmp ("/test1://bla-bla/some/path/test2://bla-bla/some/path", result) == 0,
                 "expected(%s) doesn't equal to actual(%s)",
                 "/test1://bla-bla/some/path/test2://bla-bla/some/path", result);
    g_free (result);

    result = vfs_path_to_str_elements_count (vpath, -2);
    fail_unless (strcmp ("/test1://bla-bla/some/path/", result) == 0,
                 "expected(%s) doesn't equal to actual(%s)", "/test1://bla-bla/some/path/", result);
    g_free (result);

    result = vfs_path_to_str_elements_count (vpath, -3);
    fail_unless (strcmp ("/", result) == 0,
                 "expected(%s) doesn't equal to actual(%s)", "/", result);
    g_free (result);

    /* index out of bound */
    result = vfs_path_to_str_elements_count (vpath, -4);
    fail_unless (strcmp ("", result) == 0, "expected(%s) doesn't equal to actual(%s)", "", result);
    g_free (result);


    result = vfs_path_to_str_elements_count (vpath, 1);
    fail_unless (strcmp ("/", result) == 0,
                 "expected(%s) doesn't equal to actual(%s)", "/", result);
    g_free (result);

    result = vfs_path_to_str_elements_count (vpath, 2);
    fail_unless (strcmp ("/test1://bla-bla/some/path/", result) == 0,
                 "expected(%s) doesn't equal to actual(%s)", "/test1://bla-bla/some/path/", result);
    g_free (result);

    result = vfs_path_to_str_elements_count (vpath, 3);
    fail_unless (strcmp ("/test1://bla-bla/some/path/test2://bla-bla/some/path", result) == 0,
                 "expected(%s) doesn't equal to actual(%s)",
                 "/test1://bla-bla/some/path/test2://bla-bla/some/path", result);
    g_free (result);

    result = vfs_path_to_str_elements_count (vpath, 4);
    fail_unless (strcmp (ETALON_PATH_URL_STR, result) == 0,
                 "expected(%s) doesn't equal to actual(%s)", ETALON_PATH_URL_STR, result);
    g_free (result);

    /* index out of bound */
    result = vfs_path_to_str_elements_count (vpath, 5);
    fail_unless (strcmp (ETALON_PATH_URL_STR, result) == 0,
                 "expected(%s) doesn't equal to actual(%s)", ETALON_PATH_URL_STR, result);
    g_free (result);

    vfs_path_free (vpath);
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */
#ifdef HAVE_CHARSET
#define encoding_check( input , etalon ) \
{ \
    vfs_path_t *vpath; \
    char *result; \
\
    vpath = vfs_path_from_str_flags (input, VPF_USE_DEPRECATED_PARSER); \
    result = vfs_path_to_str(vpath); \
    fail_unless( result != NULL && strcmp(result, etalon) ==0, \
    "\ninput : %s\nactual: %s\netalon: %s", input, result , etalon ); \
\
    g_free(result); \
    vfs_path_free(vpath); \
}

/* *INDENT-OFF* */
START_TEST (test_vfs_path_from_to_string_encoding)
/* *INDENT-ON* */
{
    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;
    load_codepages_list ();

    encoding_check
        ("/#test1/bla-bla1/some/path/#test2/bla-bla2/#enc:KOI8-R/some/path#test3/111/22/33",
         "/test1://bla-bla1/some/path/test2://#enc:KOI8-R/bla-bla2/some/path/test3://111/22/33");

    encoding_check
        ("/#test1/bla-bla1/#enc:IBM866/some/path/#test2/bla-bla2/#enc:KOI8-R/some/path#test3/111/22/33",
         "/test1://#enc:IBM866/bla-bla1/some/path/test2://#enc:KOI8-R/bla-bla2/some/path/test3://111/22/33");

    encoding_check
        ("/#test1/bla-bla1/some/path/#test2/bla-bla2/#enc:IBM866/#enc:KOI8-R/some/path#test3/111/22/33",
         "/test1://bla-bla1/some/path/test2://#enc:KOI8-R/bla-bla2/some/path/test3://111/22/33");

    encoding_check
        ("/#test1/bla-bla1/some/path/#test2/bla-bla2/#enc:IBM866/some/#enc:KOI8-R/path#test3/111/22/33",
         "/test1://bla-bla1/some/path/test2://#enc:KOI8-R/bla-bla2/some/path/test3://111/22/33");

    encoding_check
        ("/#test1/bla-bla1/some/path/#test2/#enc:IBM866/bla-bla2/#enc:KOI8-R/some/path#test3/111/22/33",
         "/test1://bla-bla1/some/path/test2://#enc:KOI8-R/bla-bla2/some/path/test3://111/22/33");

    encoding_check
        ("/#test1/bla-bla1/some/path/#enc:IBM866/#test2/bla-bla2/#enc:KOI8-R/some/path#test3/111/22/33",
         "/test1://#enc:IBM866/bla-bla1/some/path/test2://#enc:KOI8-R/bla-bla2/some/path/test3://111/22/33");

    free_codepages_list ();
}

/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */
#define ETALON_STR "/path/to/file.ext/test1://#enc:KOI8-R"
/* *INDENT-OFF* */
START_TEST (test_vfs_path_encoding_at_end)
/* *INDENT-ON* */
{
    vfs_path_t *vpath;
    char *result;
    const vfs_path_element_t *element;

    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;
    load_codepages_list ();

    vpath =
        vfs_path_from_str_flags ("/path/to/file.ext#test1:/#enc:KOI8-R", VPF_USE_DEPRECATED_PARSER);
    result = vfs_path_to_str (vpath);

    element = vfs_path_get_by_index (vpath, -1);
    fail_unless (*element->path == '\0', "element->path should be empty, but actual value is '%s'",
                 element->path);
    fail_unless (element->encoding != NULL
                 && strcmp (element->encoding, "KOI8-R") == 0,
                 "element->encoding should be 'KOI8-R', but actual value is '%s'",
                 element->encoding);

    fail_unless (result != NULL && strcmp (result, ETALON_STR) == 0,
                 "\nactual: %s\netalon: %s", result, ETALON_STR);

    g_free (result);
    vfs_path_free (vpath);

    free_codepages_list ();
}

/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */
#endif /* HAVE_CHARSET */
/* --------------------------------------------------------------------------------------------- */

#undef ETALON_PATH_STR
#define ETALON_PATH_STR "/test1://bla-bla/some/path/test2://user:passwd@some.host:1234/bla-bla/some/path/test3://111/22/33"
/* *INDENT-OFF* */
START_TEST (test_vfs_path_from_to_string_uri)
/* *INDENT-ON* */
{
    vfs_path_t *vpath;
    size_t vpath_len;
    char *result;
    vpath = vfs_path_from_str (ETALON_PATH_STR);

    vpath_len = vfs_path_elements_count (vpath);
    fail_unless (vpath_len == 4, "vpath length should be 4 (actial: %d)", vpath_len);

    result = vfs_path_to_str (vpath);
    fail_unless (strcmp (ETALON_PATH_STR, result) == 0,
                 "\nexpected(%s)\ndoesn't equal to actual(%s)", ETALON_PATH_STR, result);
    g_free (result);

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
    tcase_add_test (tc_core, test_vfs_path_from_to_string);
    tcase_add_test (tc_core, test_vfs_path_from_to_string2);
    tcase_add_test (tc_core, test_vfs_path_from_to_partial_string_by_class);
#ifdef HAVE_CHARSET
    tcase_add_test (tc_core, test_vfs_path_from_to_string_encoding);
    tcase_add_test (tc_core, test_vfs_path_encoding_at_end);
#endif
    tcase_add_test (tc_core, test_vfs_path_from_to_string_uri);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "vfs_path_string_convert.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
