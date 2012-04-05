/*
   lib/vfs - vfs_path_t charset recode functions

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

#ifndef HAVE_CHARSET
#define HAVE_CHARSET 1
#endif

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

#define path_recode_one_check(input, etalon1, etalon2) {\
    vpath = vfs_path_from_str (input);\
    element = vfs_path_get_by_index(vpath, -1);\
    fail_unless ( strcmp (element->path, etalon1) == 0, "expected: %s\nactual: %s\n", etalon1, element->path);\
    result = vfs_path_to_str(vpath);\
    fail_unless ( strcmp (result, etalon2) == 0, "\nexpected: %s\nactual: %s\n", etalon2, result);\
    g_free(result);\
    vfs_path_free (vpath);\
}

START_TEST (test_path_recode_base_utf8)
{
    vfs_path_t *vpath;
    char *result;
    const vfs_path_element_t *element;

    str_init_strings ("UTF-8");

    vfs_init ();
    init_localfs ();
    vfs_setup_work_dir ();

    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;

    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;
    load_codepages_list ();

    path_recode_one_check("/тестовый/путь", "/тестовый/путь", "/тестовый/путь");

    path_recode_one_check("/#enc:KOI8-R/я┌п╣я│я┌п╬п╡я▀п╧/п©я┐я┌я▄", "/тестовый/путь", "/#enc:KOI8-R/я┌п╣я│я┌п╬п╡я▀п╧/п©я┐я┌я▄");

    free_codepages_list ();
    str_uninit_strings ();
    vfs_shut ();

}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_path_recode_base_koi8r)
{
    vfs_path_t *vpath;
    char *result;
    const vfs_path_element_t *element;

    str_init_strings ("KOI8-R");

    vfs_init ();
    init_localfs ();
    vfs_setup_work_dir ();

    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;

    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;
    load_codepages_list ();

    path_recode_one_check("/я┌п╣я│я┌п╬п╡я▀п╧/п©я┐я┌я▄", "/я┌п╣я│я┌п╬п╡я▀п╧/п©я┐я┌я▄", "/я┌п╣я│я┌п╬п╡я▀п╧/п©я┐я┌я▄");

    path_recode_one_check("/#enc:UTF-8/тестовый/путь", "/я┌п╣я│я┌п╬п╡я▀п╧/п©я┐я┌я▄", "/#enc:UTF-8/тестовый/путь");

    free_codepages_list ();
    str_uninit_strings ();
    vfs_shut ();

}
END_TEST

/* --------------------------------------------------------------------------------------------- */
struct vfs_s_subclass test_subclass1;
struct vfs_class vfs_test_ops1;

START_TEST(test_path_to_str_flags)
{

    vfs_path_t *vpath;
    char *str_path;

    str_init_strings ("UTF-8");

    vfs_init ();
    init_localfs ();

    vfs_setup_work_dir ();
    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;
    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;
    load_codepages_list ();

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

    vpath = vfs_path_from_str ("/test1://user:passwd@host.name/#enc:KOI8-R/я┌п╣я│я┌п╬п╡я▀п╧/п©я┐я┌я▄");
    str_path = vfs_path_to_str_flags (vpath, 0, VPF_STRIP_PASSWORD);
    fail_unless (strcmp ("/test1://user@host.name/#enc:KOI8-R/я┌п╣я│я┌п╬п╡я▀п╧/п©я┐я┌я▄", str_path) == 0, "\nstr=%s\n", str_path);
    g_free (str_path);

    str_path = vfs_path_to_str_flags (vpath, 0, VPF_RECODE);
    fail_unless (strcmp ("/test1://user:passwd@host.name/тестовый/путь", str_path) == 0, "\nstr=%s\n", str_path);
    g_free (str_path);

    str_path = vfs_path_to_str_flags (vpath, 0, VPF_RECODE | VPF_STRIP_PASSWORD);
    fail_unless (strcmp ("/test1://user@host.name/тестовый/путь", str_path) == 0, "\nstr=%s\n", str_path);
    g_free (str_path);

    vfs_path_free (vpath);

    vpath = vfs_path_build_filename (mc_config_get_home_dir (), "test", "dir", NULL);
    str_path = vfs_path_to_str_flags (vpath, 0, VPF_STRIP_HOME);
    fail_unless (strcmp ("~/test/dir", str_path) == 0, "\nstr=%s\n", str_path);
    g_free (str_path);
    vfs_path_free (vpath);

    vpath = vfs_path_build_filename (mc_config_get_home_dir (), "test1://user:passwd@host.name/#enc:KOI8-R/я┌п╣я│я┌п╬п╡я▀п╧/п©я┐я┌я▄", NULL);
    str_path = vfs_path_to_str_flags (vpath, 0, VPF_STRIP_HOME | VPF_STRIP_PASSWORD);
    fail_unless (strcmp ("~/test1://user@host.name/#enc:KOI8-R/я┌п╣я│я┌п╬п╡я▀п╧/п©я┐я┌я▄", str_path) == 0, "\nstr=%s\n", str_path);
    g_free (str_path);
    str_path = vfs_path_to_str_flags (vpath, 0, VPF_STRIP_HOME | VPF_STRIP_PASSWORD | VPF_HIDE_CHARSET);
    fail_unless (strcmp ("~/test1://user@host.name/я┌п╣я│я┌п╬п╡я▀п╧/п©я┐я┌я▄", str_path) == 0, "\nstr=%s\n", str_path);
    g_free (str_path);
    str_path = vfs_path_to_str_flags (vpath, 0, VPF_STRIP_HOME | VPF_RECODE);
    fail_unless (strcmp ("~/test1://user:passwd@host.name/тестовый/путь", str_path) == 0, "\nstr=%s\n", str_path);
    g_free (str_path);
    str_path = vfs_path_to_str_flags (vpath, 0, VPF_STRIP_HOME | VPF_RECODE | VPF_STRIP_PASSWORD);
    fail_unless (strcmp ("~/test1://user@host.name/тестовый/путь", str_path) == 0, "\nstr=%s\n", str_path);
    g_free (str_path);
    vfs_path_free (vpath);

    free_codepages_list ();
    str_uninit_strings ();
    vfs_shut ();

}
END_TEST

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
