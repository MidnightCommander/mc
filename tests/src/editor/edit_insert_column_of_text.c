/*
   src/editor - tests for edit_insert_column_of_text() function

   Copyright (C) 2025
   Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2025

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
#include "src/selcodepage.h"

#include "src/editor/editwidget.h"

static WGroup owner;
static WEdit *test_edit;

// input text
static const char test_text_in[] = "11111\n"  //
                                   "22\n"     //
                                   "aa\n"     //
                                   "bb\n";    //

// result of colunm cipy to the middle of line:
static const char test_insert_column_out1[] = "11111\n"   //
                                              "22\n"      //
                                              "a111a\n"   //
                                              "b2  b\n";  //

// result colunm copy to the end of line, no trailing spaces
static const char test_insert_column_out2[] = "11111\n"  //
                                              "22\n"     //
                                              "aa111\n"  //
                                              "bb2\n";   //

/* --------------------------------------------------------------------------------------------- */

// Function under test
void edit_insert_column_of_text (WEdit *edit, unsigned char *data, off_t size, long width,
                                 off_t *start_pos, off_t *end_pos, long *col1, long *col2);

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

static void
test_load_text (void)
{
    for (const char *ti = test_text_in; *ti != '\0'; ti++)
        edit_buffer_insert (&test_edit->buffer, *ti);
}

/* --------------------------------------------------------------------------------------------- */

static GString *
test_get_block (const off_t start, const off_t finish)
{
    GString *r;

    r = g_string_sized_new (finish - start);

    // copy from buffer, excluding chars that are out of the column 'margins'
    for (off_t i = start; i < finish; i++)
    {
        off_t x;

        x = edit_buffer_get_bol (&test_edit->buffer, i);
        x = edit_move_forward3 (test_edit, x, 0, i);

        const int c = edit_buffer_get_byte (&test_edit->buffer, i);

        if ((x >= test_edit->column1 && x < test_edit->column2)
            || (x >= test_edit->column2 && x < test_edit->column1) || c == '\n')
            g_string_append_c (r, (gchar) c);
    }

    return r;
}

/* --------------------------------------------------------------------------------------------- */

static void
test_insert_column_check (const char *test_out)
{
    GString *actual_text;

    actual_text = g_string_new ("");

    for (off_t i = 0; i < test_edit->buffer.size; i++)
    {
        const int chr = edit_buffer_get_byte (&test_edit->buffer, i);

        g_string_append_c (actual_text, chr);
    }

    mctest_assert_str_eq (actual_text->str, test_out);
    g_string_free (actual_text, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static const struct test_insert_column_ds
{
    off_t offset;
    const char *expected_text;
} test_insert_column_ds[] = {
    {
        // 0. Insert column at middle of line
        10,
        test_insert_column_out1,
    },
    {
        // 1. Insert column at end of line
        11,
        test_insert_column_out2,
    },
};

/* @Test(dataSource = "test_insert_column_ds") */
START_PARAMETRIZED_TEST (test_insert_column, test_insert_column_ds)
{
    off_t start_mark, end_mark;
    GString *copy_buf;
    off_t start_pos, end_pos;
    long col1, col2;

    // given
    test_load_text ();

    test_edit->column_highlight = 1;
    test_edit->mark1 = 1;
    test_edit->mark2 = 8;
    test_edit->column1 = 1;
    test_edit->column2 = 4;

    // when
    edit_cursor_move (test_edit, -test_edit->buffer.curs1 + data->offset);

    const gboolean eval_marks_result = eval_marks (test_edit, &start_mark, &end_mark);

    mctest_assert_true (eval_marks_result);

    copy_buf = test_get_block (start_mark, end_mark);
    edit_push_markers (test_edit);

    const long col_delta = labs (test_edit->column2 - test_edit->column1);

    edit_insert_column_of_text (test_edit, (unsigned char *) copy_buf->str, copy_buf->len,
                                col_delta, &start_pos, &end_pos, &col1, &col2);

    g_string_free (copy_buf, TRUE);

    // then
    test_insert_column_check (data->expected_text);
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
    mctest_add_parameterized_test (tc_core, test_insert_column, test_insert_column_ds);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
