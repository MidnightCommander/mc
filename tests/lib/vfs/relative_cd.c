/* lib/vfs - test vfs_path_t manipulation functions

   Copyright (C) 2011 Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2011

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

#include <check.h>

#include "lib/global.c"

#include "lib/strutil.h"
#include "lib/vfs/xdirentry.h"
#include "lib/vfs/path.h"

#include "src/vfs/local/local.c"


struct vfs_s_subclass test_subclass1;
struct vfs_class vfs_test_ops1;

static int test_chdir (const vfs_path_t * vpath);


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
}

static void
teardown (void)
{
    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

static int
test_chdir (const vfs_path_t * vpath)
{
    char *path = vfs_path_to_str (vpath);
    printf ("test_chdir: %s\n", path);
    g_free (path);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

/* *INDENT-OFF* */
START_TEST (test_relative_cd)
/* *INDENT-ON* */
{
    vfs_path_t *vpath;

    vpath = vfs_path_from_str ("/test1://user:pass@some.host:12345/path/to/dir");
    fail_if (mc_chdir (vpath) == -1);
    vfs_path_free (vpath);

    vpath = vfs_path_from_str_flags ("some-non-exists-dir", VPF_NO_CANON);
    fail_if (mc_chdir (vpath) == -1);
    vfs_path_free (vpath);
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* Relative to panel_correct_path_to_show()  */

/* *INDENT-OFF* */
START_TEST (test_vpath_to_str_filter)
/* *INDENT-ON* */
{
    vfs_path_t *vpath, *last_vpath;
    char *filtered_path;
    const vfs_path_element_t *path_element;

    vpath = vfs_path_from_str ("/test1://some.host/dir");
    path_element = vfs_path_element_clone (vfs_path_get_by_index (vpath, -1));
    vfs_path_free (vpath);

    last_vpath = vfs_path_new ();
    last_vpath->relative = TRUE;

    vfs_path_add_element (last_vpath, path_element);

    filtered_path = vfs_path_to_str_flags (last_vpath, 0,
                                           VPF_STRIP_HOME | VPF_STRIP_PASSWORD | VPF_HIDE_CHARSET);
    vfs_path_free (last_vpath);

    fail_unless (strcmp ("test1://some.host/dir", filtered_path) == 0, "actual: %s", filtered_path);
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
    tcase_add_test (tc_core, test_relative_cd);
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
