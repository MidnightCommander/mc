/*
   lib/vfs - manipulate with current directory

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

#include "lib/global.h"
#include "lib/strutil.h"
#include "lib/vfs/xdirentry.h"

#include "src/vfs/local/local.c"

static void
setup (void)
{
    str_init_strings (NULL);

    vfs_init ();
    init_localfs ();
    vfs_setup_work_dir ();
}

static void
teardown (void)
{
    vfs_shut ();
    str_uninit_strings ();
}

static int
test_chdir (const vfs_path_t * vpath)
{
#if 0
    char *path = vfs_path_to_str (vpath);
    printf ("test_chdir: %s\n", path);
    g_free (path);
#else
    (void) vpath;
#endif
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

#define cd_and_check( cd_dir, etalon ) \
    vpath = vfs_path_from_str (cd_dir); \
    mc_chdir(vpath); \
    vfs_path_free (vpath); \
    buffer = _vfs_get_cwd (); \
    fail_unless( \
        strcmp(etalon, buffer) == 0, \
        "\n expected(%s) doesn't equal \nto actual(%s)", etalon, buffer); \
    g_free (buffer);

/* *INDENT-OFF* */
START_TEST (set_up_current_dir_url)
/* *INDENT-ON* */
{
    vfs_path_t *vpath;
    static struct vfs_s_subclass test_subclass;
    static struct vfs_class vfs_test_ops;
    char *buffer;

    vfs_s_init_class (&vfs_test_ops, &test_subclass);

    vfs_test_ops.name = "testfs";
    vfs_test_ops.flags = VFSF_NOLINKS;
    vfs_test_ops.prefix = "test";
    vfs_test_ops.chdir = test_chdir;

    vfs_register_class (&vfs_test_ops);

    cd_and_check ("/dev/some.file/test://", "/dev/some.file/test://");

    cd_and_check ("/dev/some.file/test://bla-bla", "/dev/some.file/test://bla-bla");

    cd_and_check ("..", "/dev/some.file/test://");

    cd_and_check ("..", "/dev");

    cd_and_check ("..", "/");

    cd_and_check ("..", "/");

    test_subclass.flags = VFS_S_REMOTE;

    cd_and_check ("/test://user:pass@host.net/path", "/test://user:pass@host.net/path");
    cd_and_check ("..", "/test://user:pass@host.net/");

    cd_and_check ("..", "/");

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
    tcase_add_test (tc_core, set_up_current_dir_url);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "current_dir.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
