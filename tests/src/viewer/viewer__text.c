/*
   src/viewer - tests for parsing of text and nroff sequences in the viewer

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

#define TEST_SUITE_NAME "/src/viewer"

#include "tests/mctest.h"

#include "lib/charsets.h"
#include "lib/strutil.h"

#include "src/setup.h"  // option_tab_spacing

// static functions of ascii.c are tested
#include "src/viewer/ascii.c"

static WView *view = NULL;

/* --------------------------------------------------------------------------------------------- */

static int
test_get_byte (void *from, const off_t pos)
{
    return mcview_get_byte ((WView *) from, pos);
}

/* --------------------------------------------------------------------------------------------- */

static int
test_get_utf8 (void *from, const off_t pos, int *len)
{
    return mcview_get_utf ((WView *) from, pos, len);
}

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    const WRect r = { 0, 0, 25, 80 };

    str_init_strings ("UTF-8");
    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;
    load_codepages_list ();
    mc_global.utf8_display = TRUE;
    mc_global.display_codepage = get_codepage_index ("UTF-8");
    mc_global.source_codepage = get_codepage_index ("UTF-8");

    option_tab_spacing = 8;

    view = mcview_new (&r, FALSE);
    view->data_area = r;
    charset_conv_init (&view->conv, test_get_byte, test_get_utf8);
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    mcview_close_datasource (view);
    widget_destroy (WIDGET (view));
    view = NULL;

    free_codepages_list ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

