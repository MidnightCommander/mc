/*
   lib/vfs - test vfs_s_get_path() function

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

#include "lib/strutil.h"
#include "lib/vfs/direntry.c"   /* for testing static methods  */

#include "src/vfs/local/local.c"

#define ARCH_NAME "/path/to/some/file.ext"
#define ETALON_PATH "path/to/test1_file.ext"
#define ETALON_VFS_NAME "#test2:user:pass@host.net"
#define ETALON_VFS_URL_NAME "test2://user:pass@host.net"

struct vfs_s_subclass test_subclass1, test_subclass2, test_subclass3;
static struct vfs_class *vfs_test_ops1 = VFS_CLASS (&test_subclass1);
static struct vfs_class *vfs_test_ops2 = VFS_CLASS (&test_subclass2);
static struct vfs_class *vfs_test_ops3 = VFS_CLASS (&test_subclass3);

/* --------------------------------------------------------------------------------------------- */

static int
test1_mock_open_archive (struct vfs_s_super *super, const vfs_path_t *vpath,
                         const vfs_path_element_t *vpath_element)
{
    struct vfs_s_inode *root;

    mctest_assert_str_eq (vfs_path_as_str (vpath), "/" ETALON_VFS_URL_NAME ARCH_NAME);

    super->name = g_strdup (vfs_path_as_str (vpath));
    root = vfs_s_new_inode (vpath_element->class, super, NULL);
    super->root = root;
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
test1_mock_archive_same (const vfs_path_element_t *vpath_element, struct vfs_s_super *super,
                         const vfs_path_t *vpath, void *cookie)
{
    const char *path;

    (void) vpath_element;
    (void) super;
    (void) cookie;

    path = vfs_path_get_last_path_str (vpath);
    return (strcmp (ARCH_NAME, path) != 0 ? 0 : 1);
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

    vfs_init_subclass (&test_subclass1, "testfs1", VFSF_NOLINKS | VFSF_REMOTE, "test1");
    test_subclass1.open_archive = test1_mock_open_archive;
    test_subclass1.archive_same = test1_mock_archive_same;
    test_subclass1.archive_check = NULL;
    vfs_register_class (vfs_test_ops1);

    vfs_init_subclass (&test_subclass2, "testfs2", VFSF_UNKNOWN, "test2");
    vfs_register_class (vfs_test_ops2);

    vfs_init_subclass (&test_subclass3, "testfs3", VFSF_UNKNOWN, "test3");
    vfs_register_class (vfs_test_ops3);
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    vfs_shut ();
    str_uninit_strings ();
}


void
vfs_die (const char *m)
{
    printf ("VFS_DIE: '%s'\n", m);
}

/* --------------------------------------------------------------------------------------------- */

/* @Test */
/* *INDENT-OFF* */
START_TEST (test_vfs_s_get_path)
/* *INDENT-ON* */
{
    /* given */
    struct vfs_s_super *archive;
    const char *result;

    /* when */
    vfs_path_t *vpath =
        vfs_path_from_str_flags ("/" ETALON_VFS_NAME ARCH_NAME "#test1:/" ETALON_PATH,
                                 VPF_USE_DEPRECATED_PARSER);

    result = vfs_s_get_path (vpath, &archive, 0);

    /* then */
    mctest_assert_str_eq (result, ETALON_PATH);
    mctest_assert_str_eq (archive->name, "/" ETALON_VFS_URL_NAME ARCH_NAME);

    g_free (vpath);

}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    /* Add new tests here: *************** */
    tcase_add_test (tc_core, test_vfs_s_get_path);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
