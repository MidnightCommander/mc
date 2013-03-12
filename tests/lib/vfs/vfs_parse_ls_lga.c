/*
   lib/vfs - test vfs_parse_ls_lga() functionality

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

#include <stdio.h>

#include "lib/vfs/utilvfs.h"
#include "lib/vfs/xdirentry.h"
#include "lib/strutil.h"

#include "src/vfs/local/local.c"


struct vfs_s_subclass test_subclass1;
struct vfs_class vfs_test_ops1;

struct vfs_s_entry *vfs_root_entry;
static struct vfs_s_inode *vfs_root_inode;
static struct vfs_s_super *vfs_test_super;

void message (int flags, const char *title, const char *text, ...);

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    static struct stat initstat;

    str_init_strings (NULL);

    vfs_init ();
    init_localfs ();
    vfs_setup_work_dir ();

    test_subclass1.flags = VFS_S_REMOTE;
    vfs_s_init_class (&vfs_test_ops1, &test_subclass1);
    vfs_test_ops1.name = "testfs1";
    vfs_test_ops1.flags = VFSF_NOLINKS;
    vfs_test_ops1.prefix = "test1:";
    vfs_register_class (&vfs_test_ops1);

    vfs_test_super = g_new0 (struct vfs_s_super, 1);
    vfs_test_super->me = &vfs_test_ops1;

    vfs_root_inode = vfs_s_new_inode (&vfs_test_ops1, vfs_test_super, &initstat);
    vfs_root_entry = vfs_s_new_entry (&vfs_test_ops1, "/", vfs_root_inode);
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    vfs_s_free_entry (&vfs_test_ops1, vfs_root_entry);
    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* @Mock */
void
message (int flags, const char *title, const char *text, ...)
{
    char *p;
    va_list ap;

    (void) flags;
    (void) title;

    va_start (ap, text);
    p = g_strdup_vprintf (text, ap);
    va_end (ap);
    printf ("message(): %s\n", p);
    g_free (p);
}

/* --------------------------------------------------------------------------------------------- */

