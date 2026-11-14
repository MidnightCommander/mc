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

#include <errno.h>

#include "src/vfs/local/local.c"

#include "src/filemanager/file.c"

/* --------------------------------------------------------------------------------------------- */

/* Give up injecting read errors after this many calls, so that a regression makes the test
   fail instead of hanging forever */
#define READ_ERRORS_MAX 1000

static const char test_content[] = "Midnight Commander copy_file_file() test data\n";

static char *test_dir = NULL;
static char *src_path = NULL;
static char *dst_path = NULL;

static gboolean read_fails = FALSE;
static int read_calls = 0;
static int query_dialog_calls = 0;

/* --------------------------------------------------------------------------------------------- */

/* @Mock */
void
mc_refresh (void)
{
}

/* --------------------------------------------------------------------------------------------- */

/* @Mock */
void
message (int flags, const char *title, const char *text, ...)
{
    (void) flags;
    (void) title;
    (void) text;
}

/* --------------------------------------------------------------------------------------------- */

/* @Mock */
int
query_dialog (const char *header, const char *text, int flags, int count, ...)
{
    (void) header;
    (void) text;
    (void) flags;
    (void) count;

    query_dialog_calls++;

    // "Incomplete file was retrieved": Delete
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

/* Fault injection at the VFS layer: fail every read of the local file system with EIO, like
   reading a damaged sector does */
static ssize_t
test_local_read (void *data, char *buffer, size_t count)
{
    read_calls++;

    if (read_fails && read_calls <= READ_ERRORS_MAX)
    {
        errno = EIO;
        return -1;
    }

    if (read_fails)
        return 0;

    return local_read (data, buffer, count);
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

    vfs_local_ops->read = test_local_read;

    // no UI in tests
    nice_rotating_dash = FALSE;
    verbose = FALSE;
    // force the read()/write() loop instead of reflinking the file
    mc_global.vfs.file_cloning = FALSE;
    mc_global.vfs.preallocate_space = FALSE;

    test_dir = g_dir_make_tmp ("mc-test-copy_file_file-XXXXXX", NULL);
    ck_assert_ptr_nonnull (test_dir);
    src_path = g_build_filename (test_dir, "src", (char *) NULL);
    dst_path = g_build_filename (test_dir, "dst", (char *) NULL);
    ck_assert (g_file_set_contents (src_path, test_content, -1, NULL));

    read_fails = FALSE;
    read_calls = 0;
    query_dialog_calls = 0;
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    (void) unlink (dst_path);
    (void) unlink (src_path);
    (void) rmdir (test_dir);
    g_free (dst_path);
    g_free (src_path);
    g_free (test_dir);

    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_copy_file_file_ok)
{
    // given
    file_op_context_t *ctx;
    FileProgressStatus status;
    char *dst_content = NULL;

    ctx = file_op_context_new (OP_COPY);

    // when
    status = copy_file_file (ctx, src_path, dst_path);

    // then
    ck_assert_int_eq (status, FILE_CONT);
    ck_assert (g_file_get_contents (dst_path, &dst_content, NULL, NULL));
    mctest_assert_str_eq (dst_content, test_content);
    ck_assert_int_eq (query_dialog_calls, 0);

    g_free (dst_content);
    file_op_context_destroy (ctx);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* Ticket #4631: after "Ignore all", a read error must skip the file instead of retrying the
   failing read forever */

/* @Test */
START_TEST (test_copy_file_file_read_error_ignore_all)
{
    // given
    file_op_context_t *ctx;
    FileProgressStatus status;

    ctx = file_op_context_new (OP_COPY);
    ctx->ignore_all = TRUE;
    read_fails = TRUE;

    // when
    status = copy_file_file (ctx, src_path, dst_path);

    // then
    ck_assert_int_eq (read_calls, 1);
    ck_assert_int_eq (status, FILE_IGNORE_ALL);
    ck_assert (ctx->ignore_all);
    // the incomplete target file is deleted
    ck_assert (!g_file_test (dst_path, G_FILE_TEST_EXISTS));

    file_op_context_destroy (ctx);
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
    tcase_add_test (tc_core, test_copy_file_file_ok);
    tcase_add_test (tc_core, test_copy_file_file_read_error_ignore_all);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
