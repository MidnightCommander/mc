/*
   src/editor - tests for undo and redo

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2026

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

#include "src/editor/edit-impl.h"  // edit_cursor_move()
#include "src/editor/editwidget.h"
#include "src/editor/editmacros.h"  // edit_load_macro_cmd()

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

    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;
    load_codepages_list ();

    edit_options.filesize_threshold = (char *) "64M";
    edit_options.group_undo = FALSE;

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
type_bytes (const char *s)
{
    for (; *s != '\0'; s++)
        edit_execute_key_command (test_edit, CK_InsertChar, (unsigned char) *s);
}

/* --------------------------------------------------------------------------------------------- */

static void
check_text (const char *expected)
{
    GString *actual;

    actual = g_string_new ("");
    for (off_t i = 0; i < test_edit->buffer.size; i++)
        g_string_append_c (actual, (char) edit_buffer_get_byte (&test_edit->buffer, i));

    mctest_assert_str_eq (actual->str, expected);
    g_string_free (actual, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_redo_line_break)
{
    type_bytes ("a\n");

    edit_execute_key_command (test_edit, CK_BackSpace, -1);
    check_text ("a");

    edit_execute_key_command (test_edit, CK_Undo, -1);
    check_text ("a\n");

    // redo removes the line break again
    edit_execute_key_command (test_edit, CK_Redo, -1);
    check_text ("a");
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
    tcase_add_test (tc_core, test_redo_line_break);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
