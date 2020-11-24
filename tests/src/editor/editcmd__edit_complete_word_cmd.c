/*
   src/editor - tests for edit_complete_word_cmd() function

   Copyright (C) 2013-2020
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2013

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

#include "lib/timer.h"
#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#endif
#include "lib/strutil.h"

#include "src/vfs/local/local.c"
#ifdef HAVE_CHARSET
#include "src/selcodepage.h"
#endif
#include "src/editor/editwidget.h"
#include "src/editor/editcmd_dialogs.h"


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
static const WEdit *editcmd_dialog_completion_show__edit;
/* @CapturedValue */
static int editcmd_dialog_completion_show__max_len;
/* @CapturedValue */
static GString **editcmd_dialog_completion_show__compl;
/* @CapturedValue */
static int editcmd_dialog_completion_show__num_compl;

/* @ThenReturnValue */
static char *editcmd_dialog_completion_show__return_value;

/* @Mock */
char *
editcmd_dialog_completion_show (const WEdit * edit, int max_len, GString ** compl, int num_compl)
{

    editcmd_dialog_completion_show__edit = edit;
    editcmd_dialog_completion_show__max_len = max_len;
    editcmd_dialog_completion_show__num_compl = num_compl;

    {
        int iterator;

        editcmd_dialog_completion_show__compl = g_new0 (GString *, num_compl);

        for (iterator = 0; iterator < editcmd_dialog_completion_show__num_compl; iterator++)
            editcmd_dialog_completion_show__compl[iterator] =
                g_string_new_len (compl[iterator]->str, compl[iterator]->len);
    }

    return editcmd_dialog_completion_show__return_value;
}

static void
editcmd_dialog_completion_show__init (void)
{
    editcmd_dialog_completion_show__edit = NULL;
    editcmd_dialog_completion_show__max_len = 0;
    editcmd_dialog_completion_show__compl = NULL;
    editcmd_dialog_completion_show__num_compl = 0;
    editcmd_dialog_completion_show__return_value = NULL;
}

static void
editcmd_dialog_completion_show__deinit (void)
{
    if (editcmd_dialog_completion_show__compl != NULL)
    {
        int iterator;

        for (iterator = 0; iterator < editcmd_dialog_completion_show__num_compl; iterator++)
            g_string_free (editcmd_dialog_completion_show__compl[iterator], TRUE);

        g_free (editcmd_dialog_completion_show__compl);
    }
}


/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
my_setup (void)
{
    mc_global.timer = mc_timer_new ();
    str_init_strings (NULL);

    vfs_init ();
    vfs_init_localfs ();
    vfs_setup_work_dir ();

#ifdef HAVE_CHARSET
    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;
    load_codepages_list ();
#endif /* HAVE_CHARSET */

    option_filesize_threshold = (char *) "64M";

    test_edit = edit_init (NULL, 0, 0, 24, 80, vfs_path_from_str ("test-data.txt"), 1);
    editcmd_dialog_completion_show__init ();
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
my_teardown (void)
{
    editcmd_dialog_completion_show__deinit ();
    edit_clean (test_edit);
    g_free (test_edit);

#ifdef HAVE_CHARSET
    free_codepages_list ();
#endif /* HAVE_CHARSET */

    vfs_shut ();

    str_uninit_strings ();
    mc_timer_destroy (mc_global.timer);
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

    int expected_max_len;
    int expected_compl_word_count;
    int input_completed_word_start_pos;
    const char *expected_completed_word;
} test_autocomplete_ds[] =
{
    { /* 0. */
        111,
        "KOI8-R",
        0,
        "UTF-8",
        1,
        "эъйцукен",

        16,
        2,
        107,
        "эъйцукен"
    },
    { /* 1. */
        147,
        "UTF-8",
        1,
        "KOI8-R",
        0,
        "��������",

        8,
        2,
        145,
        "��������"
    },
};
/* *INDENT-ON* */

/* @Test(dataSource = "test_autocomplete_ds") */
/* *INDENT-OFF* */
START_PARAMETRIZED_TEST (test_autocomplete, test_autocomplete_ds)
/* *INDENT-ON* */
{
    /* given */
    editcmd_dialog_completion_show__return_value = g_strdup (data->input_completed_word);


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
    mctest_assert_ptr_eq (editcmd_dialog_completion_show__edit, test_edit);
    mctest_assert_int_eq (editcmd_dialog_completion_show__num_compl,
                          data->expected_compl_word_count);
    mctest_assert_int_eq (editcmd_dialog_completion_show__max_len, data->expected_max_len);

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
        155,
        "UTF-8",
        1,
        "KOI8-R",
        0,

        154,
        "����"
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
    int number_failed;

    Suite *s = suite_create (TEST_SUITE_NAME);
    TCase *tc_core = tcase_create ("Core");
    SRunner *sr;

    tcase_add_checked_fixture (tc_core, my_setup, my_teardown);

    /* Add new tests here: *************** */
#ifdef HAVE_CHARSET
    mctest_add_parameterized_test (tc_core, test_autocomplete, test_autocomplete_ds);
    mctest_add_parameterized_test (tc_core, test_autocomplete_single, test_autocomplete_single_ds);
#endif /* HAVE_CHARSET */
    /* *********************************** */

    suite_add_tcase (s, tc_core);
    sr = srunner_create (s);
    srunner_set_log (sr, "edit_complete_word_cmd.log");
    srunner_run_all (sr, CK_ENV);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* --------------------------------------------------------------------------------------------- */
