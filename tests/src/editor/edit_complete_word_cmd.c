/*
   src/editor - tests for edit_complete_word_cmd() function

   Copyright (C) 2013-2024
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2013
   Andrew Borodin <aborodin@vmail.ru>, 2021-2022

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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define TEST_SUITE_NAME "/src/editor"

#include "tests/mctest.h"

#include <ctype.h>

#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#endif
#include "lib/strutil.h"

#include "src/vfs/local/local.c"
#ifdef HAVE_CHARSET
#include "src/selcodepage.h"
#endif
#include "src/editor/editwidget.h"
#include "src/editor/editmacros.h"      /* edit_load_macro_cmd() */
#include "src/editor/editcomplete.h"

static WGroup owner;
static WEdit *test_edit;

/* --------------------------------------------------------------------------------------------- */
/* @Mock */
void
mc_refresh (void)
{
}

/* --------------------------------------------------------------------------------------------- */
/* @Mock */
void
edit_load_syntax (WEdit * _edit, GPtrArray * _pnames, const char *_type)
{
    (void) _edit;
    (void) _pnames;
    (void) _type;
}

/* --------------------------------------------------------------------------------------------- */

/* @Mock */
int
edit_get_syntax_color (WEdit * _edit, off_t _byte_index)
{
    (void) _edit;
    (void) _byte_index;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

/* @Mock */
gboolean
edit_load_macro_cmd (WEdit * _edit)
{
    (void) _edit;

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/* @CapturedValue */
static const WEdit *edit_completion_dialog_show__edit;
/* @CapturedValue */
static int edit_completion_dialog_show__max_width;
/* @CapturedValue */
static GQueue *edit_completion_dialog_show__compl;

/* @ThenReturnValue */
static char *edit_completion_dialog_show__return_value;

/* @Mock */
char *
edit_completion_dialog_show (const WEdit * edit, GQueue * compl, int max_width)
{

    edit_completion_dialog_show__edit = edit;
    edit_completion_dialog_show__max_width = max_width;

    {
        GList *i;

        edit_completion_dialog_show__compl = g_queue_new ();

        for (i = g_queue_peek_tail_link (compl); i != NULL; i = g_list_previous (i))
        {
            GString *s = (GString *) i->data;

            g_queue_push_tail (edit_completion_dialog_show__compl, mc_g_string_dup (s));
        }
    }

    return edit_completion_dialog_show__return_value;
}

static void
edit_completion_dialog_show__init (void)
{
    edit_completion_dialog_show__edit = NULL;
    edit_completion_dialog_show__max_width = 0;
    edit_completion_dialog_show__compl = NULL;
    edit_completion_dialog_show__return_value = NULL;
}

static void
edit_completion_dialog_show__string_free (gpointer data)
{
    g_string_free ((GString *) data, TRUE);
}

static void
edit_completion_dialog_show__deinit (void)
{
    if (edit_completion_dialog_show__compl != NULL)
        g_queue_free_full (edit_completion_dialog_show__compl,
                           edit_completion_dialog_show__string_free);
}

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
my_setup (void)
{
    WRect r;

    str_init_strings (NULL);

    vfs_init ();
    vfs_init_localfs ();
    vfs_setup_work_dir ();

#ifdef HAVE_CHARSET
    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;
    load_codepages_list ();
#endif /* HAVE_CHARSET */

    mc_global.main_config = mc_config_init ("edit_complete_word_cmd.ini", FALSE);
    mc_config_set_bool (mc_global.main_config, CONFIG_APP_SECTION,
                        "editor_wordcompletion_collect_all_files", TRUE);

    edit_options.filesize_threshold = (char *) "64M";

    rect_init (&r, 0, 0, 24, 80);
    test_edit = edit_init (NULL, &r, vfs_path_from_str ("test-data.txt"), 1);
    memset (&owner, 0, sizeof (owner));
    group_add_widget (&owner, WIDGET (test_edit));
    edit_completion_dialog_show__init ();
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
my_teardown (void)
{
    edit_completion_dialog_show__deinit ();
    edit_clean (test_edit);
    group_remove_widget (test_edit);
    g_free (test_edit);

    mc_config_deinit (mc_global.main_config);

#ifdef HAVE_CHARSET
    free_codepages_list ();
#endif /* HAVE_CHARSET */

    vfs_shut ();

    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

#ifdef HAVE_CHARSET
/* @DataSource("test_autocomplete_ds") */
/* *INDENT-OFF* */
static const struct test_autocomplete_ds
{
    off_t input_position;
    const char *input_system_code_page;
    int input_source_codepage_id;
    const char *input_editor_code_page;
    int input_display_codepage_id;
    const char *input_completed_word;

    int expected_max_width;
    int expected_compl_word_count;
    int input_completed_word_start_pos;
    const char *expected_completed_word;
} test_autocomplete_ds[] =
{
    { /* 0. */
        102,
        "KOI8-R",
        0,
        "UTF-8",
        1,
        "ÑÑŠÐ¹Ñ†ÑƒÐºÐµÐ½",

        16,
        2,
        98,
        "ÑÑŠÐ¹Ñ†ÑƒÐºÐµÐ½"
    },
    { /* 1. */
        138,
        "UTF-8",
        1,
        "KOI8-R",
        0,
        "ÜßÊÃÕËÅÎ",

        8,
        2,
        136,
        "ÜßÊÃÕËÅÎ"
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_autocomplete_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_autocomplete, test_autocomplete_ds)
/* *INDENT-ON* */
{
    /* given */
    edit_completion_dialog_show__return_value = g_strdup (data->input_completed_word);


    mc_global.source_codepage = data->input_source_codepage_id;
    mc_global.display_codepage = data->input_display_codepage_id;
    cp_source = data->input_editor_code_page;
    cp_display = data->input_system_code_page;

    do_set_codepage (0);
    edit_set_codeset (test_edit);

    /* when */
    edit_cursor_move (test_edit, data->input_position);
    edit_complete_word_cmd (test_edit);

    /* then */
    mctest_assert_ptr_eq (edit_completion_dialog_show__edit, test_edit);
    ck_assert_int_eq (g_queue_get_length (edit_completion_dialog_show__compl),
                      data->expected_compl_word_count);
    ck_assert_int_eq (edit_completion_dialog_show__max_width, data->expected_max_width);

    {
        off_t i = 0;
        GString *actual_completed_str;

        actual_completed_str = g_string_new ("");

        while (TRUE)
        {
            int chr;

            chr =
                edit_buffer_get_byte (&test_edit->buffer,
                                      data->input_completed_word_start_pos + i++);
            if (isspace (chr))
                break;
            g_string_append_c (actual_completed_str, chr);
        }
        mctest_assert_str_eq (actual_completed_str->str, data->expected_completed_word);
        g_string_free (actual_completed_str, TRUE);
    }
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_autocomplete_single_ds") */
/* *INDENT-OFF* */
static const struct test_autocomplete_single_ds
{
    off_t input_position;
    const char *input_system_code_page;
    int input_source_codepage_id;
    const char *input_editor_code_page;
    int input_display_codepage_id;

    int input_completed_word_start_pos;

    const char *expected_completed_word;
} test_autocomplete_single_ds[] =
{
    { /* 0. */
        146,
        "UTF-8",
        1,
        "KOI8-R",
        0,

        145,
        "ÆÙ×Á"
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_autocomplete_single_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_autocomplete_single, test_autocomplete_single_ds)
/* *INDENT-ON* */
{
    /* given */
    mc_global.source_codepage = data->input_source_codepage_id;
    mc_global.display_codepage = data->input_display_codepage_id;
    cp_source = data->input_editor_code_page;
    cp_display = data->input_system_code_page;

    do_set_codepage (0);
    edit_set_codeset (test_edit);

    /* when */
    edit_cursor_move (test_edit, data->input_position);
    edit_complete_word_cmd (test_edit);

    /* then */
    {
        off_t i = 0;
        GString *actual_completed_str;

        actual_completed_str = g_string_new ("");

        while (TRUE)
        {
            int chr;

            chr =
                edit_buffer_get_byte (&test_edit->buffer,
                                      data->input_completed_word_start_pos + i++);
            if (isspace (chr))
                break;
            g_string_append_c (actual_completed_str, chr);
        }
        mctest_assert_str_eq (actual_completed_str->str, data->expected_completed_word);
        g_string_free (actual_completed_str, TRUE);
    }
}
/* *INDENT-OFF* */
END_PARAMETRIZED_TEST
/* *INDENT-ON* */


#endif /* HAVE_CHARSET */

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, my_setup, my_teardown);

    /* Add new tests here: *************** */
#ifdef HAVE_CHARSET
    mctest_add_parameterized_test (tc_core, test_autocomplete, test_autocomplete_ds);
    mctest_add_parameterized_test (tc_core, test_autocomplete_single, test_autocomplete_single_ds);
#endif /* HAVE_CHARSET */
    /* *********************************** */

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
