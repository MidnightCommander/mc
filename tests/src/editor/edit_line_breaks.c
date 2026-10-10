/*
   src/editor - tests for line break handling: CRLF/LF/CR detection,
   atomic "\r\n" editing, line break inheritance and write conversion

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

#define TEST_SUITE_NAME "/src/editor"

#include "tests/mctest.h"

#include <stdio.h>

#include "lib/charsets.h"
#include "lib/keybind.h"
#include "lib/vfs/path.h"
#include "lib/vfs/vfs.h"
#include "src/selcodepage.h"
#include "src/vfs/local/local.c"

#include "src/editor/edit-impl.h"
#include "src/editor/editmacros.h"  // edit_load_macro_cmd()
#include "src/editor/editsearch.h"  // edit_search_update_callback()
#include "src/editor/editwidget.h"

static WGroup owner;
static WEdit *test_edit;

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
void
status_msg_init (status_msg_t *sm, const char *title, double delay, status_msg_cb init_cb,
                 status_msg_update_cb update_cb, status_msg_cb deinit_cb)
{
    (void) sm;
    (void) title;
    (void) delay;
    (void) init_cb;
    (void) update_cb;
    (void) deinit_cb;
}

/* --------------------------------------------------------------------------------------------- */

/* @Mock */
void
status_msg_deinit (status_msg_t *sm)
{
    (void) sm;
}

/* --------------------------------------------------------------------------------------------- */

/* @Mock */
mc_search_cbret_t
edit_search_update_callback (const void *user_data, off_t char_offset)
{
    (void) user_data;
    (void) char_offset;

    return MC_SEARCH_CB_OK;
}

/* --------------------------------------------------------------------------------------------- */

/* @Mock */
void
edit_load_syntax (WEdit *edit, GPtrArray *pnames, const char *type)
{
    (void) edit;
    (void) pnames;
    (void) type;
}

/* --------------------------------------------------------------------------------------------- */

