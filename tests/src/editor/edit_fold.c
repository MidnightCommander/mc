/*
   src/editor - tests for edit_fold_*() functions

   Copyright (C) 2025
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

#define TEST_SUITE_NAME "/src/editor"

#include "tests/mctest.h"

#include "lib/charsets.h"
#include "src/selcodepage.h"

#include "src/editor/editwidget.h"
#include "src/editor/edit-impl.h"

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
    edit_fold_flush (test_edit);
    edit_clean (test_edit);
    group_remove_widget (test_edit);
    g_free (test_edit);

    free_codepages_list ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */
/* edit_fold_make + edit_fold_find */
/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_make_and_find)
{
    edit_fold_t *f;

    // given
    edit_fold_make (test_edit, 10, 5);

    // when
    f = edit_fold_find (test_edit, 10);

    // then
    mctest_assert_not_null (f);
    ck_assert_int_eq (f->line_start, 10);
    ck_assert_int_eq (f->line_count, 5);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_make_zero_count_is_noop)
{
    // given
    edit_fold_make (test_edit, 10, 0);

    // then
    mctest_assert_null (edit_fold_find (test_edit, 10));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_make_negative_count_is_noop)
{
    // given
    edit_fold_make (test_edit, 10, -1);

    // then
    mctest_assert_null (edit_fold_find (test_edit, 10));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_make_two_non_overlapping)
{
    edit_fold_t *f1, *f2;

    // given
    edit_fold_make (test_edit, 10, 3);
    edit_fold_make (test_edit, 20, 4);

    // when
    f1 = edit_fold_find (test_edit, 10);
    f2 = edit_fold_find (test_edit, 20);

    // then
    mctest_assert_not_null (f1);
    ck_assert_int_eq (f1->line_start, 10);
    ck_assert_int_eq (f1->line_count, 3);

    mctest_assert_not_null (f2);
    ck_assert_int_eq (f2->line_start, 20);
    ck_assert_int_eq (f2->line_count, 4);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_make_overlapping_replaces)
{
    edit_fold_t *f;

    // given: create a fold at line 10 with 5 lines
    edit_fold_make (test_edit, 10, 5);

    // when: create overlapping fold that spans lines 8-18
    edit_fold_make (test_edit, 8, 10);

    // then: old fold removed, new fold present
    f = edit_fold_find (test_edit, 8);
    mctest_assert_not_null (f);
    ck_assert_int_eq (f->line_start, 8);
    ck_assert_int_eq (f->line_count, 10);

    // original fold at line 10 should now be part of the new fold, not a separate one
    f = edit_fold_find (test_edit, 10);
    mctest_assert_not_null (f);
    ck_assert_int_eq (f->line_start, 8);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_find_empty_list)
{
    // then
    mctest_assert_null (edit_fold_find (test_edit, 10));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_find_within_range)
{
    edit_fold_t *f;

    // given: fold at line 10 hiding 5 lines (10-15)
    edit_fold_make (test_edit, 10, 5);

    // when: search for line inside fold
    f = edit_fold_find (test_edit, 13);

    // then
    mctest_assert_not_null (f);
    ck_assert_int_eq (f->line_start, 10);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_find_outside_range)
{
    // given
    edit_fold_make (test_edit, 10, 5);

    // then: line before fold
    mctest_assert_null (edit_fold_find (test_edit, 9));
    // line after fold
    mctest_assert_null (edit_fold_find (test_edit, 16));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* edit_fold_is_hidden */
/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_is_hidden_before_fold)
{
    // given: fold at line 10 hiding 5 lines
    edit_fold_make (test_edit, 10, 5);

    // then
    mctest_assert_false (edit_fold_is_hidden (test_edit, 9));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_is_hidden_fold_start)
{
    // given
    edit_fold_make (test_edit, 10, 5);

    // then: fold start line is NOT hidden (it's the visible summary line)
    mctest_assert_false (edit_fold_is_hidden (test_edit, 10));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_is_hidden_inside_fold)
{
    // given
    edit_fold_make (test_edit, 10, 5);

    // then: lines 11-15 are hidden
    mctest_assert_true (edit_fold_is_hidden (test_edit, 11));
    mctest_assert_true (edit_fold_is_hidden (test_edit, 13));
    mctest_assert_true (edit_fold_is_hidden (test_edit, 15));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_is_hidden_after_fold)
{
    // given
    edit_fold_make (test_edit, 10, 5);

    // then
    mctest_assert_false (edit_fold_is_hidden (test_edit, 16));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* edit_fold_remove */
/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_remove_existing)
{
    // given
    edit_fold_make (test_edit, 10, 5);

    // when
    gboolean result = edit_fold_remove (test_edit, 10);

    // then
    mctest_assert_true (result);
    mctest_assert_null (edit_fold_find (test_edit, 10));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_remove_empty_list)
{
    // when
    gboolean result = edit_fold_remove (test_edit, 10);

    // then
    mctest_assert_false (result);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_remove_then_find_returns_null)
{
    // given
    edit_fold_make (test_edit, 10, 5);
    edit_fold_make (test_edit, 20, 3);

    // when: remove first fold
    edit_fold_remove (test_edit, 10);

    // then: first fold gone, second still present
    mctest_assert_null (edit_fold_find (test_edit, 10));
    mctest_assert_not_null (edit_fold_find (test_edit, 20));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* edit_fold_next_visible / edit_fold_prev_visible */
/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_next_visible_no_folds)
{
    // then
    ck_assert_int_eq (edit_fold_next_visible (test_edit, 10), 11);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_next_visible_at_fold_start)
{
    // given: fold at line 10 hiding 5 lines
    edit_fold_make (test_edit, 10, 5);

    // then: from fold start, skip to line_start + line_count + 1
    ck_assert_int_eq (edit_fold_next_visible (test_edit, 10), 16);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_next_visible_before_fold)
{
    // given
    edit_fold_make (test_edit, 10, 5);

    // then: line before fold, normal increment
    ck_assert_int_eq (edit_fold_next_visible (test_edit, 9), 10);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_prev_visible_no_folds)
{
    // then
    ck_assert_int_eq (edit_fold_prev_visible (test_edit, 10), 9);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_prev_visible_after_fold)
{
    // given: fold at line 10 hiding 5 lines (lines 11-15 hidden)
    edit_fold_make (test_edit, 10, 5);

    // then: prev_visible(16) checks fold_find(15). Line 15 is in fold range [10,15].
    // Since 15 > line_start(10), it jumps back to fold start.
    ck_assert_int_eq (edit_fold_prev_visible (test_edit, 16), 10);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_prev_visible_at_fold_start)
{
    // given
    edit_fold_make (test_edit, 10, 5);

    // then: prev_visible(10) checks fold_find(9), which returns NULL, so returns 9
    ck_assert_int_eq (edit_fold_prev_visible (test_edit, 10), 9);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_prev_visible_at_zero)
{
    // then: should not go below 0
    ck_assert_int_eq (edit_fold_prev_visible (test_edit, 0), 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* edit_fold_flush */
/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_flush)
{
    // given
    edit_fold_make (test_edit, 10, 5);
    edit_fold_make (test_edit, 20, 3);
    edit_fold_make (test_edit, 30, 7);

    // when
    edit_fold_flush (test_edit);

    // then: all folds removed
    mctest_assert_null (test_edit->folds);
    mctest_assert_null (edit_fold_find (test_edit, 10));
    mctest_assert_null (edit_fold_find (test_edit, 20));
    mctest_assert_null (edit_fold_find (test_edit, 30));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* edit_fold_inc / edit_fold_dec */
/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_inc_before_fold)
{
    edit_fold_t *f;

    // given: fold at line 10 with 5 hidden lines
    edit_fold_make (test_edit, 10, 5);

    // when: insert a line before the fold (at line 5)
    edit_fold_inc (test_edit, 5);

    // then: fold shifts down by 1
    f = edit_fold_find (test_edit, 11);
    mctest_assert_not_null (f);
    ck_assert_int_eq (f->line_start, 11);
    ck_assert_int_eq (f->line_count, 5);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_inc_inside_fold)
{
    edit_fold_t *f;

    // given: fold at line 10 with 5 hidden lines
    edit_fold_make (test_edit, 10, 5);

    // when: insert a line inside the fold (at line 12)
    edit_fold_inc (test_edit, 12);

    // then: fold grows by 1
    f = edit_fold_find (test_edit, 10);
    mctest_assert_not_null (f);
    ck_assert_int_eq (f->line_start, 10);
    ck_assert_int_eq (f->line_count, 6);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_dec_before_fold)
{
    edit_fold_t *f;

    // given: fold at line 10 with 5 hidden lines
    edit_fold_make (test_edit, 10, 5);

    // when: delete a line before the fold (at line 5)
    edit_fold_dec (test_edit, 5);

    // then: fold shifts up by 1
    f = edit_fold_find (test_edit, 9);
    mctest_assert_not_null (f);
    ck_assert_int_eq (f->line_start, 9);
    ck_assert_int_eq (f->line_count, 5);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_dec_inside_fold)
{
    edit_fold_t *f;

    // given: fold at line 10 with 5 hidden lines
    edit_fold_make (test_edit, 10, 5);

    // when: delete a line inside the fold (at line 12)
    edit_fold_dec (test_edit, 12);

    // then: fold shrinks by 1
    f = edit_fold_find (test_edit, 10);
    mctest_assert_not_null (f);
    ck_assert_int_eq (f->line_start, 10);
    ck_assert_int_eq (f->line_count, 4);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_dec_removes_collapsed_fold)
{
    // given: fold at line 10 with 1 hidden line
    edit_fold_make (test_edit, 10, 1);

    // when: delete the only hidden line
    edit_fold_dec (test_edit, 11);

    // then: fold is removed entirely
    mctest_assert_null (edit_fold_find (test_edit, 10));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* sorted insertion order */
/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_make_maintains_sorted_order)
{
    edit_fold_t *f;

    // given: create folds out of order
    edit_fold_make (test_edit, 30, 2);
    edit_fold_make (test_edit, 10, 2);
    edit_fold_make (test_edit, 20, 2);

    // then: linked list should be in sorted order
    f = test_edit->folds;
    mctest_assert_not_null (f);
    ck_assert_int_eq (f->line_start, 10);

    f = f->next;
    mctest_assert_not_null (f);
    ck_assert_int_eq (f->line_start, 20);

    f = f->next;
    mctest_assert_not_null (f);
    ck_assert_int_eq (f->line_start, 30);

    mctest_assert_null (f->next);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* edit_fold_indicator_width */
/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_indicator_width_returns_4)
{
    edit_fold_t f;

    // given: any fold
    memset (&f, 0, sizeof (f));
    f.line_start = 10;
    f.line_count = 5;

    // then: indicator "...}" is always 4 columns
    ck_assert_int_eq (edit_fold_indicator_width (&f), 4);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_indicator_width_independent_of_line_count)
{
    edit_fold_t f1, f2;

    // given: folds with different line counts
    memset (&f1, 0, sizeof (f1));
    f1.line_start = 0;
    f1.line_count = 1;

    memset (&f2, 0, sizeof (f2));
    f2.line_start = 0;
    f2.line_count = 99999;

    // then: same width regardless of line count
    ck_assert_int_eq (edit_fold_indicator_width (&f1), edit_fold_indicator_width (&f2));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* edit_fold_remove from inside fold */
/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_remove_from_inside)
{
    // given: fold at line 10 hiding 5 lines
    edit_fold_make (test_edit, 10, 5);

    // when: remove by a line inside the fold
    gboolean result = edit_fold_remove (test_edit, 13);

    // then: fold is removed
    mctest_assert_true (result);
    mctest_assert_null (edit_fold_find (test_edit, 10));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* edit_fold_inc / edit_fold_dec edge cases */
/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_inc_at_fold_start)
{
    edit_fold_t *f;

    // given: fold at line 10 with 5 hidden lines
    edit_fold_make (test_edit, 10, 5);

    // when: insert at the fold start line itself
    edit_fold_inc (test_edit, 10);

    // then: insertion at fold start line — fold is unchanged
    // (line == line_start is neither "after" nor "inside")
    f = edit_fold_find (test_edit, 10);
    mctest_assert_not_null (f);
    ck_assert_int_eq (f->line_start, 10);
    ck_assert_int_eq (f->line_count, 5);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_dec_at_fold_start)
{
    edit_fold_t *f;

    // given: fold at line 10 with 5 hidden lines
    edit_fold_make (test_edit, 10, 5);

    // when: delete at the fold start line itself
    edit_fold_dec (test_edit, 10);

    // then: fold start stays, no change (deletion is AT start, not inside)
    f = edit_fold_find (test_edit, 10);
    mctest_assert_not_null (f);
    ck_assert_int_eq (f->line_start, 10);
    ck_assert_int_eq (f->line_count, 5);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_inc_after_fold)
{
    edit_fold_t *f;

    // given: fold at line 10 with 5 hidden lines (range 10-15)
    edit_fold_make (test_edit, 10, 5);

    // when: insert after the fold (at line 20)
    edit_fold_inc (test_edit, 20);

    // then: fold unchanged
    f = edit_fold_find (test_edit, 10);
    mctest_assert_not_null (f);
    ck_assert_int_eq (f->line_start, 10);
    ck_assert_int_eq (f->line_count, 5);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_fold_dec_after_fold)
{
    edit_fold_t *f;

    // given: fold at line 10 with 5 hidden lines
    edit_fold_make (test_edit, 10, 5);

    // when: delete after the fold (at line 20)
    edit_fold_dec (test_edit, 20);

    // then: fold unchanged
    f = edit_fold_find (test_edit, 10);
    mctest_assert_not_null (f);
    ck_assert_int_eq (f->line_start, 10);
    ck_assert_int_eq (f->line_count, 5);
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
    // edit_fold_make + edit_fold_find
    tcase_add_test (tc_core, test_fold_make_and_find);
    tcase_add_test (tc_core, test_fold_make_zero_count_is_noop);
    tcase_add_test (tc_core, test_fold_make_negative_count_is_noop);
    tcase_add_test (tc_core, test_fold_make_two_non_overlapping);
    tcase_add_test (tc_core, test_fold_make_overlapping_replaces);
    tcase_add_test (tc_core, test_fold_find_empty_list);
    tcase_add_test (tc_core, test_fold_find_within_range);
    tcase_add_test (tc_core, test_fold_find_outside_range);
    // edit_fold_is_hidden
    tcase_add_test (tc_core, test_fold_is_hidden_before_fold);
    tcase_add_test (tc_core, test_fold_is_hidden_fold_start);
    tcase_add_test (tc_core, test_fold_is_hidden_inside_fold);
    tcase_add_test (tc_core, test_fold_is_hidden_after_fold);
    // edit_fold_remove
    tcase_add_test (tc_core, test_fold_remove_existing);
    tcase_add_test (tc_core, test_fold_remove_empty_list);
    tcase_add_test (tc_core, test_fold_remove_then_find_returns_null);
    // edit_fold_next_visible / edit_fold_prev_visible
    tcase_add_test (tc_core, test_fold_next_visible_no_folds);
    tcase_add_test (tc_core, test_fold_next_visible_at_fold_start);
    tcase_add_test (tc_core, test_fold_next_visible_before_fold);
    tcase_add_test (tc_core, test_fold_prev_visible_no_folds);
    tcase_add_test (tc_core, test_fold_prev_visible_after_fold);
    tcase_add_test (tc_core, test_fold_prev_visible_at_fold_start);
    tcase_add_test (tc_core, test_fold_prev_visible_at_zero);
    // edit_fold_flush
    tcase_add_test (tc_core, test_fold_flush);
    // edit_fold_inc / edit_fold_dec
    tcase_add_test (tc_core, test_fold_inc_before_fold);
    tcase_add_test (tc_core, test_fold_inc_inside_fold);
    tcase_add_test (tc_core, test_fold_dec_before_fold);
    tcase_add_test (tc_core, test_fold_dec_inside_fold);
    tcase_add_test (tc_core, test_fold_dec_removes_collapsed_fold);
    // sorted order
    tcase_add_test (tc_core, test_fold_make_maintains_sorted_order);
    // edit_fold_indicator_width
    tcase_add_test (tc_core, test_fold_indicator_width_returns_4);
    tcase_add_test (tc_core, test_fold_indicator_width_independent_of_line_count);
    // edit_fold_remove from inside
    tcase_add_test (tc_core, test_fold_remove_from_inside);
    // edit_fold_inc / edit_fold_dec edge cases
    tcase_add_test (tc_core, test_fold_inc_at_fold_start);
    tcase_add_test (tc_core, test_fold_dec_at_fold_start);
    tcase_add_test (tc_core, test_fold_inc_after_fold);
    tcase_add_test (tc_core, test_fold_dec_after_fold);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
