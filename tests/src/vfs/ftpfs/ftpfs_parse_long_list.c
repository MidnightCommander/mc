/* src/vfs/ftpfs - tests for ftpfs_parse_long_list() function.

   Copyright (C) 2021-2024
   Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2021

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

#define TEST_SUITE_NAME "/src/vfs/ftpfs"

#include "tests/mctest.h"

#include <stdio.h>

#include "direntry.c"
#include "src/vfs/ftpfs/ftpfs_parse_ls.c"

/* --------------------------------------------------------------------------------------------- */

static struct vfs_s_subclass ftpfs_subclass;
static struct vfs_class *me = VFS_CLASS (&ftpfs_subclass);

static struct vfs_s_super *super;

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    vfs_init_subclass (&ftpfs_subclass, "ftpfs", VFSF_NOLINKS | VFSF_REMOTE | VFSF_USETMP, "ftp");

    super = vfs_s_new_super (me);
    super->name = g_strdup (PATH_SEP_STR);
    super->root = vfs_s_new_inode (me, super, vfs_s_default_stat (me, S_IFDIR | 0755));
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    vfs_s_free_super (me, super);
}

/* --------------------------------------------------------------------------------------------- */

static GSList *
read_list (const char *fname)
{
    FILE *f;
    char buf[BUF_MEDIUM];
    GSList *ret = NULL;

    f = fopen (fname, "r");
    if (f == NULL)
        return NULL;

    while (fgets (buf, sizeof (buf), f) != NULL)
        ret = g_slist_prepend (ret, g_strdup (buf));

    fclose (f);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_ftpfs_parse_long_list_ds") */
/* *INDENT-OFF* */
static const struct test_ftpfs_parse_long_list_ds
{
    const char *name;
} test_ftpfs_parse_long_list_ds[] =
{
    { /* 0. Ticket #2841 */
        "aix"
    },
    { /* 1. Ticket #3174 */
        "ms"
    }
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_ftpfs_parse_long_list_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_ftpfs_parse_long_list, test_ftpfs_parse_long_list_ds)
/* *INDENT-ON* */
{
    /* given */
    char *name;
    GSList *input, *parsed, *output;
    GSList *parsed_iter, *output_iter;
    int err_count;

    /* when */
    name = g_strdup_printf ("%s/%s_list.input", TEST_DATA_DIR, data->name);
    input = read_list (name);
    g_free (name);
    mctest_assert_not_null (input);

    name = g_strdup_printf ("%s/%s_list.output", TEST_DATA_DIR, data->name);
    output = read_list (name);
    g_free (name);
    mctest_assert_not_null (output);

    parsed = ftpfs_parse_long_list (me, super->root, input, &err_count);

    /* then */
    for (parsed_iter = parsed, output_iter = output;
         parsed_iter != NULL && output_iter != NULL;
         parsed_iter = g_slist_next (parsed_iter), output_iter = g_slist_next (output_iter))
        mctest_assert_str_eq (VFS_ENTRY (parsed_iter->data)->name, (char *) output_iter->data);

    mctest_assert_null (parsed_iter);
    mctest_assert_null (output_iter);

    for (parsed_iter = parsed, output_iter = output; parsed_iter != NULL;
         parsed_iter = g_slist_next (parsed_iter))
        vfs_s_free_entry (me, VFS_ENTRY (parsed_iter->data));

    g_slist_free (parsed);

    g_slist_free_full (input, g_free);
    g_slist_free_full (output, g_free);
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    /* Add new tests here: *************** */
    mctest_add_parameterized_test (tc_core, test_ftpfs_parse_long_list,
                                   test_ftpfs_parse_long_list_ds);
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
