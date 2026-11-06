/*
   src/filemanager - tests for cd_expand_wildcards() function

   Copyright (C) 2026
   Free Software Foundation, Inc.

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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define TEST_SUITE_NAME "/src/filemanager"

#include "tests/mctest.h"

#include <unistd.h>

#include "src/vfs/local/local.c"

#include "src/filemanager/cd.c"

/* --------------------------------------------------------------------------------------------- */

/* @Mock */
panel_view_mode_t
get_current_type (void)
{
    return view_listing;
}

/* @Mock */
gboolean
panel_cd (WPanel *panel, const vfs_path_t *new_dir_vpath, enum cd_enum cd_type)
{
    (void) panel;
    (void) new_dir_vpath;
    (void) cd_type;

    return TRUE;
}

/* @Mock */
void
sync_tree (const vfs_path_t *vpath)
{
    (void) vpath;
}

/* --------------------------------------------------------------------------------------------- */

/* directories of the test tree, parents first */
static const char *const test_dirs[] = {
    "alpha",    "alphabet", "beta",       "Zeta",       ".hidden",   "with space",
    "star*dir", "sub",      "sub/inner1", "sub/inner2", "br[ack]et",
};

/* files and symlinks of the test tree: { name, symlink target or NULL for a file } */
static const char *const test_files[][2] = {
    { "alfile", NULL },
    { "lnk", "beta" },
    { "badlnk", "nowhere" },
};

static char *test_root;
static WPanel test_panel;

/* --------------------------------------------------------------------------------------------- */

static char *
test_path (const char *name)
{
    return g_build_filename (test_root, name, (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    size_t i;

    str_init_strings (NULL);

    vfs_init ();
    vfs_init_localfs ();
    vfs_setup_work_dir ();

    test_root = g_dir_make_tmp ("mctest-XXXXXX", NULL);
    mctest_assert_not_null (test_root);

    for (i = 0; i < G_N_ELEMENTS (test_dirs); i++)
    {
        char *path;

        path = test_path (test_dirs[i]);
        ck_assert_int_eq (mkdir (path, 0700), 0);
        g_free (path);
    }

    for (i = 0; i < G_N_ELEMENTS (test_files); i++)
    {
        char *path;

        path = test_path (test_files[i][0]);
        if (test_files[i][1] == NULL)
            ck_assert (g_file_set_contents (path, "", 0, NULL));
        else
            ck_assert_int_eq (symlink (test_files[i][1], path), 0);
        g_free (path);
    }

    // relative paths are relative to the current directory of the panel
    test_panel.cwd_vpath = vfs_path_from_str (test_root);
    current_panel = &test_panel;
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    size_t i;

    current_panel = NULL;
    vfs_path_free (test_panel.cwd_vpath, TRUE);

    for (i = 0; i < G_N_ELEMENTS (test_files); i++)
    {
        char *path;

        path = test_path (test_files[i][0]);
        unlink (path);
        g_free (path);
    }

    for (i = G_N_ELEMENTS (test_dirs); i > 0; i--)
    {
        char *path;

        path = test_path (test_dirs[i - 1]);
        rmdir (path);
        g_free (path);
    }

    rmdir (test_root);
    g_free (test_root);

    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_cd_expand_wildcards_ds") */
static const struct test_cd_expand_wildcards_ds
{
    const char *input;
    const char *expected;  // NULL: the path can't be expanded to a directory
} test_cd_expand_wildcards_ds[] = {
    // only directories match, not files
    { "al*", "alpha" },
    { "alphabe?", "alphabet" },
    // strcmp() order: uppercase first
    { "*eta", "Zeta" },
    // symlinks to directories match, broken ones don't
    { "l?k", "lnk" },
    { "bad*", NULL },
    // wildcards don't match a leading '.'
    { ".h*", ".hidden" },
    { "*hidden", NULL },
    { "with*", "with space" },
    { "sub/inn*", "sub/inner1" },
    // backtracking: "star*dir" and "sub" match "s*", but only "sub" has a matching subdirectory
    { "s*/i*2", "sub/inner2" },
    { "*/inner2", "sub/inner2" },
    { "s*/i*2/", "sub/inner2/" },
    // wildcards in the name of a found directory aren't expanded again
    { "st*", "star*dir" },
    { "b[r]*", "br[ack]et" },
    { "nomatch*", NULL },
    { "al*/x", NULL },
    { "alfile/*", NULL },
};

/* @Test(dataSource = "test_cd_expand_wildcards_ds") */
START_PARAMETRIZED_TEST (test_cd_expand_wildcards, test_cd_expand_wildcards_ds)
{
    char *actual;

    // when: relative to the current directory of the panel
    actual = cd_expand_wildcards (data->input, 0);

    // then
    mctest_assert_str_eq (actual, data->expected);
    g_free (actual);

    // when: absolute path; wildcards in the already expanded part are skipped
    {
        char *input, *expected;

        input = test_path (data->input);
        expected = data->expected == NULL ? NULL : test_path (data->expected);

        actual = cd_expand_wildcards (input, strlen (test_root));

        // then
        mctest_assert_str_eq (actual, expected);
        g_free (actual);
        g_free (expected);
        g_free (input);
    }
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    // Add new tests here: ***************
    mctest_add_parameterized_test (tc_core, test_cd_expand_wildcards, test_cd_expand_wildcards_ds);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
