/*
   src/viewer - ANSI SGR parser tests

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

#define TEST_SUITE_NAME "/src/viewer"

#include "tests/mctest.h"

#include "src/viewer/ansi.h"

/* --------------------------------------------------------------------------------------------- */

/* helper: feed a string through the parser and collect displayable chars */
static GString *
parse_and_collect (mcview_ansi_state_t *state, const char *input)
{
    GString *result;
    const char *p;

    result = g_string_new ("");

    for (p = input; *p != '\0'; p++)
    {
        mcview_ansi_result_t r;

        r = mcview_ansi_parse_char (state, (unsigned char) *p);
        if (r == ANSI_RESULT_CHAR)
            g_string_append_c (result, *p);
    }

    return result;
}

/* --------------------------------------------------------------------------------------------- */
/* Test: init sets default state */

START_TEST (test_ansi_init_defaults)
{
    // given
    mcview_ansi_state_t state;

    // when
    mcview_ansi_state_init (&state);

    // then
    ck_assert_int_eq (state.fg, MCVIEW_ANSI_COLOR_DEFAULT);
    ck_assert_int_eq (state.bg, MCVIEW_ANSI_COLOR_DEFAULT);
    mctest_assert_false (state.bold);
    mctest_assert_false (state.underline);
    mctest_assert_false (state.in_escape);
    mctest_assert_false (state.in_csi);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: plain text passes through unchanged */

START_TEST (test_ansi_plain_text_passthrough)
{
    // given
    mcview_ansi_state_t state;
    GString *result;

    mcview_ansi_state_init (&state);

    // when
    result = parse_and_collect (&state, "Hello, World!");

    // then
    mctest_assert_str_eq (result->str, "Hello, World!");
    g_string_free (result, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: ESC[ m (reset) is consumed, no visible output */

START_TEST (test_ansi_reset_consumed)
{
    // given
    mcview_ansi_state_t state;
    GString *result;

    mcview_ansi_state_init (&state);

    // when — ESC[m between text
    result = parse_and_collect (&state, "ab\033[mcd");

    // then — only "abcd" visible
    mctest_assert_str_eq (result->str, "abcd");
    g_string_free (result, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: ESC[0m explicit reset restores defaults */

START_TEST (test_ansi_explicit_reset)
{
    // given
    mcview_ansi_state_t state;

    mcview_ansi_state_init (&state);

    // when — set red, then reset
    g_string_free (parse_and_collect (&state, "\033[31m"), TRUE);
    ck_assert_int_eq (state.fg, 1);

    g_string_free (parse_and_collect (&state, "\033[0m"), TRUE);

    // then — back to defaults
    ck_assert_int_eq (state.fg, MCVIEW_ANSI_COLOR_DEFAULT);
    ck_assert_int_eq (state.bg, MCVIEW_ANSI_COLOR_DEFAULT);
    mctest_assert_false (state.bold);
    mctest_assert_false (state.underline);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: foreground color SGR codes 30-37 */

START_TEST (test_ansi_foreground_colors)
{
    // given
    mcview_ansi_state_t state;

    mcview_ansi_state_init (&state);

    // when — ESC[31m = red foreground
    g_string_free (parse_and_collect (&state, "\033[31m"), TRUE);

    // then
    ck_assert_int_eq (state.fg, 1);
    ck_assert_int_eq (state.bg, MCVIEW_ANSI_COLOR_DEFAULT);

    // when — ESC[34m = blue foreground
    g_string_free (parse_and_collect (&state, "\033[34m"), TRUE);

    // then
    ck_assert_int_eq (state.fg, 4);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: background color SGR codes 40-47 */

START_TEST (test_ansi_background_colors)
{
    // given
    mcview_ansi_state_t state;

    mcview_ansi_state_init (&state);

    // when — ESC[42m = green background
    g_string_free (parse_and_collect (&state, "\033[42m"), TRUE);

    // then
    ck_assert_int_eq (state.bg, 2);
    ck_assert_int_eq (state.fg, MCVIEW_ANSI_COLOR_DEFAULT);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: bold SGR code 1 */

START_TEST (test_ansi_bold)
{
    // given
    mcview_ansi_state_t state;

    mcview_ansi_state_init (&state);

    // when — ESC[1m = bold
    g_string_free (parse_and_collect (&state, "\033[1m"), TRUE);

    // then
    mctest_assert_true (state.bold);
    mctest_assert_false (state.underline);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: underline SGR code 4 */

START_TEST (test_ansi_underline)
{
    // given
    mcview_ansi_state_t state;

    mcview_ansi_state_init (&state);

    // when — ESC[4m = underline
    g_string_free (parse_and_collect (&state, "\033[4m"), TRUE);

    // then
    mctest_assert_false (state.bold);
    mctest_assert_true (state.underline);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: combined parameters ESC[01;34m = bold + blue */

START_TEST (test_ansi_combined_params)
{
    // given
    mcview_ansi_state_t state;

    mcview_ansi_state_init (&state);

    // when — ESC[01;34m = bold blue (source-highlight typical output)
    g_string_free (parse_and_collect (&state, "\033[01;34m"), TRUE);

    // then
    mctest_assert_true (state.bold);
    ck_assert_int_eq (state.fg, 4);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: bright foreground colors 90-97 */

START_TEST (test_ansi_bright_foreground)
{
    // given
    mcview_ansi_state_t state;

    mcview_ansi_state_init (&state);

    // when — ESC[91m = bright red
    g_string_free (parse_and_collect (&state, "\033[91m"), TRUE);

    // then — bright colors map to 8-15
    ck_assert_int_eq (state.fg, 9);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: bright background colors 100-107 */

START_TEST (test_ansi_bright_background)
{
    // given
    mcview_ansi_state_t state;

    mcview_ansi_state_init (&state);

    // when — ESC[101m = bright red background
    g_string_free (parse_and_collect (&state, "\033[101m"), TRUE);

    // then — bright bg colors map to 8-15
    ck_assert_int_eq (state.bg, 9);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: color change without reset — state accumulates */

START_TEST (test_ansi_color_change_without_reset)
{
    // given
    mcview_ansi_state_t state;

    mcview_ansi_state_init (&state);

    // when — set red, then set bold without resetting red
    g_string_free (parse_and_collect (&state, "\033[31m"), TRUE);
    g_string_free (parse_and_collect (&state, "\033[1m"), TRUE);

    // then — both red and bold active
    ck_assert_int_eq (state.fg, 1);
    mctest_assert_true (state.bold);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: default foreground SGR 39, default background SGR 49 */

START_TEST (test_ansi_default_color_codes)
{
    // given
    mcview_ansi_state_t state;

    mcview_ansi_state_init (&state);

    // when — set colors then reset individually
    g_string_free (parse_and_collect (&state, "\033[31;42m"), TRUE);
    ck_assert_int_eq (state.fg, 1);
    ck_assert_int_eq (state.bg, 2);

    g_string_free (parse_and_collect (&state, "\033[39m"), TRUE);

    // then — fg reset, bg preserved
    ck_assert_int_eq (state.fg, MCVIEW_ANSI_COLOR_DEFAULT);
    ck_assert_int_eq (state.bg, 2);

    // when — reset bg
    g_string_free (parse_and_collect (&state, "\033[49m"), TRUE);

    // then
    ck_assert_int_eq (state.bg, MCVIEW_ANSI_COLOR_DEFAULT);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: non-SGR CSI sequence (e.g., cursor movement) is consumed but doesn't affect colors */

START_TEST (test_ansi_non_sgr_csi_ignored)
{
    // given
    mcview_ansi_state_t state;

    mcview_ansi_state_init (&state);

    // set a known color first
    g_string_free (parse_and_collect (&state, "\033[31m"), TRUE);
    ck_assert_int_eq (state.fg, 1);

    // when — ESC[2J = clear screen (not SGR)
    g_string_free (parse_and_collect (&state, "\033[2J"), TRUE);

    // then — color unchanged, sequence consumed
    ck_assert_int_eq (state.fg, 1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: incomplete escape at end of input — chars consumed, no crash */

START_TEST (test_ansi_incomplete_escape)
{
    // given
    mcview_ansi_state_t state;
    GString *result;

    mcview_ansi_state_init (&state);

    // when — ESC[ without terminator, then normal text in next call
    result = parse_and_collect (&state, "ab\033[");
    mctest_assert_str_eq (result->str, "ab");
    g_string_free (result, TRUE);

    // parser should be in CSI state
    mctest_assert_true (state.in_csi);

    // when — continue with normal text (CSI aborted by non-param/non-terminator)
    result = parse_and_collect (&state, "31mX");

    // then — the "31m" completes the CSI, "X" is displayable
    mctest_assert_str_eq (result->str, "X");
    ck_assert_int_eq (state.fg, 1);
    g_string_free (result, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: 256-color foreground ESC[38;5;Nm */

START_TEST (test_ansi_256_color_foreground)
{
    // given
    mcview_ansi_state_t state;

    mcview_ansi_state_init (&state);

    // when — ESC[38;5;196m = 256-color red
    g_string_free (parse_and_collect (&state, "\033[38;5;196m"), TRUE);

    // then — fg set to 196
    ck_assert_int_eq (state.fg, 196);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: 256-color background ESC[48;5;Nm */

START_TEST (test_ansi_256_color_background)
{
    // given
    mcview_ansi_state_t state;

    mcview_ansi_state_init (&state);

    // when — ESC[48;5;82m = 256-color green bg
    g_string_free (parse_and_collect (&state, "\033[48;5;82m"), TRUE);

    // then — bg set to 82
    ck_assert_int_eq (state.bg, 82);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: real source-highlight output pattern */

START_TEST (test_ansi_source_highlight_pattern)
{
    // given
    mcview_ansi_state_t state;
    GString *result;

    mcview_ansi_state_init (&state);

    // when — typical source-highlight output: ESC[01;34m keyword ESC[m
    result = parse_and_collect (&state, "\033[01;34mif\033[m (x)");

    // then — visible text is "if (x)"
    mctest_assert_str_eq (result->str, "if (x)");
    // after reset, colors should be defaults
    ck_assert_int_eq (state.fg, MCVIEW_ANSI_COLOR_DEFAULT);
    mctest_assert_false (state.bold);
    g_string_free (result, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: ESC followed by non-'[' is consumed (not CSI, just ESC + char) */

START_TEST (test_ansi_esc_non_csi)
{
    // given
    mcview_ansi_state_t state;
    GString *result;

    mcview_ansi_state_init (&state);

    // when — ESC followed by 'c' (not '[') — this is "RIS" reset
    result = parse_and_collect (&state, "a\033cb");

    // then — ESC and 'c' consumed, only 'a' and 'b' visible
    mctest_assert_str_eq (result->str, "ab");
    g_string_free (result, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: truecolor foreground ESC[38;2;R;G;Bm → approximate to 256-color */

START_TEST (test_ansi_truecolor_foreground)
{
    // given
    mcview_ansi_state_t state;

    mcview_ansi_state_init (&state);

    // when — ESC[38;2;255;0;0m = truecolor red → should map to 196
    // cube: r=5 g=0 b=0 → 16 + 180 + 0 + 0 = 196
    g_string_free (parse_and_collect (&state, "\033[38;2;255;0;0m"), TRUE);

    // then
    ck_assert_int_eq (state.fg, 196);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: truecolor background ESC[48;2;R;G;Bm */

START_TEST (test_ansi_truecolor_background)
{
    // given
    mcview_ansi_state_t state;

    mcview_ansi_state_init (&state);

    // when — ESC[48;2;0;0;255m = truecolor blue bg → should map to 21
    // cube: r=0 g=0 b=5 → 16 + 0 + 0 + 5 = 21
    g_string_free (parse_and_collect (&state, "\033[48;2;0;0;255m"), TRUE);

    // then
    ck_assert_int_eq (state.bg, 21);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: truecolor combined with bold in one sequence */

START_TEST (test_ansi_truecolor_combined)
{
    // given
    mcview_ansi_state_t state;

    mcview_ansi_state_init (&state);

    // when — ESC[1;38;2;255;128;0m = bold + truecolor orange fg
    // cube: r=5 g=2 b=0 → 16 + 180 + 12 + 0 = 208
    g_string_free (parse_and_collect (&state, "\033[1;38;2;255;128;0m"), TRUE);

    // then
    mctest_assert_true (state.bold);
    ck_assert_int_eq (state.fg, 208);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Test: truecolor R;G;B values not misinterpreted as SGR codes */

START_TEST (test_ansi_truecolor_no_misparse)
{
    // given
    mcview_ansi_state_t state;

    mcview_ansi_state_init (&state);

    // when — ESC[38;2;100;150;200m — R=100 must NOT trigger bright-bg (100-107)
    // cube: r=1 g=2 b=4 → 16 + 36 + 12 + 4 = 68
    g_string_free (parse_and_collect (&state, "\033[38;2;100;150;200m"), TRUE);

    // then — bg must stay default (not affected by R=100)
    ck_assert_int_eq (state.bg, MCVIEW_ANSI_COLOR_DEFAULT);
    ck_assert_int_eq (state.fg, 68);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    // Add new tests here: ***************
    tcase_add_test (tc_core, test_ansi_init_defaults);
    tcase_add_test (tc_core, test_ansi_plain_text_passthrough);
    tcase_add_test (tc_core, test_ansi_reset_consumed);
    tcase_add_test (tc_core, test_ansi_explicit_reset);
    tcase_add_test (tc_core, test_ansi_foreground_colors);
    tcase_add_test (tc_core, test_ansi_background_colors);
    tcase_add_test (tc_core, test_ansi_bold);
    tcase_add_test (tc_core, test_ansi_underline);
    tcase_add_test (tc_core, test_ansi_combined_params);
    tcase_add_test (tc_core, test_ansi_bright_foreground);
    tcase_add_test (tc_core, test_ansi_bright_background);
    tcase_add_test (tc_core, test_ansi_color_change_without_reset);
    tcase_add_test (tc_core, test_ansi_default_color_codes);
    tcase_add_test (tc_core, test_ansi_non_sgr_csi_ignored);
    tcase_add_test (tc_core, test_ansi_incomplete_escape);
    tcase_add_test (tc_core, test_ansi_256_color_foreground);
    tcase_add_test (tc_core, test_ansi_256_color_background);
    tcase_add_test (tc_core, test_ansi_source_highlight_pattern);
    tcase_add_test (tc_core, test_ansi_esc_non_csi);
    tcase_add_test (tc_core, test_ansi_truecolor_foreground);
    tcase_add_test (tc_core, test_ansi_truecolor_background);
    tcase_add_test (tc_core, test_ansi_truecolor_combined);
    tcase_add_test (tc_core, test_ansi_truecolor_no_misparse);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
