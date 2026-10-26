/*
   src/editor - tests for edit_write_stream() function

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

#define TEST_SUITE_NAME "/src/editor"

#include "tests/mctest.h"

#include <stdio.h>

#include "lib/charsets.h"
#include "src/selcodepage.h"

// edit_write_stream() is static
#include "src/editor/editcmd.c"

static WGroup owner;
static WEdit *test_edit;

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    WRect r;

    str_init_strings (NULL);

    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;
    load_codepages_list ();

    edit_options.filesize_threshold = (char *) "64M";

    rect_init (&r, 0, 0, 24, 80);
    test_edit = edit_init (NULL, &r, NULL);
    memset (&owner, 0, sizeof (owner));
    group_add_widget (&owner, WIDGET (test_edit));

    mc_global.source_codepage = 0;
    mc_global.display_codepage = 0;
    cp_source = "ASCII";
    cp_display = "ASCII";

    do_set_codepage (0);
    edit_set_codeset (test_edit);
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    edit_clean (test_edit);
    group_remove_widget (test_edit);
    g_free (test_edit);

    free_codepages_list ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_edit_write_stream_ds") */
static const struct test_edit_write_stream_ds
{
    const char *input;
    LineBreaks lb;
    const char *expected;
} test_edit_write_stream_ds[] = {
    // 0-2. LF
    { "a\nb\n", LB_UNIX, "a\nb\n" },
    { "a\nb\n", LB_WIN, "a\r\nb\r\n" },
    { "a\nb\n", LB_MAC, "a\rb\r" },
    // 3-5. CR LF
    { "a\r\nb\r\n", LB_UNIX, "a\nb\n" },
    { "a\r\nb\r\n", LB_WIN, "a\r\nb\r\n" },
    { "a\r\nb\r\n", LB_MAC, "a\rb\r" },
    // 6-8. CR
    { "a\rb\r", LB_UNIX, "a\nb\n" },
    { "a\rb\r", LB_WIN, "a\r\nb\r\n" },
    { "a\rb\r", LB_MAC, "a\rb\r" },
    // 9-11. no line break at the end
    { "a\nb", LB_UNIX, "a\nb" },
    { "a\nb", LB_WIN, "a\r\nb" },
    { "a\nb", LB_MAC, "a\rb" },
    // 12-14. mixed and empty lines: LF, CR LF, CR CR, LF LF
    { "x\n\r\ny\r\rz\n\n", LB_UNIX, "x\n\ny\n\nz\n\n" },
    { "x\n\r\ny\r\rz\n\n", LB_WIN, "x\r\n\r\ny\r\n\r\nz\r\n\r\n" },
    { "x\n\r\ny\r\rz\n\n", LB_MAC, "x\r\ry\r\rz\r\r" },
    // 15. CR at the very end
    { "a\r", LB_UNIX, "a\n" },
};

/* @Test(dataSource = "test_edit_write_stream_ds") */
START_PARAMETRIZED_TEST (test_edit_write_stream, test_edit_write_stream_ds)
{
    FILE *f;
    off_t len;
    char actual[64];
    size_t actual_len;

    // given
    for (const char *ti = data->input; *ti != '\0'; ti++)
        edit_buffer_insert (&test_edit->buffer, *ti);
    test_edit->lb = data->lb;

    f = tmpfile ();
    mctest_assert_not_null (f);

    // when
    len = edit_write_stream (test_edit, f);

    // then
    mctest_assert_true (len == test_edit->buffer.size);

    rewind (f);
    actual_len = fread (actual, 1, sizeof (actual) - 1, f);
    actual[actual_len] = '\0';
    fclose (f);

    mctest_assert_str_eq (actual, data->expected);
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
    mctest_add_parameterized_test (tc_core, test_edit_write_stream, test_edit_write_stream_ds);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