static void
fill_stat_struct (struct stat *etalon_stat, int iterator)
{
    switch (iterator)
    {
    case 0:
        etalon_stat->st_dev = 0;
        etalon_stat->st_ino = 0;
        etalon_stat->st_mode = 0x41fd;
        etalon_stat->st_nlink = 10;
        etalon_stat->st_uid = 500;
        etalon_stat->st_gid = 500;
        etalon_stat->st_rdev = 0;
        etalon_stat->st_size = 4096;
        etalon_stat->st_blksize = 512;
        etalon_stat->st_blocks = 8;
        etalon_stat->st_atime = 1308838140;
        etalon_stat->st_mtime = 1308838140;
        etalon_stat->st_ctime = 1308838140;
        break;
    case 1:
        etalon_stat->st_dev = 0;
        etalon_stat->st_ino = 0;
        etalon_stat->st_mode = 0xa1ff;
        etalon_stat->st_nlink = 10;
        etalon_stat->st_uid = 500;
        etalon_stat->st_gid = 500;
        etalon_stat->st_rdev = 0;
        etalon_stat->st_size = 11;
        etalon_stat->st_blksize = 512;
        etalon_stat->st_blocks = 1;
        etalon_stat->st_atime = 1268431200;
        etalon_stat->st_mtime = 1268431200;
        etalon_stat->st_ctime = 1268431200;
        break;
    case 2:
        etalon_stat->st_dev = 0;
        etalon_stat->st_ino = 0;
        etalon_stat->st_mode = 0x41fd;
        etalon_stat->st_nlink = 10;
        etalon_stat->st_uid = 500;
        etalon_stat->st_gid = 500;
        etalon_stat->st_rdev = 0;
        etalon_stat->st_size = 4096;
        etalon_stat->st_blksize = 512;
        etalon_stat->st_blocks = 8;
        etalon_stat->st_atime = 1308838140;
        etalon_stat->st_mtime = 1308838140;
        etalon_stat->st_ctime = 1308838140;
        break;
    case 3:
        etalon_stat->st_dev = 0;
        etalon_stat->st_ino = 0;
        etalon_stat->st_mode = 0x41fd;
        etalon_stat->st_nlink = 10;
        etalon_stat->st_uid = 500;
        etalon_stat->st_gid = 500;
        etalon_stat->st_rdev = 0;
        etalon_stat->st_size = 4096;
        etalon_stat->st_blksize = 512;
        etalon_stat->st_blocks = 8;
        etalon_stat->st_atime = 1308838140;
        etalon_stat->st_mtime = 1308838140;
        etalon_stat->st_ctime = 1308838140;
        break;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_vfs_parse_ls_lga_ds") */
/* *INDENT-OFF* */
static const struct test_vfs_parse_ls_lga_ds
{
    const char *input_string;
    int expected_result;
    const char *expected_filename;
    const char *expected_linkname;
    const size_t expected_filepos;
} test_vfs_parse_ls_lga_ds[] =
{
    { /* 0. */
        "drwxrwxr-x   10 500      500          4096 Jun 23 17:09 build_root",
        1,
        "build_root",
        NULL,
        0
    },
    { /* 1. */
        "lrwxrwxrwx    1 500      500            11 Mar 13  2010 COPYING -> doc/COPYING",
        1,
        "COPYING",
        "doc/COPYING",
        0
    },
    { /* 2. */
        "drwxrwxr-x   10 500      500          4096 Jun 23 17:09 ..",
        1,
        "..",
        NULL,
        0
    },
    { /* 3. */
        "drwxrwxr-x   10 500      500          4096 Jun 23 17:09   build_root",
        1,
        "build_root",
        NULL,
        0
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_vfs_parse_ls_lga_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_vfs_parse_ls_lga, test_vfs_parse_ls_lga_ds)
/* *INDENT-ON* */

{
    /* given */
    size_t filepos = 0;
    struct stat etalon_stat;
    static struct stat test_stat;
    char *filename = NULL;
    char *linkname = NULL;
    gboolean actual_result;

    vfs_parse_ls_lga_init ();

    etalon_stat.st_blocks = 0;
    etalon_stat.st_size = 0;
    etalon_stat.st_mode = 0;
    fill_stat_struct (&etalon_stat, _i);

    /* when */
    actual_result =
        vfs_parse_ls_lga (data->input_string, &test_stat, &filename, &linkname, &filepos);

    /* then */
    mctest_assert_int_eq (actual_result, data->expected_result);

    mctest_assert_str_eq (filename, data->expected_filename);
    mctest_assert_str_eq (linkname, data->expected_linkname);

    mctest_assert_int_eq (etalon_stat.st_dev, test_stat.st_dev);
    mctest_assert_int_eq (etalon_stat.st_ino, test_stat.st_ino);
    mctest_assert_int_eq (etalon_stat.st_mode, test_stat.st_mode);
    mctest_assert_int_eq (etalon_stat.st_uid, test_stat.st_uid);
    mctest_assert_int_eq (etalon_stat.st_gid, test_stat.st_gid);
    mctest_assert_int_eq (etalon_stat.st_rdev, test_stat.st_rdev);
    mctest_assert_int_eq (etalon_stat.st_size, test_stat.st_size);
    mctest_assert_int_eq (etalon_stat.st_blksize, test_stat.st_blksize);
    mctest_assert_int_eq (etalon_stat.st_blocks, test_stat.st_blocks);

    /* FIXME: these commented checks are related to time zone!
       mctest_assert_int_eq (etalon_stat.st_atime, test_stat.st_atime);
       mctest_assert_int_eq (etalon_stat.st_mtime, test_stat.st_mtime);
       mctest_assert_int_eq (etalon_stat.st_ctime, test_stat.st_ctime);
     */
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* @Test */
/* *INDENT-OFF* */
START_TEST (test_vfs_parse_ls_lga_reorder)
/* *INDENT-ON* */
{
    /* given */
    size_t filepos = 0;
    struct vfs_s_entry *ent1, *ent2, *ent3;

    vfs_parse_ls_lga_init ();

    /* init ent1 */
    ent1 = vfs_s_generate_entry (&vfs_test_ops1, NULL, vfs_root_inode, 0);
    vfs_parse_ls_lga
        ("drwxrwxr-x   10 500      500          4096 Jun 23 17:09      build_root1", &ent1->ino->st,
         &ent1->name, &ent1->ino->linkname, &filepos);
    vfs_s_store_filename_leading_spaces (ent1, filepos);
    vfs_s_insert_entry (&vfs_test_ops1, vfs_root_inode, ent1);


    /* init ent2 */
    ent2 = vfs_s_generate_entry (&vfs_test_ops1, NULL, vfs_root_inode, 0);
    vfs_parse_ls_lga ("drwxrwxr-x   10 500      500          4096 Jun 23 17:09    build_root2",
                      &ent2->ino->st, &ent2->name, &ent2->ino->linkname, &filepos);
    vfs_s_store_filename_leading_spaces (ent2, filepos);
    vfs_s_insert_entry (&vfs_test_ops1, vfs_root_inode, ent2);

    /* init ent3 */
    ent3 = vfs_s_generate_entry (&vfs_test_ops1, NULL, vfs_root_inode, 0);
    vfs_parse_ls_lga ("drwxrwxr-x   10 500      500          4096 Jun 23 17:09 ..",
                      &ent3->ino->st, &ent3->name, &ent3->ino->linkname, &filepos);
    vfs_s_store_filename_leading_spaces (ent3, filepos);
    vfs_s_insert_entry (&vfs_test_ops1, vfs_root_inode, ent3);

    /* when */
    vfs_s_normalize_filename_leading_spaces (vfs_root_inode, vfs_parse_ls_lga_get_final_spaces ());

    /* then */
    mctest_assert_str_eq (ent1->name, "     build_root1");
    mctest_assert_str_eq (ent2->name, "   build_root2");
}
/* *INDENT-OFF* */
END_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */
#define parce_one_line(ent_index, ls_output) {\
    ent[ent_index] = vfs_s_generate_entry (&vfs_test_ops1, NULL, vfs_root_inode, 0);\
    if (! vfs_parse_ls_lga (ls_output,\
    &ent[ent_index]->ino->st, &ent[ent_index]->name, &ent[ent_index]->ino->linkname, &filepos))\
    {\
        fail ("An error occurred while parse ls output");\
        return;\
    }\
    vfs_s_store_filename_leading_spaces (ent[ent_index], filepos);\
    vfs_s_insert_entry (&vfs_test_ops1, vfs_root_inode, ent[ent_index]);\
    \
}

/* @Test */
/* *INDENT-OFF* */
START_TEST (test_vfs_parse_ls_lga_unaligned)
/* *INDENT-ON* */
{
    /* given */
    size_t filepos = 0;
    struct vfs_s_entry *ent[4];

    vfs_parse_ls_lga_init ();

    parce_one_line (0, "drwxrwxr-x   10 500      500          4096 Jun 23 17:09  build_root1");
    parce_one_line (1, "drwxrwxr-x   10 500     500         4096 Jun 23 17:09     build_root2");
    parce_one_line (2, "drwxrwxr-x 10 500 500 4096 Jun 23 17:09  ..");
    parce_one_line (3,
                    "drwxrwxr-x      10   500        500             4096   Jun   23   17:09   build_root 0");

    /* when */
    vfs_s_normalize_filename_leading_spaces (vfs_root_inode, vfs_parse_ls_lga_get_final_spaces ());

    /* then */
    mctest_assert_str_eq (ent[0]->name, "build_root1");
    mctest_assert_str_eq (ent[0]->name, "build_root1");
    mctest_assert_str_eq (ent[1]->name, "   build_root2");
    mctest_assert_str_eq (ent[3]->name, " build_root 0");
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
    mctest_add_parameterized_test (tc_core, test_vfs_parse_ls_lga, test_vfs_parse_ls_lga_ds);
    tcase_add_test (tc_core, test_vfs_parse_ls_lga_reorder);
    tcase_add_test (tc_core, test_vfs_parse_ls_lga_unaligned);
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "vfs_parse_ls_lga.log");
    srunner_run_all (sr, CK_NORMAL);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