static void
test_set_text (const char *text, const gboolean nroff)
{
    mcview_close_datasource (view);
    mcview_set_datasource_string (view, text);
    view->mode_flags.nroff = nroff;
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_line_width_ds") */
static const struct test_line_width_ds
{
    const char *text;
    const off_t width;
} test_line_width_ds[] = {
    // TAB advances to the next tab stop
    { "a\tb\n", 9 },
    { "1234567\tb\n", 9 },
    { "12345678\tb\n", 17 },
    // CJK character is two columns wide
    { "\xe4\xb8\xad\tb\n", 9 },
    // combining character doesn't occupy a column
    { "e\xcc\x81x\n", 2 },
    // invalid UTF-8 byte is shown as one non-printable character
    { "A\xe9Z\n", 3 },
};

/* @Test(dataSource = "test_line_width_ds") */
START_PARAMETRIZED_TEST (test_line_width, test_line_width_ds)
{
    // given
    mcview_state_machine_t state;
    off_t linewidth = -1;

    test_set_text (data->text, FALSE);
    mcview_state_machine_init (&state, 0);

    // when
    mcview_display_line (view, &state, -1, NULL, &linewidth);

    // then
    ck_assert_int_eq (linewidth, data->width);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_nroff_char_ds") */
static const struct test_nroff_char_ds
{
    const char *text;
    const int ch;
    const int color;
    const off_t next_offset;
} test_nroff_char_ds[] = {
    { "_\bx", 'x', VIEWER_UNDERLINED_COLOR, 3 },
    { "x\bx", 'x', VIEWER_BOLD_COLOR, 3 },
    { "_\bx\bx", 'x', VIEWER_BOLD_UNDERLINED_COLOR, 5 },
    // not a nroff sequence: return the first character only
    { "a\bb", 'a', VIEWER_NORMAL_COLOR, 1 },
    // x _ x is not bold and underlined
    { "x\b_\b_", 'x', VIEWER_NORMAL_COLOR, 1 },
    // underlined non-ASCII character
    { "_\b\xd0\x96", 0x0416, VIEWER_UNDERLINED_COLOR, 4 },
};

/* @Test(dataSource = "test_nroff_char_ds") */
START_PARAMETRIZED_TEST (test_nroff_char, test_nroff_char_ds)
{
    // given
    mcview_state_machine_t state;
    int color = -1;
    gboolean ok;

    test_set_text (data->text, TRUE);
    mcview_state_machine_init (&state, 0);

    // when
    ok = mcview_get_next_maybe_nroff_char (view, &state, &color);

    // then
    mctest_assert_true (ok);
    ck_assert_int_eq (view->conv.ch, data->ch);
    ck_assert_int_eq (color, data->color);
    ck_assert_int_eq (state.offset, data->next_offset);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_invalid_utf8_ds") */
static const struct test_invalid_utf8_ds
{
    const char *text;
} test_invalid_utf8_ds[] = {
    { "\xe9" },
    { "\xc3Z" },
    // 0xFF must not be confused with EOF
    { "\xff" },
};

/* @Test(dataSource = "test_invalid_utf8_ds") */
START_PARAMETRIZED_TEST (test_invalid_utf8, test_invalid_utf8_ds)
{
    // given
    mcview_state_machine_t state;
    gboolean ok;

    test_set_text (data->text, FALSE);
    mcview_state_machine_init (&state, 0);

    // when
    ok = mcview_get_next_char (view, &state);

    // then: invalid byte is a single non-printable character, not a Latin-1 one (ticket #3783)
    mctest_assert_true (ok);
    mctest_assert_false (view->conv.printable);
    ck_assert_int_eq (view->conv.ch, NON_PRINTABLE_CHAR);
    ck_assert_int_eq (state.offset, 1);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_is_nroff_sequence)
{
    // given
    test_set_text ("ab_\bc", TRUE);

    // when, then
    ck_assert_int_eq (mcview_is_nroff_sequence (view, 0), FALSE);
    ck_assert_int_eq (mcview_is_nroff_sequence (view, 2), TRUE);
    ck_assert_int_eq (mcview_is_nroff_sequence (view, 4), FALSE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_ccache_lookup_offset_by_line)
{
    // given
    GString *text;
    coord_cache_entry_t coord = { 0 };

    text = g_string_new (NULL);
    for (int i = 0; i < 2000; i++)
        g_string_append_printf (text, "line %05d\n", i);
    test_set_text (text->str, FALSE);
    g_string_free (text, TRUE);

    coord.cc_line = 1500;

    // when
    mcview_ccache_lookup (view, &coord, CCACHE_OFFSET);

    // then
    ck_assert_int_eq (coord.cc_offset, 1500 * 11);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_nroff_real_len_at_eof)
{
    // given
    test_set_text ("abc", TRUE);

    // when: the range goes beyond EOF
    const int len = mcview__get_nroff_real_len (view, 2, 2);

    // then: no hang, no nroff sequences
    ck_assert_int_eq (len, 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_nroff_current_char)
{
    // given
    mcview_nroff_t *nroff;

    test_set_text ("a\n_\b\xd0\x96", TRUE);

    // when: non-printable character
    nroff = mcview_nroff_seq_new_num (view, 1);

    // then: search engine gets it as is
    ck_assert_int_eq (nroff->type, NROFF_TYPE_NONE);
    ck_assert_int_eq (nroff->current_char, '\n');

    // when: underlined character
    mcview_nroff_seq_next (nroff);

    // then: search engine gets the character in the source codepage
    ck_assert_int_eq (nroff->type, NROFF_TYPE_UNDERLINE);
    ck_assert_int_eq (nroff->current_char, 0x0416);
    ck_assert_int_eq (nroff->char_length, 2);

    mcview_nroff_seq_free (&nroff);
}
END_TEST

/* @Test */
START_TEST (test_nroff_seq_prev)
{
    // given
    mcview_nroff_t *nroff;
    int c;

    test_set_text ("a_\bcd", TRUE);
    nroff = mcview_nroff_seq_new_num (view, 4);

    // when: go back from 'd'
    c = mcview_nroff_seq_prev (nroff);

    // then: previous character is underlined 'c'
    ck_assert_int_eq (c, 'c');
    ck_assert_int_eq (nroff->index, 1);
    ck_assert_int_eq (nroff->type, NROFF_TYPE_UNDERLINE);

    mcview_nroff_seq_free (&nroff);
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
    mctest_add_parameterized_test (tc_core, test_line_width, test_line_width_ds);
    mctest_add_parameterized_test (tc_core, test_nroff_char, test_nroff_char_ds);
    mctest_add_parameterized_test (tc_core, test_invalid_utf8, test_invalid_utf8_ds);
    tcase_add_test (tc_core, test_is_nroff_sequence);
    tcase_add_test (tc_core, test_ccache_lookup_offset_by_line);
    tcase_add_test (tc_core, test_nroff_real_len_at_eof);
    tcase_add_test (tc_core, test_nroff_current_char);
    tcase_add_test (tc_core, test_nroff_seq_prev);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