/* @Mock */
int
edit_get_syntax_color (WEdit *edit, off_t byte_index)
{
    (void) edit;
    (void) byte_index;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

/* @Mock */
gboolean
edit_load_macro_cmd (WEdit *edit)
{
    (void) edit;

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    WRect r;

    str_init_strings (NULL);

    vfs_init ();
    vfs_init_localfs ();
    vfs_setup_work_dir ();

    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;
    load_codepages_list ();

    edit_options.filesize_threshold = (char *) "64M";
    edit_options.return_does_auto_indent = FALSE;
    edit_options.save_position = FALSE;

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
    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

// the line counters are maintained by edit_insert()/edit_delete() and the cached line break type
// is refreshed on redraw in the real session; refresh them after the raw buffer manipulations
static void
test_refresh_lines (void)
{
    long lines = 0;  // the number of line breaks, as edit_buffer_read_file() counts it
    long curs_line = 0;

    for (off_t i = 0; i < test_edit->buffer.size; i++)
    {
        if (edit_buffer_get_byte (&test_edit->buffer, i) == '\n')
        {
            lines++;
            if (i < test_edit->buffer.curs1)
                curs_line++;
        }
    }

    test_edit->buffer.lines = lines;
    test_edit->buffer.curs_line = curs_line;
    edit_buffer_refresh_line_breaks (&test_edit->buffer);
}

/* --------------------------------------------------------------------------------------------- */

static void
test_load_text (const char *text)
{
    for (; *text != '\0'; text++)
        edit_buffer_insert (&test_edit->buffer, (unsigned char) *text);
    test_refresh_lines ();
}

/* --------------------------------------------------------------------------------------------- */

// move the cursor to the absolute buffer position
static void
test_cursor_to (off_t pos)
{
    edit_cursor_move (test_edit, pos - test_edit->buffer.curs1);
    test_refresh_lines ();
}

/* --------------------------------------------------------------------------------------------- */

static void
test_check (const char *expected)
{
    GString *actual;

    actual = g_string_new ("");

    for (off_t i = 0; i < test_edit->buffer.size; i++)
        g_string_append_c (actual, edit_buffer_get_byte (&test_edit->buffer, i));

    mctest_assert_str_eq (actual->str, expected);
    g_string_free (actual, TRUE);
}

/* --------------------------------------------------------------------------------------------- */
/* edit_buffer_detect_line_breaks() */

static const struct test_detect_ds
{
    const char *in;
    LineBreaks expected;
} test_detect_ds[] = {
    // empty buffer
    { "", LB_UNIX },
    { "abc", LB_UNIX },
    // all "\n"
    { "a\nb\n", LB_UNIX },
    { "a\nb", LB_UNIX },
    // all "\r\n"
    { "a\r\nb\r\n", LB_WIN },
    { "a\r\nb", LB_WIN },
    // all "\r"
    { "a\rb\r", LB_MAC },
    { "a\rb", LB_MAC },
    // mixture of line breaks
    { "a\r\nb\n", LB_ASIS },
    { "a\nb\r\n", LB_ASIS },
    { "a\rb\n", LB_ASIS },
    { "a\nb\rc", LB_ASIS },
    { "a\r\nb\rc", LB_ASIS },
};

/* @Test(dataSource = "test_detect_ds") */
START_PARAMETRIZED_TEST (test_detect, test_detect_ds)
{
    // given
    test_load_text (data->in);

    // when
    const LineBreaks lb = edit_buffer_detect_line_breaks (&test_edit->buffer);

    // then
    ck_assert_int_eq (lb, data->expected);
}
END_PARAMETRIZED_TEST

// deleting the byte between "\r" and "\n" makes them a "\r\n" line break
START_TEST (test_detect_after_joining_cr_lf)
{
    // given: a mixed text, cursor is after the "X" between "\r" and "\n"
    test_load_text ("a\rX\nb\r\n");
    test_cursor_to (3);
    ck_assert_int_eq (edit_buffer_get_line_breaks (&test_edit->buffer), LB_ASIS);

    // when
    edit_execute_cmd (test_edit, CK_BackSpace, -1);
    edit_buffer_refresh_line_breaks (&test_edit->buffer);

    // then
    test_check ("a\r\nb\r\n");
    ck_assert_int_eq (edit_buffer_get_line_breaks (&test_edit->buffer), LB_WIN);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* edit_buffer_is_crlf() */

static const struct test_is_crlf_ds
{
    const char *in;
    off_t index;
    gboolean expected;
} test_is_crlf_ds[] = {
    { "a\r\nb", 1, TRUE },    // "\r" of the "\r\n" pair
    { "a\r\nb", 0, FALSE },   // plain char
    { "a\r\nb", 2, FALSE },   // "\n" of the "\r\n" pair
    { "a\r\nb", 3, FALSE },   // out of range
    { "a\r\nb", -1, FALSE },  // negative index
    { "a\r", 1, FALSE },      // trailing "\r", out of range
    { "\r\n", 0, TRUE },      //
    { "a\rb\n", 1, FALSE },   // standalone "\r"
    { "a\rb\n", 3, FALSE },   // standalone "\n"
};

/* @Test(dataSource = "test_is_crlf_ds") */
START_PARAMETRIZED_TEST (test_is_crlf, test_is_crlf_ds)
{
    // given
    test_load_text (data->in);

    // when
    const gboolean is_crlf = edit_buffer_is_crlf (&test_edit->buffer, data->index);

    // then
    ck_assert_int_eq (is_crlf, data->expected);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */
/* edit_buffer_trailing_ws_start() */

static const struct test_trailing_ws_ds
{
    const char *in;
    off_t bol;
    off_t expected;
} test_trailing_ws_ds[] = {
    // LF lines
    { "ab  \n", 0, 2 },  // trailing spaces
    { "ab\n", 0, 2 },    // no trailing spaces
    { "   \n", 0, 0 },   // the line is all spaces
    // CRLF lines in a pure Windows file: the hidden "\r" is part of the line break,
    // so the content ends at the "\r"
    { "ab  \r\n", 0, 2 },        // trailing spaces
    { "ab\r\n", 0, 2 },          // no trailing spaces
    { "    \r\n", 0, 0 },        // the line is all spaces
    { "a\t \r\n", 0, 1 },        // trailing tab + space
    { "xx\r\nab  \r\n", 4, 6 },  // second line, trailing spaces
    // a CRLF line in a mixed file: the visible "\r" ("^M") is the last character
    { "xx\nab  \r\n", 3, 8 },
    // last line without a line break
    { "ab  ", 0, 2 },
};

/* @Test(dataSource = "test_trailing_ws_ds") */
START_PARAMETRIZED_TEST (test_trailing_ws_start, test_trailing_ws_ds)
{
    // given
    test_load_text (data->in);

    // when
    const off_t tws = edit_buffer_trailing_ws_start (&test_edit->buffer, data->bol);

    // then
    ck_assert_int_eq (tws, data->expected);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */
/* edit_write_stream() */

static const struct test_write_ds
{
    const char *in;
    LineBreaks lb;
    const char *out;
} test_write_ds[] = {
    { "a\r\nb\r\n", LB_ASIS, "a\r\nb\r\n" },  // as-is
    { "a\r\nb\r\n", LB_UNIX, "a\nb\n" },      // "\r\n" -> "\n"
    { "a\r\nb\r\n", LB_WIN, "a\r\nb\r\n" },   // "\r\n" -> "\r\n"
    { "a\r\nb\r\n", LB_MAC, "a\rb\r" },       // "\r\n" -> "\r"
    { "a\nb\n", LB_WIN, "a\r\nb\r\n" },       // "\n" -> "\r\n"
    { "a\nb\n", LB_MAC, "a\rb\r" },           // "\n" -> "\r"
    { "a\nb", LB_WIN, "a\r\nb" },             // no trailing line break
    { "a\r\nb", LB_UNIX, "a\nb" },            // no trailing line break
    { "abc\n", LB_UNIX, "abc\n" },            // no extra "\n" is appended (regression)
    { "abc\r\n", LB_UNIX, "abc\n" },
    { "abc\r\n", LB_WIN, "abc\r\n" },
    { "\n", LB_WIN, "\r\n" },              // line break at the beginning
    { "a\r\n\r\nb", LB_UNIX, "a\n\nb" },   // blank line
    { "a\r\r\nb", LB_UNIX, "a\n\nb" },     // standalone "\r" before "\r\n"
    { "a\r\nb\r\nc", LB_MAC, "a\rb\rc" },  // last line without line break
    { "a\rb", LB_UNIX, "a\nb" },           // standalone "\r" is a line break too
    { "abc", LB_WIN, "abc" },              // no line breaks
};

/* @Test(dataSource = "test_write_ds") */
START_PARAMETRIZED_TEST (test_write_stream, test_write_ds)
{
    char *mem = NULL;
    size_t mem_size = 0;
    FILE *f;
    off_t written;

    // given
    test_load_text (data->in);
    test_edit->lb = data->lb;

    // when
    f = open_memstream (&mem, &mem_size);
    written = edit_write_stream (test_edit, f);
    fclose (f);

    // then
    ck_assert_int_eq (written, (off_t) strlen (data->in));
    mctest_assert_str_eq (mem, data->out);
    free (mem);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */
/* CK_Enter: a new line break inherits the type of the current line break */

START_TEST (test_enter_inherits_crlf)
{
    // given: cursor is at the end of the first line
    test_load_text ("line1\r\nline2\r\n");
    test_cursor_to (5);

    // when
    edit_execute_cmd (test_edit, CK_Enter, -1);

    // then
    test_check ("line1\r\n\r\nline2\r\n");
}
END_TEST

START_TEST (test_enter_inherits_lf)
{
    // given: cursor is at the end of the first line
    test_load_text ("line1\nline2\n");
    test_cursor_to (5);

    // when
    edit_execute_cmd (test_edit, CK_Enter, -1);

    // then
    test_check ("line1\n\nline2\n");
}
END_TEST

// the last line has no line break of its own: use the previous line's line break
START_TEST (test_enter_last_line_inherits_crlf)
{
    // given: cursor is at the end of the last line
    test_load_text ("a\r\nb\r\nc");
    test_cursor_to (7);

    // when
    edit_execute_cmd (test_edit, CK_Enter, -1);

    // then
    test_check ("a\r\nb\r\nc\r\n");
}
END_TEST

START_TEST (test_enter_last_line_inherits_lf)
{
    // given: cursor is at the end of the last line
    test_load_text ("a\nb\nc");
    test_cursor_to (5);

    // when
    edit_execute_cmd (test_edit, CK_Enter, -1);

    // then
    test_check ("a\nb\nc\n");
}
END_TEST

START_TEST (test_enter_empty_buffer)
{
    // when
    edit_execute_cmd (test_edit, CK_Enter, -1);

    // then
    test_check ("\n");
}
END_TEST

/* with auto indent enabled the line break must stay of the file's type */
START_TEST (test_enter_auto_indent_crlf)
{
    // given: indented line, cursor at the end of the second line
    edit_options.return_does_auto_indent = TRUE;
    test_load_text ("  a\r\n  b\r\n");
    test_cursor_to (8);

    // when
    edit_execute_cmd (test_edit, CK_Enter, -1);

    // then: the new line gets the indent of the previous line, line break stays CRLF
    test_check ("  a\r\n  b\r\n  \r\n");
}
END_TEST

/* with auto paragraph formatting enabled pressing Enter must keep the file's line breaks */
START_TEST (test_enter_auto_para_format_crlf)
{
    // given: cursor at the end of the second line
    edit_options.auto_para_formatting = TRUE;
    test_load_text ("aaaa\r\nbbbb\r\n");
    test_cursor_to (10);

    // when
    edit_execute_cmd (test_edit, CK_Enter, -1);

    // then
    test_check ("aaaa\r\nbbbb\r\n\r\n");
}
END_TEST

/* formatting a CRLF paragraph must not leak "\r" into the text or change line breaks */
START_TEST (test_format_paragraph_crlf)
{
    // given: a CRLF paragraph, cursor on a non-blank line
    test_load_text ("aaaa bbbb\r\ncccc dddd\r\n");
    test_cursor_to (11);

    // when
    format_paragraph (test_edit, FALSE);

    // then: the paragraph is left unchanged (line breaks intact, no stray "\r")
    test_check ("aaaa bbbb\r\ncccc dddd\r\n");
}
END_TEST

START_TEST (test_enter_auto_indent_crlf_empty_prev_line)
{
    // given: cursor is on an empty line
    edit_options.return_does_auto_indent = TRUE;
    test_load_text ("line1\r\n\r\nline3\r\n");
    test_cursor_to (7);

    // when
    edit_execute_cmd (test_edit, CK_Enter, -1);

    // then
    test_check ("line1\r\n\r\n\r\nline3\r\n");
}
END_TEST

START_TEST (test_enter_auto_indent_crlf_last_line)
{
    // given: cursor is at the end of the last (indented) line
    edit_options.return_does_auto_indent = TRUE;
    test_load_text ("  a\r\n  b\r\n  c");
    test_cursor_to (13);

    // when
    edit_execute_cmd (test_edit, CK_Enter, -1);

    // then: the new line gets the indent of the previous line, line break stays CRLF
    test_check ("  a\r\n  b\r\n  c\r\n  ");
}
END_TEST

/* typewriter wrap: the line is broken at the last space, the text typed after it stays in order */
START_TEST (test_typewriter_wrap_crlf)
{
    // given: a CRLF file, wrap at column 8
    edit_options.typewriter_wrap = TRUE;
    edit_options.word_wrap_line_length = 8;
    test_load_text ("\r\n");
    test_cursor_to (0);

    // when
    for (const char *s = "aaaa bbbbcd"; *s != '\0'; s++)
        edit_execute_cmd (test_edit, -1, *s);

    // then
    test_check ("aaaa \r\nbbbbcd\r\n");
    ck_assert_int_eq (test_edit->buffer.curs1, 13);
}
END_TEST

START_TEST (test_typewriter_wrap_lf)
{
    // given: an LF file, wrap at column 8
    edit_options.typewriter_wrap = TRUE;
    edit_options.word_wrap_line_length = 8;
    test_load_text ("\n");
    test_cursor_to (0);

    // when
    for (const char *s = "aaaa bbbbcd"; *s != '\0'; s++)
        edit_execute_cmd (test_edit, -1, *s);

    // then
    test_check ("aaaa \nbbbbcd\n");
    ck_assert_int_eq (test_edit->buffer.curs1, 12);
}
END_TEST

/* moving down onto a shorter CRLF line must not leave the cursor inside the "\r\n" pair */
START_TEST (test_down_then_enter_crlf)
{
    // given: auto-indent on; a non-empty line, an empty line, a non-empty line; all CRLF
    edit_options.return_does_auto_indent = TRUE;
    test_load_text ("xxxx\r\n\r\nyyyy\r\n");
    test_cursor_to (0);

    // when: go to the end of the first line, move down onto the empty line, press Enter
    edit_execute_cmd (test_edit, CK_End, -1);
    edit_execute_cmd (test_edit, CK_Down, -1);

    // then: the cursor is at the end of the empty line, before its "\r" (not after it)
    ck_assert_int_eq (test_edit->buffer.curs1, 6);

    edit_execute_cmd (test_edit, CK_Enter, -1);

    // a new CRLF line is added; no LF line break is introduced
    test_check ("xxxx\r\n\r\n\r\nyyyy\r\n");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* CK_End: the cursor stops before the "\r" of a "\r\n" line break */

START_TEST (test_end_stops_before_cr)
{
    // given: cursor is at the begin of the first line
    test_load_text ("ab\r\ncd");
    test_cursor_to (0);

    // when
    edit_execute_cmd (test_edit, CK_End, -1);

    // then
    ck_assert_int_eq (test_edit->buffer.curs1, 2);
}
END_TEST

START_TEST (test_end_stops_at_lf)
{
    // given: cursor is at the begin of the first line
    test_load_text ("ab\ncd");
    test_cursor_to (0);

    // when
    edit_execute_cmd (test_edit, CK_End, -1);

    // then
    ck_assert_int_eq (test_edit->buffer.curs1, 2);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* CK_Delete/CK_BackSpace: a "\r\n" line break is an atomic unit */

START_TEST (test_delete_crlf_atomic)
{
    // given: cursor is on the "\r" of a "\r\n" line break
    test_load_text ("ab\r\ncd");
    test_cursor_to (2);

    // when
    edit_execute_cmd (test_edit, CK_Delete, -1);

    // then
    test_check ("abcd");
    ck_assert_int_eq (test_edit->buffer.curs1, 2);
}
END_TEST

START_TEST (test_backspace_crlf_atomic)
{
    // given: cursor is after the "\n" of a "\r\n" line break
    test_load_text ("ab\r\ncd");
    test_cursor_to (4);

    // when
    edit_execute_cmd (test_edit, CK_BackSpace, -1);

    // then
    test_check ("abcd");
    ck_assert_int_eq (test_edit->buffer.curs1, 2);
}
END_TEST

// a standalone "\r" (not followed by "\n") is not a line break: delete one byte only
START_TEST (test_delete_standalone_cr)
{
    // given: cursor is on the standalone "\r"
    test_load_text ("ab\rcd");
    test_cursor_to (2);

    // when
    edit_execute_cmd (test_edit, CK_Delete, -1);

    // then
    test_check ("abcd");
    ck_assert_int_eq (test_edit->buffer.curs1, 2);
}
END_TEST

START_TEST (test_backspace_standalone_cr)
{
    // given: cursor is after the standalone "\r"
    test_load_text ("ab\rcd");
    test_cursor_to (3);

    // when
    edit_execute_cmd (test_edit, CK_BackSpace, -1);

    // then
    test_check ("abcd");
    ck_assert_int_eq (test_edit->buffer.curs1, 2);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* CK_Right/CK_Left: the cursor never stops between the "\r" and the "\n" of a line break */

START_TEST (test_right_skips_crlf)
{
    // given: cursor is at the end of the first line
    test_load_text ("abc\r\ndef\r\n");
    test_cursor_to (0);
    edit_execute_cmd (test_edit, CK_End, -1);

    // when
    edit_execute_cmd (test_edit, CK_Right, -1);

    // then: the cursor is at the beginning of the second line
    ck_assert_int_eq (test_edit->buffer.curs1, 5);
}
END_TEST

START_TEST (test_left_skips_crlf)
{
    // given: cursor is at the beginning of the second line
    test_load_text ("abc\r\ndef\r\n");
    test_cursor_to (5);

    // when
    edit_execute_cmd (test_edit, CK_Left, -1);

    // then: the cursor is at the end of the first line, before the "\r"
    ck_assert_int_eq (test_edit->buffer.curs1, 3);
}
END_TEST

// Enter after Right at the end of a CRLF line adds a CRLF line, not an LF one
START_TEST (test_right_then_enter_crlf)
{
    // given: cursor is at the end of the first line
    test_load_text ("abc\r\ndef\r\n");
    test_cursor_to (0);
    edit_execute_cmd (test_edit, CK_End, -1);

    // when
    edit_execute_cmd (test_edit, CK_Right, -1);
    edit_execute_cmd (test_edit, CK_Enter, -1);

    // then
    test_check ("abc\r\n\r\ndef\r\n");
}
END_TEST

// Enter with the cursor between "\r" and "\n" does not split the line break
START_TEST (test_enter_inside_crlf)
{
    // given: cursor is between the "\r" and the "\n"
    test_load_text ("abc\r\ndef\r\n");
    test_cursor_to (4);

    // when
    edit_execute_cmd (test_edit, CK_Enter, -1);

    // then
    test_check ("abc\r\n\r\ndef\r\n");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Commands that move the cursor or select text treat a hidden "\r\n" line break as a single
 * character, exactly like a "\n" one */

START_TEST (test_word_right_crlf)
{
    // given: cursor is at the end of the first line
    test_load_text ("abc\r\nxyz\r\n");
    test_cursor_to (3);

    // when
    edit_execute_cmd (test_edit, CK_WordRight, -1);

    // then: the cursor is at the beginning of the second line
    ck_assert_int_eq (test_edit->buffer.curs1, 5);
}
END_TEST

START_TEST (test_word_right_tws_crlf)
{
    // given: cursor is before the trailing spaces of the first line
    test_load_text ("abc   \r\nxyz\r\n");
    test_cursor_to (3);

    // when
    edit_execute_cmd (test_edit, CK_WordRight, -1);

    // then: the cursor is at the end of the first line, before the "\r"
    ck_assert_int_eq (test_edit->buffer.curs1, 6);
}
END_TEST

START_TEST (test_word_left_crlf)
{
    // given: cursor is at the beginning of the second line
    test_load_text ("abc\r\nxyz\r\n");
    test_cursor_to (5);

    // when
    edit_execute_cmd (test_edit, CK_WordLeft, -1);

    // then: the cursor is at the end of the first line, before the "\r"
    ck_assert_int_eq (test_edit->buffer.curs1, 3);

    // when
    edit_execute_cmd (test_edit, -1, 'X');

    // then
    test_check ("abcX\r\nxyz\r\n");
}
END_TEST

// delete word right deletes the trailing spaces, but not the line break
START_TEST (test_right_delete_word_tws_crlf)
{
    // given: cursor is before the trailing spaces of the first line
    test_load_text ("abc  \r\nxyz\r\n");
    test_cursor_to (3);

    // when
    edit_execute_cmd (test_edit, CK_DeleteToWordEnd, -1);

    // then
    test_check ("abc\r\nxyz\r\n");
}
END_TEST

// the marked line ends before its "\r\n" line break
START_TEST (test_mark_line_crlf)
{
    // given
    edit_options.persistent_selections = FALSE;
    test_load_text ("abc\r\nxyz\r\n");
    test_cursor_to (0);

    // when
    edit_execute_cmd (test_edit, CK_MarkLine, -1);
    edit_execute_cmd (test_edit, CK_Delete, -1);

    // then: the text of the line is deleted, the line break is intact
    test_check ("\r\nxyz\r\n");
}
END_TEST

// marking a "word" at the end of a line marks the whole "\r\n" line break
START_TEST (test_mark_word_eol_crlf)
{
    // given
    edit_options.persistent_selections = FALSE;
    test_load_text ("abc\r\nxyz\r\n");
    test_cursor_to (3);

    // when
    edit_execute_cmd (test_edit, CK_MarkWord, -1);
    edit_execute_cmd (test_edit, CK_Delete, -1);

    // then
    test_check ("abcxyz\r\n");
}
END_TEST

// a search can leave the cursor between "\r" and "\n": the next command starts before "\r"
START_TEST (test_search_lf_then_type_crlf)
{
    // given
    test_load_text ("abc\r\nxyz\r\n");
    test_cursor_to (0);
    edit_search_options.type = MC_SEARCH_T_HEX;
    test_edit->last_search_string = g_strdup ("0a");
    edit_search_init (test_edit, test_edit->last_search_string);
    test_edit->search_start = 0;

    // when: search for "\n" and type a character
    edit_search_cmd (test_edit, TRUE);
    edit_execute_cmd (test_edit, -1, 'Q');

    // then
    test_check ("abcQ\r\nxyz\r\n");
}
END_TEST

// a column block does not include the hidden "\r" of short lines
START_TEST (test_column_copy_crlf)
{
    // given: columns 1..5 of the first two lines are marked, cursor is on the third line
    test_load_text ("abc\r\nabcdef\r\nXXXXXXXX\r\nYYYYYYYY\r\n");
    test_edit->column_highlight = 1;
    edit_set_markers (test_edit, 1, 10, 1, 5);
    test_cursor_to (13);

    // when
    edit_execute_cmd (test_edit, CK_Copy, -1);

    // then: the same result as with "\n" line breaks
    test_check ("abc\r\nabcdef\r\nbc  XXXXXXXX\r\nbcdeYYYYYYYY\r\n");
}
END_TEST

START_TEST (test_column_move_crlf)
{
    // given: columns 1..5 of the first two lines are marked, cursor is on the third line
    test_load_text ("abc\r\nabcdef\r\nXXXXXXXX\r\nYYYYYYYY\r\n");
    test_edit->column_highlight = 1;
    edit_set_markers (test_edit, 1, 10, 1, 5);
    test_cursor_to (13);

    // when
    edit_execute_cmd (test_edit, CK_Move, -1);

    // then: the same result as with "\n" line breaks
    test_check ("a\r\naf\r\nbc  XXXXXXXX\r\nbcdeYYYYYYYY\r\n");
}
END_TEST

// a column block pasted at the end of a short line does not add trailing spaces to it
START_TEST (test_column_paste_short_line_crlf)
{
    // given: columns 1..5 of the first two lines are marked, cursor is at the end of "X"
    test_load_text ("a\r\nabcdef\r\nX\r\nY\r\n");
    test_edit->column_highlight = 1;
    edit_set_markers (test_edit, 1, 8, 1, 5);
    test_cursor_to (12);

    // when
    edit_execute_cmd (test_edit, CK_Copy, -1);

    // then: the same result as with "\n" line breaks
    test_check ("a\r\nabcdef\r\nX\r\nYbcde\r\n");
}
END_TEST

// a column block pasted from the clipboard file does not add trailing spaces to short lines
START_TEST (test_column_paste_from_file_crlf)
{
    char *path = NULL;
    int fd;
    vfs_path_t *vpath;

    // given: columns 1..5 of the first three lines are saved as a column block,
    // cursor is at the end of "X"
    test_load_text ("abcdef\r\nabc\r\nabcdef\r\nX\r\nY\r\nZ\r\nW\r\n");
    fd = g_file_open_tmp ("mc-test-line-breaks-XXXXXX", &path, NULL);
    ck_assert_int_ge (fd, 0);
    close (fd);
    test_edit->column_highlight = 1;
    edit_set_markers (test_edit, 1, 19, 1, 5);
    mctest_assert_true (edit_save_block (test_edit, path, 1, 19));
    edit_set_markers (test_edit, 0, 0, 0, 0);
    test_edit->column_highlight = 0;
    test_cursor_to (22);

    // when
    vpath = vfs_path_from_str (path);
    edit_insert_file (test_edit, vpath);
    vfs_path_free (vpath, TRUE);
    unlink (path);
    g_free (path);

    // then: the same result as with "\n" line breaks
    test_check ("abcdef\r\nabc\r\nabcdef\r\nXbcde\r\nYbc\r\nZbcde\r\nW\r\n");
}
END_TEST

// a selection of whole lines is kept before the hidden "\r" by a search in the selection
START_TEST (test_search_in_selection_crlf)
{
    // given: the first two lines are selected
    edit_options.persistent_selections = FALSE;
    edit_search_options.only_in_selection = TRUE;
    edit_search_options.type = MC_SEARCH_T_NORMAL;
    test_load_text ("ab\r\ncd\r\nef\r\n");
    edit_set_markers (test_edit, 0, 8, 0, 0);
    test_cursor_to (0);
    test_edit->last_search_string = g_strdup ("c");
    edit_search_init (test_edit, test_edit->last_search_string);
    test_edit->search_start = 0;

    // when: search in the selection, then delete the selection
    edit_search_cmd (test_edit, TRUE);
    edit_execute_cmd (test_edit, CK_Delete, -1);

    // then: the same result as with "\n" line breaks, no bare "\n" is left
    test_check ("\r\nef\r\n");
}
END_TEST

// a column block pasted past the end of the text adds "\r\n" line breaks
START_TEST (test_column_copy_past_eof_crlf)
{
    // given: columns 1..3 of the first two lines are marked, cursor is at the end of the text
    test_load_text ("abcd\r\nabcd\r\nX");
    test_edit->column_highlight = 1;
    edit_set_markers (test_edit, 1, 9, 1, 3);
    test_cursor_to (13);

    // when
    edit_execute_cmd (test_edit, CK_Copy, -1);

    // then: the same result as with "\n" line breaks
    test_check ("abcd\r\nabcd\r\nXbc\r\n bc");
}
END_TEST

START_TEST (test_column_paste_from_file_past_eof_crlf)
{
    char *path = NULL;
    int fd;
    vfs_path_t *vpath;

    // given: columns 1..3 of the first two lines are saved as a column block,
    // cursor is at the end of the text
    test_load_text ("abcd\r\nabcd\r\nX");
    fd = g_file_open_tmp ("mc-test-line-breaks-XXXXXX", &path, NULL);
    ck_assert_int_ge (fd, 0);
    close (fd);
    test_edit->column_highlight = 1;
    edit_set_markers (test_edit, 1, 9, 1, 3);
    mctest_assert_true (edit_save_block (test_edit, path, 1, 9));
    edit_set_markers (test_edit, 0, 0, 0, 0);
    test_edit->column_highlight = 0;
    test_cursor_to (13);

    // when
    vpath = vfs_path_from_str (path);
    edit_insert_file (test_edit, vpath);
    vfs_path_free (vpath, TRUE);
    unlink (path);
    g_free (path);

    // then: the same result as with "\n" line breaks
    test_check ("abcd\r\nabcd\r\nXbc\r\n bc");
}
END_TEST

// formatting must not add a "\n" line break to a file with hidden "\r\n" line breaks
START_TEST (test_format_last_line_crlf)
{
    // given: the last line has no line break and is longer than the wrap length
    edit_options.word_wrap_line_length = 10;
    test_load_text ("aa\r\n\r\nbbbb cccc dddd");
    test_cursor_to (20);

    // when
    edit_execute_key_command (test_edit, CK_ParagraphFormat, -1);

    // then: the paragraph is left as is
    test_check ("aa\r\n\r\nbbbb cccc dddd");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* CK_DeleteToEnd: stop before the "\r" of a "\r\n" line break */

START_TEST (test_delete_to_line_end_crlf)
{
    // given: cursor is at the begin of the first line
    test_load_text ("ab\r\ncd");
    test_cursor_to (0);

    // when
    edit_execute_cmd (test_edit, CK_DeleteToEnd, -1);

    // then: the "\r\n" line break itself is preserved
    test_check ("\r\ncd");
}
END_TEST

START_TEST (test_delete_to_line_end_lf)
{
    // given: cursor is at the begin of the first line
    test_load_text ("ab\ncd");
    test_cursor_to (0);

    // when
    edit_execute_cmd (test_edit, CK_DeleteToEnd, -1);

    // then
    test_check ("\ncd");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* In a file with a mixture of line breaks the "\r" of a "\r\n" pair is NOT hidden: it is shown
 * as "^M" and is edited as an ordinary character, exactly as in a file without any special line
 * break handling. The test buffers below are mixed (contain both "\r\n" and "\n"), so they are
 * LB_ASIS. */

// CK_End: the cursor stops after the visible "\r", at the "\n"
START_TEST (test_end_stops_after_cr_mixed)
{
    // given: a mixed file, cursor is at the begin of the CRLF line
    test_load_text ("ab\r\ncd\n");
    test_cursor_to (0);

    // when
    edit_execute_cmd (test_edit, CK_End, -1);

    // then: the cursor is after the "\r" (offset 3)
    ck_assert_int_eq (test_edit->buffer.curs1, 3);
}
END_TEST

// CK_Right/CK_Left: the cursor moves over the visible "\r" and the "\n" one by one
START_TEST (test_right_left_crlf_mixed)
{
    // given: a mixed file, cursor is before the "\r"
    test_load_text ("ab\r\ncd\n");
    test_cursor_to (2);

    // when
    edit_execute_cmd (test_edit, CK_Right, -1);

    // then
    ck_assert_int_eq (test_edit->buffer.curs1, 3);

    // when
    edit_execute_cmd (test_edit, CK_Right, -1);
    edit_execute_cmd (test_edit, CK_Left, -1);

    // then
    ck_assert_int_eq (test_edit->buffer.curs1, 3);
}
END_TEST

// CK_Delete: only the visible "\r" is deleted
START_TEST (test_delete_cr_mixed)
{
    // given: a mixed file, cursor is before the "\r" of a "\r\n" line break
    test_load_text ("ab\r\ncd\n");
    test_cursor_to (2);

    // when
    edit_execute_cmd (test_edit, CK_Delete, -1);

    // then: the "^M" is gone, the line break is intact
    test_check ("ab\ncd\n");
    ck_assert_int_eq (test_edit->buffer.curs1, 2);
}
END_TEST

// CK_BackSpace: only the "\n" is deleted
START_TEST (test_backspace_lf_mixed)
{
    // given: a mixed file, cursor is at the begin of the line after a "\r\n" line break
    test_load_text ("ab\r\ncd\n");
    test_cursor_to (4);

    // when
    edit_execute_cmd (test_edit, CK_BackSpace, -1);

    // then: the lines are joined, the "^M" stays
    test_check ("ab\rcd\n");
    ck_assert_int_eq (test_edit->buffer.curs1, 3);
}
END_TEST

// CK_DeleteToEnd: the visible "\r" is deleted together with the rest of the line
START_TEST (test_delete_to_line_end_crlf_mixed)
{
    // given: a mixed file, cursor is at the begin of the CRLF line
    test_load_text ("ab\r\ncd\n");
    test_cursor_to (0);

    // when
    edit_execute_cmd (test_edit, CK_DeleteToEnd, -1);

    // then: "ab^M" is deleted, the "\n" line break is preserved
    test_check ("\ncd\n");
}
END_TEST

// typing in overwrite mode replaces the visible "\r"
START_TEST (test_overwrite_cr_mixed)
{
    // given: a mixed file, overwrite mode, cursor is before the "\r"
    test_load_text ("ab\r\ncd\n");
    test_cursor_to (2);
    test_edit->overwrite = 1;

    // when
    edit_execute_cmd (test_edit, -1, 'X');

    // then
    test_check ("abX\ncd\n");
}
END_TEST

// CK_Enter after CK_End on a CRLF line: the line keeps its "\r\n" line break,
// a new line is added after it, the cursor is at the begin of the new line
START_TEST (test_enter_after_end_crlf_mixed)
{
    // given: a mixed file, cursor is at the begin of the CRLF line
    test_load_text ("ab\r\ncd\n");
    test_cursor_to (0);

    // when: move to the end of the line (after "^M"), then press Enter
    edit_execute_cmd (test_edit, CK_End, -1);
    edit_execute_cmd (test_edit, CK_Enter, -1);

    // then: no duplicate "\r" is introduced, the cursor is at the begin of the new line
    test_check ("ab\r\n\ncd\n");
    ck_assert_int_eq (test_edit->buffer.curs1, 4);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* Block and undo operations work byte by byte: they must not remove more than they intend to,
 * even when the text contains "\r\n" line breaks. */

// block delete removes the selected bytes only
START_TEST (test_block_delete_crlf)
{
    // given: the first two lines of a CRLF file are selected
    test_load_text ("a\r\nb\r\nXYZ\r\n");
    test_cursor_to (0);
    edit_set_markers (test_edit, 0, 6, 0, 0);

    // when
    edit_block_delete_cmd (test_edit);

    // then: the third line is intact
    test_check ("XYZ\r\n");
}
END_TEST

// block move keeps all bytes of the moved block, including the "\r"
START_TEST (test_block_move_crlf)
{
    // given: the first two lines of a CRLF file are selected, cursor is at the end of the file
    test_load_text ("a\r\nb\r\nXYZ\r\n");
    test_cursor_to (0);
    edit_set_markers (test_edit, 0, 6, 0, 0);
    test_cursor_to (11);

    // when
    edit_block_move_cmd (test_edit);

    // then
    test_check ("XYZ\r\na\r\nb\r\n");
}
END_TEST

// undo of Enter removes the inserted "\r\n" only
START_TEST (test_undo_enter_crlf)
{
    // given: cursor is at the end of the first line
    test_load_text ("abc\r\ndef\r\n");
    test_cursor_to (3);
    edit_execute_key_command (test_edit, CK_Enter, -1);
    test_check ("abc\r\n\r\ndef\r\n");

    // when
    edit_execute_key_command (test_edit, CK_Undo, -1);

    // then: the "c" before the line break is not deleted
    test_check ("abc\r\ndef\r\n");
}
END_TEST

// undo of Delete restores the whole "\r\n" line break
START_TEST (test_undo_delete_crlf)
{
    // given: cursor is before the "\r" of a "\r\n" line break
    test_load_text ("abc\r\ndef\r\n");
    test_cursor_to (3);
    edit_execute_key_command (test_edit, CK_Delete, -1);
    test_check ("abcdef\r\n");

    // when
    edit_execute_key_command (test_edit, CK_Undo, -1);

    // then
    test_check ("abc\r\ndef\r\n");
}
END_TEST

// delete word left stops at a "\r\n" line break like at a "\n" one
START_TEST (test_left_delete_word_crlf)
{
    // given: cursor is at the beginning of the second line
    test_load_text ("foo   \r\nbar");
    test_cursor_to (8);

    // when
    edit_execute_cmd (test_edit, CK_DeleteToWordBegin, -1);

    // then: only the line break is deleted, the trailing spaces are kept
    test_check ("foo   bar");
}
END_TEST

// typing in overwrite mode at the end of a CRLF line does not join it with the next line
START_TEST (test_overwrite_at_eol_crlf)
{
    // given: overwrite mode, cursor is at the end of the first line
    test_load_text ("abc\r\ndef\r\n");
    test_cursor_to (0);
    test_edit->overwrite = 1;
    edit_execute_cmd (test_edit, CK_End, -1);

    // when
    edit_execute_cmd (test_edit, -1, 'X');

    // then
    test_check ("abcX\r\ndef\r\n");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/* loading a file: the buffer keeps the raw content, detection works on the loaded text */

START_TEST (test_load_crlf_file)
{
    char *path = NULL;
    int fd;
    WEdit *edit;
    WRect r;
    edit_arg_t arg;
    vfs_path_t *vpath;
    FILE *f;
    GString *actual;

    // given: a file with "\r\n" line breaks
    fd = g_file_open_tmp ("mc-test-line-breaks-XXXXXX", &path, NULL);
    ck_assert_int_ge (fd, 0);
    f = fdopen (fd, "wb");
    mctest_assert_not_null (f);
    fputs ("line1\r\nline2\r\nline3\r\n", f);
    fclose (f);

    vpath = vfs_path_from_str (path);
    arg.file_vpath = vpath;
    arg.line_number = 0;
    rect_init (&r, 0, 0, 24, 80);
    edit = edit_init (NULL, &r, &arg);

    // then
    mctest_assert_not_null (edit);

    actual = g_string_new ("");
    for (off_t i = 0; i < edit->buffer.size; i++)
        g_string_append_c (actual, edit_buffer_get_byte (&edit->buffer, i));
    mctest_assert_str_eq (actual->str, "line1\r\nline2\r\nline3\r\n");
    g_string_free (actual, TRUE);

    ck_assert_int_eq (edit->lb, LB_ASIS);
    ck_assert_int_eq (edit_buffer_detect_line_breaks (&edit->buffer), LB_WIN);

    // cleanup
    edit_clean (edit);
    g_free (edit);
    vfs_path_free (vpath, TRUE);
    unlink (path);
    g_free (path);
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
    mctest_add_parameterized_test (tc_core, test_detect, test_detect_ds);
    tcase_add_test (tc_core, test_detect_after_joining_cr_lf);
    mctest_add_parameterized_test (tc_core, test_is_crlf, test_is_crlf_ds);
    mctest_add_parameterized_test (tc_core, test_trailing_ws_start, test_trailing_ws_ds);
    mctest_add_parameterized_test (tc_core, test_write_stream, test_write_ds);
    tcase_add_test (tc_core, test_enter_inherits_crlf);
    tcase_add_test (tc_core, test_enter_inherits_lf);
    tcase_add_test (tc_core, test_enter_last_line_inherits_crlf);
    tcase_add_test (tc_core, test_enter_last_line_inherits_lf);
    tcase_add_test (tc_core, test_enter_empty_buffer);
    tcase_add_test (tc_core, test_enter_auto_indent_crlf);
    tcase_add_test (tc_core, test_enter_auto_para_format_crlf);
    tcase_add_test (tc_core, test_format_paragraph_crlf);
    tcase_add_test (tc_core, test_enter_auto_indent_crlf_empty_prev_line);
    tcase_add_test (tc_core, test_enter_auto_indent_crlf_last_line);
    tcase_add_test (tc_core, test_typewriter_wrap_crlf);
    tcase_add_test (tc_core, test_typewriter_wrap_lf);
    tcase_add_test (tc_core, test_down_then_enter_crlf);
    tcase_add_test (tc_core, test_end_stops_before_cr);
    tcase_add_test (tc_core, test_end_stops_at_lf);
    tcase_add_test (tc_core, test_delete_crlf_atomic);
    tcase_add_test (tc_core, test_backspace_crlf_atomic);
    tcase_add_test (tc_core, test_delete_standalone_cr);
    tcase_add_test (tc_core, test_backspace_standalone_cr);
    tcase_add_test (tc_core, test_right_skips_crlf);
    tcase_add_test (tc_core, test_left_skips_crlf);
    tcase_add_test (tc_core, test_right_then_enter_crlf);
    tcase_add_test (tc_core, test_enter_inside_crlf);
    tcase_add_test (tc_core, test_word_right_crlf);
    tcase_add_test (tc_core, test_word_right_tws_crlf);
    tcase_add_test (tc_core, test_word_left_crlf);
    tcase_add_test (tc_core, test_right_delete_word_tws_crlf);
    tcase_add_test (tc_core, test_mark_line_crlf);
    tcase_add_test (tc_core, test_mark_word_eol_crlf);
    tcase_add_test (tc_core, test_search_lf_then_type_crlf);
    tcase_add_test (tc_core, test_column_copy_crlf);
    tcase_add_test (tc_core, test_column_move_crlf);
    tcase_add_test (tc_core, test_column_paste_short_line_crlf);
    tcase_add_test (tc_core, test_column_paste_from_file_crlf);
    tcase_add_test (tc_core, test_search_in_selection_crlf);
    tcase_add_test (tc_core, test_column_copy_past_eof_crlf);
    tcase_add_test (tc_core, test_column_paste_from_file_past_eof_crlf);
    tcase_add_test (tc_core, test_format_last_line_crlf);
    tcase_add_test (tc_core, test_delete_to_line_end_crlf);
    tcase_add_test (tc_core, test_delete_to_line_end_lf);
    tcase_add_test (tc_core, test_end_stops_after_cr_mixed);
    tcase_add_test (tc_core, test_right_left_crlf_mixed);
    tcase_add_test (tc_core, test_delete_cr_mixed);
    tcase_add_test (tc_core, test_backspace_lf_mixed);
    tcase_add_test (tc_core, test_delete_to_line_end_crlf_mixed);
    tcase_add_test (tc_core, test_overwrite_cr_mixed);
    tcase_add_test (tc_core, test_enter_after_end_crlf_mixed);
    tcase_add_test (tc_core, test_block_delete_crlf);
    tcase_add_test (tc_core, test_block_move_crlf);
    tcase_add_test (tc_core, test_undo_enter_crlf);
    tcase_add_test (tc_core, test_undo_delete_crlf);
    tcase_add_test (tc_core, test_left_delete_word_crlf);
    tcase_add_test (tc_core, test_overwrite_at_eol_crlf);
    tcase_add_test (tc_core, test_load_crlf_file);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */