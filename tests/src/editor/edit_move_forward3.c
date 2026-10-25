/*
   src/editor - tests for edit_move_forward3() function

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

#include "lib/charsets.h"
#include "lib/strutil.h"

#include "src/editor/editwidget.h"

static WGroup owner;
static WEdit *test_edit;

/* --------------------------------------------------------------------------------------------- */

static void
test_init (const char *display, const char *source)
{
    WRect r;

    str_init_strings (display);

    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;
    load_codepages_list ();

    mc_global.utf8_display = str_isutf8 (display);
    mc_global.display_codepage = get_codepage_index (display);
    mc_global.source_codepage = get_codepage_index (source);
    ck_assert_int_ge (mc_global.display_codepage, 0);
    ck_assert_int_ge (mc_global.source_codepage, 0);
    cp_display = get_codepage_id (mc_global.display_codepage);
    g_free (init_translation_table (mc_global.source_codepage, mc_global.display_codepage));

    edit_options.filesize_threshold = (char *) "64M";

    rect_init (&r, 0, 0, 24, 80);
    test_edit = edit_init (NULL, &r, NULL);
    memset (&owner, 0, sizeof (owner));
    group_add_widget (&owner, WIDGET (test_edit));
}

/* --------------------------------------------------------------------------------------------- */

static void
test_deinit (void)
{
    edit_clean (test_edit);
    group_remove_widget (test_edit);
    g_free (test_edit);

    free_codepages_list ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_edit_move_forward3_ds") */
static const struct test_edit_move_forward3_ds
{
    const char *display;
    const char *source;
    const char *text;  // the column of 'X' is calculated
    const long column;
} test_edit_move_forward3_ds[] = {
    { "UTF-8", "UTF-8", "abcX", 3 },
    // wide character takes two columns
    { "UTF-8", "UTF-8", "\xe4\xb8\xadpqX", 4 },
    { "UTF-8", "UTF-8", "\xe4\xb8\xad\tX", 8 },
    // Ticket #4880: non-printable Unicode characters are shown as single '.' on 8-bit display
    { "KOI8-R", "UTF-8", "\xef\xbb\xbfX", 1 },
    { "KOI8-R", "UTF-8", "\xf4\x8f\xbf\xbfX", 1 },
    { "KOI8-R", "UTF-8", "\xd0\x96X", 1 },
    // control character is shown in caret notation
    { "KOI8-R", "CP1251", "\x01X", 2 },
    // 8-bit character is converted to display codepage only once
    { "KOI8-R", "CP1251", "a\xa0X", 2 },
    { "KOI8-R", "CP1251", "\xc0\xc1X", 2 },
};

/* @Test(dataSource = "test_edit_move_forward3_ds") */
START_PARAMETRIZED_TEST (test_edit_move_forward3, test_edit_move_forward3_ds)
{
    // given
    off_t x_pos;
    long column;

    test_init (data->display, data->source);

    for (const char *ti = data->text; *ti != '\0'; ti++)
        edit_buffer_insert (&test_edit->buffer, *ti);
    x_pos = (off_t) (strchr (data->text, 'X') - data->text);

    // when
    column = (long) edit_move_forward3 (test_edit, 0, 0, x_pos);

    // then
    ck_assert_int_eq (column, data->column);

    test_deinit ();
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    // Add new tests here: ***************
    mctest_add_parameterized_test (tc_core, test_edit_move_forward3, test_edit_move_forward3_ds);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
