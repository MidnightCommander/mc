/*
   src/filemanager - tests for copy_file_file() function

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

#include "lib/strutil.h"
#include "lib/util.h"

#include "src/vfs/local/local.c"

#include "src/setup.h"               // verbose
#include "src/filemanager/layout.h"  // nice_rotating_dash
#include "src/filemanager/file.h"
#include "src/filemanager/filegui.h"

/* --------------------------------------------------------------------------------------------- */

/* Big enough to take several chunks of the chunked range copy (which starts at 1 MiB and doubles),
 * and deliberately not a multiple of any block size so that the tail goes through the EOF path. */
#define BIG_SIZE ((3 << 20) + 12345)

static char *test_dir = NULL;
static char *src_path = NULL;
static char *dst_path = NULL;

/* --------------------------------------------------------------------------------------------- */

static char *
make_data (size_t len, guint32 seed)
{
    char *data;
    size_t i;

    data = g_malloc (len);
    for (i = 0; i < len; i++)
    {
        seed = seed * 1103515245U + 12345U;
        data[i] = (char) (seed >> 16);
    }
    return data;
}

/* --------------------------------------------------------------------------------------------- */

static char *
dup_data (const char *data, size_t len)
{
    char *copy;

    copy = g_malloc (len);
    memcpy (copy, data, len);
    return copy;
}

/* --------------------------------------------------------------------------------------------- */

static void
write_file (const char *path, const char *data, size_t len)
{
    GError *error = NULL;

    ck_assert_msg (g_file_set_contents (path, data, (gssize) len, &error), "%s",
                   error != NULL ? error->message : "");
}

/* --------------------------------------------------------------------------------------------- */

static void
assert_file_equals (const char *path, const char *expected, size_t expected_len)
{
    char *actual = NULL;
    gsize actual_len = 0;
    size_t i;

    ck_assert (g_file_get_contents (path, &actual, &actual_len, NULL));
    ck_assert_uint_eq (actual_len, expected_len);
    for (i = 0; i < expected_len && actual[i] == expected[i]; i++)
        ;
    ck_assert_msg (i == expected_len, "destination differs from expected at offset %zu", i);
    g_free (actual);
}

/* --------------------------------------------------------------------------------------------- */

static FileProgressStatus
run_copy (off_t do_reget, gboolean do_append)
{
    file_op_context_t *ctx;
    FileProgressStatus ret;

    ctx = file_op_context_new (OP_COPY);
    ctx->ask_overwrite = FALSE;
    ctx->do_reget = do_reget;
    ctx->do_append = do_append;

    ret = copy_file_file (ctx, src_path, dst_path);

    file_op_context_destroy (ctx);
    return ret;
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

    verbose = FALSE;
    nice_rotating_dash = FALSE;  // no screen to draw on
    mc_global.vfs.preallocate_space = FALSE;

    test_dir = g_dir_make_tmp ("mc-test-copy-XXXXXX", NULL);
    ck_assert_ptr_nonnull (test_dir);
    src_path = g_build_filename (test_dir, "src", (char *) NULL);
    dst_path = g_build_filename (test_dir, "dst", (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    unlink (src_path);
    unlink (dst_path);
    rmdir (test_dir);
    g_free (src_path);
    g_free (dst_path);
    g_free (test_dir);

    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_copy_ds") */
static const struct test_copy_ds
{
    gboolean file_cloning;
    size_t src_len;
    size_t dst_len;  // length of pre-existing destination, 0 if it doesn't exist
    gboolean do_append;
    gboolean reget;  // destination is a prefix of source and the copy resumes after it
} test_copy_ds[] = {
    // plain copy
    { TRUE, 5, 0, FALSE, FALSE },
    { FALSE, 5, 0, FALSE, FALSE },
    { TRUE, BIG_SIZE, 0, FALSE, FALSE },
    { FALSE, BIG_SIZE, 0, FALSE, FALSE },
    // [ Append ]: source must go after the existing destination contents
    { TRUE, 5, 7, TRUE, FALSE },
    { FALSE, 5, 7, TRUE, FALSE },
    { TRUE, BIG_SIZE, 4096, TRUE, FALSE },
    { FALSE, BIG_SIZE, 4096, TRUE, FALSE },
    { TRUE, BIG_SIZE, 4096 + 7, TRUE, FALSE },
    { FALSE, BIG_SIZE, 4096 + 7, TRUE, FALSE },
    // [ Reget ]: resume an interrupted copy
    { TRUE, BIG_SIZE, 4096, TRUE, TRUE },
    { FALSE, BIG_SIZE, 4096, TRUE, TRUE },
    { TRUE, BIG_SIZE, (1 << 20) + 3, TRUE, TRUE },
    { FALSE, BIG_SIZE, (1 << 20) + 3, TRUE, TRUE },
};

/* @Test(dataSource = "test_copy_ds") */
START_PARAMETRIZED_TEST (test_copy, test_copy_ds)
{
    char *src;
    char *dst = NULL;
    char *expected;
    size_t expected_len;
    FileProgressStatus ret;

    // given
    src = make_data (data->src_len, 1);
    write_file (src_path, src, data->src_len);

    if (data->dst_len != 0)
    {
        dst = data->reget ? dup_data (src, data->dst_len) : make_data (data->dst_len, 2);
        write_file (dst_path, dst, data->dst_len);
    }

    if (data->do_append && !data->reget)
    {
        expected_len = data->dst_len + data->src_len;
        expected = g_malloc (expected_len);
        memcpy (expected, dst, data->dst_len);
        memcpy (expected + data->dst_len, src, data->src_len);
    }
    else
    {
        expected_len = data->src_len;
        expected = dup_data (src, expected_len);
    }

    mc_global.vfs.file_cloning = data->file_cloning;

    // when
    ret = run_copy (data->reget ? (off_t) data->dst_len : 0, data->do_append);

    // then
    ck_assert_int_eq (ret, FILE_CONT);
    assert_file_equals (dst_path, expected, expected_len);

    g_free (expected);
    g_free (dst);
    g_free (src);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);
    tcase_set_timeout (tc_core, 60);

    // Add new tests here: ***************
    mctest_add_parameterized_test (tc_core, test_copy, test_copy_ds);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
