/*
   src/filemanager - tests for the destination of copy and move operations

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Yury V. Zaytsev <yury@shurup.com>, 2026

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

/*
 * The "to:" field of F5/F6 is pre-filled with "dir/name" for a single item and with "dir/" for
 * several items.  A trailing slash means "into this directory".  Without it, the last component
 * of the destination of a single item is the name of the target, even if a directory of that
 * name exists (also "/usr"), except "." and "..".  Several items go into the directory unless the
 * last component is a mask.  Thus Enter on "dir/name" gives the same target as Enter on "dir/".
 * What the copy routines then do with an existing target (overwrite query, error, merge of
 * directories) does not depend on the text.
 *
 * Commit 69b8a0581 (#2276) reverted the first pre-fill, commit 9b5a8dec3, because "dir/name"
 * meant "dir/name/name" when the directory "dir/name" existed.  test_round_trip_existing_dir
 * catches this.
 */

#define TEST_SUITE_NAME "/src/filemanager"

#include "tests/mctest.h"

#include "src/vfs/local/local.c"

#include "src/filemanager/filegui.c"
#include "src/filemanager/file.c"

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    str_init_strings (NULL);

    vfs_init ();
    vfs_init_localfs ();
    vfs_setup_work_dir ();
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_build_default_dest_ds") */
static const struct test_build_default_dest_ds
{
    const char *dir;
    const char *source;  // NULL for several items
    const char *expected;
} test_build_default_dest_ds[] = {
    { "/dst", NULL, "/dst/" },                                              // 0: several items
    { "/dst/", NULL, "/dst/" },                                             // 1
    { "/", NULL, "/" },                                                     // 2
    { "/dst", "file.txt", "/dst/file.txt" },                                // 3: single item
    { "/dst/", "file.txt", "/dst/file.txt" },                               // 4
    { "/", "file.txt", "/file.txt" },                                       // 5
    { "/dst", "sub/deep.txt", "/dst/deep.txt" },                            // 6: panelized panel
    { "/dst", "/src/tree/dir", "/dst/dir" },                                // 7: tree
    { "/arc.tgz/utar://dir", "file.txt", "/arc.tgz/utar://dir/file.txt" },  // 8: VFS
    { "/arc.tgz/utar://", "file.txt", "/arc.tgz/utar://file.txt" },         // 9: VFS root
};

