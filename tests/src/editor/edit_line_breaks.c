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
    tcase_add_test (tc_core, test_load_crlf_file);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */