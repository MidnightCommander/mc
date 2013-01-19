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

const char *
mc_config_get_home_dir (void)
{
    return "/mock/home";
}


static void
setup (void)
{
}

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

#define path_recode_one_check(input, etalon1, etalon2) {\
    vpath = vfs_path_from_str (input);\
    element = vfs_path_get_by_index(vpath, -1);\
    fail_unless ( strcmp (element->path, etalon1) == 0, "expected: %s\nactual: %s\n", etalon1, element->path);\
    result = vfs_path_to_str(vpath);\
    fail_unless ( strcmp (result, etalon2) == 0, "\nexpected: %s\nactual: %s\n", etalon2, result);\
    g_free(result);\
    vfs_path_free (vpath);\
}

/* *INDENT-OFF* */
START_TEST (test_path_recode_base_utf8)
/* *INDENT-ON* */
{
    vfs_path_t *vpath;
    char *result;
    const vfs_path_element_t *element;

    test_init_vfs ("UTF-8");

    path_recode_one_check ("/ÔÅÓÔÏ×ÙÊ/ÐÕÔØ", "/ÔÅÓÔÏ×ÙÊ/ÐÕÔØ", "/ÔÅÓÔÏ×ÙÊ/ÐÕÔØ");

    path_recode_one_check ("/#enc:KOI8-R/Ñ‚ÐµÑÑ‚Ð¾Ð²Ñ‹Ð¹/Ð¿ÑƒÑ‚ÑŒ", "/ÔÅÓÔÏ×ÙÊ/ÐÕÔØ",
                           "/#enc:KOI8-R/Ñ‚ÐµÑÑ‚Ð¾Ð²Ñ‹Ð¹/Ð¿ÑƒÑ‚ÑŒ");

    test_deinit_vfs ();
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */
START_TEST (test_path_recode_base_koi8r)
/* *INDENT-ON* */
{
    vfs_path_t *vpath;
    char *result;
    const vfs_path_element_t *element;

    test_init_vfs ("KOI8-R");

    path_recode_one_check ("/Ñ‚ÐµÑÑ‚Ð¾Ð²Ñ‹Ð¹/Ð¿ÑƒÑ‚ÑŒ", "/Ñ‚ÐµÑÑ‚Ð¾Ð²Ñ‹Ð¹/Ð¿ÑƒÑ‚ÑŒ",
                           "/Ñ‚ÐµÑÑ‚Ð¾Ð²Ñ‹Ð¹/Ð¿ÑƒÑ‚ÑŒ");

    path_recode_one_check ("/#enc:UTF-8/ÔÅÓÔÏ×ÙÊ/ÐÕÔØ", "/Ñ‚ÐµÑÑ‚Ð¾Ð²Ñ‹Ð¹/Ð¿ÑƒÑ‚ÑŒ",
                           "/#enc:UTF-8/ÔÅÓÔÏ×ÙÊ/ÐÕÔØ");

    test_deinit_vfs ();
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */
struct vfs_s_subclass test_subclass1;
struct vfs_class vfs_test_ops1;

/* *INDENT-OFF* */
START_TEST(test_path_to_str_flags)
/* *INDENT-ON* */
{

    vfs_path_t *vpath;
    char *str_path;

    test_init_vfs ("UTF-8");

    test_subclass1.flags = VFS_S_REMOTE;
    vfs_s_init_class (&vfs_test_ops1, &test_subclass1);
    vfs_test_ops1.name = "testfs1";
    vfs_test_ops1.flags = VFSF_NOLINKS;
    vfs_test_ops1.prefix = "test1";
    vfs_register_class (&vfs_test_ops1);

    vpath = vfs_path_from_str_flags ("test1://user:passwd@127.0.0.1", VPF_NO_CANON);
    str_path = vfs_path_to_str_flags (vpath, 0, VPF_STRIP_PASSWORD);
    fail_unless (strcmp ("test1://user@127.0.0.1/", str_path) == 0, "\nstr=%s\n", str_path);
    g_free (str_path);
    vfs_path_free (vpath);

    vpath =
        vfs_path_from_str ("/test1://user:passwd@host.name/#enc:KOI8-R/Ñ‚ÐµÑÑ‚Ð¾Ð²Ñ‹Ð¹/Ð¿ÑƒÑ‚ÑŒ");
    str_path = vfs_path_to_str_flags (vpath, 0, VPF_STRIP_PASSWORD);
    fail_unless (strcmp ("/test1://user@host.name/#enc:KOI8-R/Ñ‚ÐµÑÑ‚Ð¾Ð²Ñ‹Ð¹/Ð¿ÑƒÑ‚ÑŒ", str_path)
                 == 0, "\nstr=%s\n", str_path);
    g_free (str_path);

    str_path = vfs_path_to_str_flags (vpath, 0, VPF_RECODE);
    fail_unless (strcmp ("/test1://user:passwd@host.name/ÔÅÓÔÏ×ÙÊ/ÐÕÔØ", str_path) == 0,
                 "\nstr=%s\n", str_path);
    g_free (str_path);

    str_path = vfs_path_to_str_flags (vpath, 0, VPF_RECODE | VPF_STRIP_PASSWORD);
    fail_unless (strcmp ("/test1://user@host.name/ÔÅÓÔÏ×ÙÊ/ÐÕÔØ", str_path) == 0, "\nstr=%s\n",
                 str_path);
    g_free (str_path);

    vfs_path_free (vpath);

    vpath = vfs_path_build_filename (mc_config_get_home_dir (), "test", "dir", NULL);
    str_path = vfs_path_to_str_flags (vpath, 0, VPF_STRIP_HOME);
    fail_unless (strcmp ("~/test/dir", str_path) == 0, "\nstr=%s\n", str_path);
    g_free (str_path);
    vfs_path_free (vpath);

    vpath =
        vfs_path_build_filename (mc_config_get_home_dir (),
                                 "test1://user:passwd@host.name/#enc:KOI8-R/Ñ‚ÐµÑÑ‚Ð¾Ð²Ñ‹Ð¹/Ð¿ÑƒÑ‚ÑŒ",
                                 NULL);
    str_path = vfs_path_to_str_flags (vpath, 0, VPF_STRIP_HOME | VPF_STRIP_PASSWORD);
    fail_unless (strcmp ("~/test1://user@host.name/#enc:KOI8-R/Ñ‚ÐµÑÑ‚Ð¾Ð²Ñ‹Ð¹/Ð¿ÑƒÑ‚ÑŒ", str_path)
                 == 0, "\nstr=%s\n", str_path);
    g_free (str_path);
    str_path =
        vfs_path_to_str_flags (vpath, 0, VPF_STRIP_HOME | VPF_STRIP_PASSWORD | VPF_HIDE_CHARSET);
    fail_unless (strcmp ("~/test1://user@host.name/Ñ‚ÐµÑÑ‚Ð¾Ð²Ñ‹Ð¹/Ð¿ÑƒÑ‚ÑŒ", str_path) == 0,
                 "\nstr=%s\n", str_path);
    g_free (str_path);
    str_path = vfs_path_to_str_flags (vpath, 0, VPF_STRIP_HOME | VPF_RECODE);
    fail_unless (strcmp ("~/test1://user:passwd@host.name/ÔÅÓÔÏ×ÙÊ/ÐÕÔØ", str_path) == 0,
                 "\nstr=%s\n", str_path);
    g_free (str_path);
    str_path = vfs_path_to_str_flags (vpath, 0, VPF_STRIP_HOME | VPF_RECODE | VPF_STRIP_PASSWORD);
    fail_unless (strcmp ("~/test1://user@host.name/ÔÅÓÔÏ×ÙÊ/ÐÕÔØ", str_path) == 0, "\nstr=%s\n",
                 str_path);
    g_free (str_path);
    vfs_path_free (vpath);

    test_deinit_vfs ();
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */
START_TEST(test_encode_info_at_start)
/* *INDENT-ON* */
{
    vfs_path_t *vpath;
    char *actual;
    const vfs_path_element_t *vpath_element;

    test_init_vfs ("UTF-8");

    vpath = vfs_path_from_str ("#enc:KOI8-R/bla-bla/some/path");
    actual = vfs_path_to_str (vpath);

    fail_unless (strcmp ("/#enc:KOI8-R/bla-bla/some/path", actual) == 0, "\nactual=%s\n", actual);

    vpath_element = vfs_path_get_by_index (vpath, -1);

    fail_unless (strcmp ("/bla-bla/some/path", vpath_element->path) == 0,
                 "\nvpath_element->path=%s\n", vpath_element->path);

    g_free (actual);
    vfs_path_free (vpath);

    test_deinit_vfs ();
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
    tcase_add_test (tc_core, test_path_recode_base_utf8);
    tcase_add_test (tc_core, test_path_recode_base_koi8r);
    tcase_add_test (tc_core, test_path_to_str_flags);
    tcase_add_test (tc_core, test_encode_info_at_start);
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