/* @Test(dataSource = "test_build_default_dest_ds") */
START_PARAMETRIZED_TEST (test_build_default_dest, test_build_default_dest_ds)
{
    // given
    char *actual;

    // when
    actual = build_default_dest (data->dir, data->source);

    // then
    mctest_assert_str_eq (actual, data->expected);
    g_free (actual);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_escape_dest_ds") */
static const struct test_escape_dest_ds
{
    const char *def_text;
    gboolean shell_patterns;
    const char *expected;
} test_escape_dest_ds[] = {
    { "/dst/", TRUE, "/dst/" },                     // 0: the slash stays (several items)
    { "/dst/file.txt", TRUE, "/dst/file.txt" },     // 1
    { "/d*r/f?.txt", TRUE, "/d\\*r/f\\?.txt" },     // 2: escaped as a replacement pattern
    { "/dst/a.b(c)", FALSE, "/dst/a\\.b\\(c\\)" },  // 3
};

/* @Test(dataSource = "test_escape_dest_ds") */
START_PARAMETRIZED_TEST (test_escape_dest, test_escape_dest_ds)
{
    // given
    char *actual;

    // when
    actual = filegui__escape_dest (data->def_text, data->shell_patterns);

    // then
    mctest_assert_str_eq (actual, data->expected);
    g_free (actual);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_split_dest_ds") */
static const struct test_split_dest_ds
{
    const char *dest;
    gboolean only_one;
    const char *expected_dir;
    const char *expected_mask;
} test_split_dest_ds[] = {
    { "/dst/", TRUE, "/dst/", "\\0" },          // 0: a trailing slash means "into"
    { "/dst/name", TRUE, "/dst/", "name" },     // 1: else the name of the target
    { "/usr", TRUE, "/", "usr" },               // 2: also if it is an existing directory
    { "name", TRUE, "", "name" },               // 3: in the current directory (Shift-F5/F6)
    { "..", TRUE, "..", "\\0" },                // 4: but "." and ".." mean "into"
    { "/dst/.", TRUE, "/dst/.", "\\0" },        // 5
    { "/dst/*.bak", TRUE, "/dst/", "*.bak" },   // 6: a mask
    { "/dst/", FALSE, "/dst/", "\\0" },         // 7: several items
    { "/dst/box", FALSE, "/dst/box", "\\0" },   // 8: always into a directory without a mask
    { "/dst/*.bak", FALSE, "/dst/", "*.bak" },  // 9: a mask
};

/* @Test(dataSource = "test_split_dest_ds") */
START_PARAMETRIZED_TEST (test_split_dest, test_split_dest_ds)
{
    // given
    char *dest, *mask;

    dest = g_strdup (data->dest);

    // when
    mask = filegui__split_dest (dest, data->only_one);

    // then
    mctest_assert_str_eq (dest, data->expected_dir);
    mctest_assert_str_eq (mask, data->expected_mask);
    g_free (mask);
    g_free (dest);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

/* target of a single item named src_name for the default text def_text and Enter */
static char *
target_of (const char *def_text, const char *src_name, gboolean shell_patterns)
{
    file_op_context_t *ctx;
    FileProgressStatus status;
    char *text, *dest, *src, *target;

    // as file_mask_dialog() does
    text = filegui__escape_dest (def_text, shell_patterns);
    dest = tilde_expand (text);
    g_free (text);

    ctx = file_op_context_new (OP_COPY);
    ctx->dest_mask = filegui__split_dest (dest, TRUE);
    if (*dest == '\0')
    {
        g_free (dest);
        dest = g_strdup ("./");
    }
    ctx->search_handle = mc_search_new (shell_patterns ? "*" : "^(.*)$", NULL);
    ctx->search_handle->is_case_sensitive = TRUE;
    ctx->search_handle->search_type = shell_patterns ? MC_SEARCH_T_GLOB : MC_SEARCH_T_REGEX;

    // as operate_single_file() does
    src = g_strconcat ("/src/", src_name, (char *) NULL);
    target = build_dest (ctx, src, dest, &status);
    ck_assert_int_eq (status, FILE_CONT);

    g_free (src);
    g_free (dest);
    g_free (ctx->dest_mask);
    file_op_context_destroy (ctx);

    return target;
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_round_trip_ds") */
static const struct test_round_trip_ds
{
    const char *dir;
    const char *name;
} test_round_trip_ds[] = {
    { "/dst", "file.txt" },          // 0
    { "/dst/", "file.txt" },         // 1
    { "/", "file.txt" },             // 2
    { "/d*r$", "file.txt" },         // 3
    { "/dst", "st*r?.txt" },         // 4
    { "/dst", "b\\s.txt" },          // 5
    { "/dst", "x\\1.txt" },          // 6
    { "/dst", "d$x.txt" },           // 7
    { "/dst", "a.b(c)+[1]^&.txt" },  // 8
    { "/dst", "sp ace.txt" },        // 9
    { "/dst", "{br}.txt" },          // 10
    { "/dst", "~tilde" },            // 11
};

/* @Test(dataSource = "test_round_trip_ds") */
START_PARAMETRIZED_TEST (test_round_trip, test_round_trip_ds)
{
    int shell_patterns;

    for (shell_patterns = 0; shell_patterns <= 1; shell_patterns++)
    {
        // given
        char *expected, *dir_only, *with_name, *def_text;

        expected = mc_build_filename (data->dir, data->name, (char *) NULL);

        // when
        def_text = build_default_dest (data->dir, NULL);
        dir_only = target_of (def_text, data->name, shell_patterns != 0);
        g_free (def_text);

        def_text = build_default_dest (data->dir, data->name);
        with_name = target_of (def_text, data->name, shell_patterns != 0);
        g_free (def_text);

        // then: Enter on "dir/name" and on "dir/" give the same target
        mctest_assert_str_eq (dir_only, expected);
        mctest_assert_str_eq (with_name, expected);

        g_free (with_name);
        g_free (dir_only);
        g_free (expected);
    }
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

/* #2276: the target already has a directory with the name of the source */
START_TEST (test_round_trip_existing_dir)
{
    char *dir, *subdir, *def_text, *target;

    dir = g_dir_make_tmp ("mctest-XXXXXX", NULL);
    mctest_assert_not_null (dir);
    subdir = g_build_filename (dir, "proj", (char *) NULL);
    ck_assert_int_eq (mkdir (subdir, 0700), 0);

    // Enter on "dir/proj" names "dir/proj" (merge or error), not "dir/proj/proj" (nesting)
    def_text = build_default_dest (dir, "proj");
    target = target_of (def_text, "proj", TRUE);
    mctest_assert_str_eq (target, subdir);
    g_free (target);
    g_free (def_text);

    // as Enter on "dir/" does
    def_text = build_default_dest (dir, NULL);
    target = target_of (def_text, "proj", TRUE);
    mctest_assert_str_eq (target, subdir);
    g_free (target);
    g_free (def_text);

    rmdir (subdir);
    rmdir (dir);
    g_free (subdir);
    g_free (dir);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* the only difference: "\0" changes a newline in the name to a space, the pre-fill keeps it */
START_TEST (test_round_trip_newline)
{
    char *def_text, *target;

    def_text = build_default_dest ("/dst", NULL);
    target = target_of (def_text, "nl\nx.txt", TRUE);
    mctest_assert_str_eq (target, "/dst/nl x.txt");
    g_free (target);
    g_free (def_text);

    def_text = build_default_dest ("/dst", "nl\nx.txt");
    target = target_of (def_text, "nl\nx.txt", TRUE);
    mctest_assert_str_eq (target, "/dst/nl\nx.txt");
    g_free (target);
    g_free (def_text);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    // Add new tests here: ***************
    mctest_add_parameterized_test (tc_core, test_build_default_dest, test_build_default_dest_ds);
    mctest_add_parameterized_test (tc_core, test_escape_dest, test_escape_dest_ds);
    mctest_add_parameterized_test (tc_core, test_split_dest, test_split_dest_ds);
    mctest_add_parameterized_test (tc_core, test_round_trip, test_round_trip_ds);
    tcase_add_test (tc_core, test_round_trip_existing_dir);
    tcase_add_test (tc_core, test_round_trip_newline);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
