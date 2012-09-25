/*
   Editor dialogs for high level editing commands

   Copyright (C) 2009, 2011, 2012
   The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2009.
   Andrew Borodin, <aborodin@vmail.ru>, 2012.

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

#include <config.h>

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/skin.h"           /* INPUT_COLOR */
#include "lib/tty/key.h"
#include "lib/search.h"
#include "lib/strutil.h"
#include "lib/widget.h"
#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#endif

#include "src/history.h"

#include "src/editor/editwidget.h"
#include "src/editor/etags.h"
#include "src/editor/editcmd_dialogs.h"

#ifdef HAVE_ASPELL
#include "src/editor/spell.h"
#endif

/*** global variables ****************************************************************************/

edit_search_options_t edit_search_options = {
    .type = MC_SEARCH_T_NORMAL,
    .case_sens = FALSE,
    .backwards = FALSE,
    .only_in_selection = FALSE,
    .whole_words = FALSE,
    .all_codepages = FALSE
};

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
editcmd_dialog_raw_key_query_cb (struct Dlg_head *h, Widget * sender,
                                 dlg_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case DLG_KEY:
        h->ret_value = parm;
        dlg_stop (h);
        return MSG_HANDLED;
    default:
        return default_dlg_callback (h, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
editcmd_dialog_search_show (WEdit * edit)
{
    char *search_text;
    size_t num_of_types;
    gchar **list_of_types;
    int dialog_result;

    list_of_types = mc_search_get_types_strings_array (&num_of_types);

    {
        quick_widget_t quick_widgets[] = {
            /* *INDENT-OFF* */
            QUICK_LABELED_INPUT (N_("Enter search string:"), input_label_above,
                                 INPUT_LAST_TEXT, 0, MC_HISTORY_SHARED_SEARCH, &search_text, NULL),
            QUICK_SEPARATOR (TRUE),
            QUICK_START_COLUMNS,
                QUICK_RADIO (num_of_types, (const char **) list_of_types,
                             (int *) &edit_search_options.type, NULL),
            QUICK_NEXT_COLUMN,
                QUICK_CHECKBOX (N_("Cas&e sensitive"), &edit_search_options.case_sens, NULL),
                QUICK_CHECKBOX (N_("&Backwards"), &edit_search_options.backwards, NULL),
                QUICK_CHECKBOX (N_("In se&lection"), &edit_search_options.only_in_selection, NULL),
                QUICK_CHECKBOX (N_("&Whole words"), &edit_search_options.whole_words, NULL),
#ifdef HAVE_CHARSET
                QUICK_CHECKBOX (N_("&All charsets"), &edit_search_options.all_codepages, NULL),
#endif
            QUICK_STOP_COLUMNS,
            QUICK_START_BUTTONS (TRUE, TRUE),
                QUICK_BUTTON (N_("&OK"), B_ENTER, NULL, NULL),
                QUICK_BUTTON (N_("&Find all"), B_USER, NULL, NULL),
                QUICK_BUTTON (N_("&Cancel"), B_CANCEL, NULL, NULL),
            QUICK_END
            /* *INDENT-ON* */
        };

        quick_dialog_t qdlg = {
            -1, -1, 58,
            N_("Search"), "[Input Line Keys]",
            quick_widgets, NULL, NULL
        };

        dialog_result = quick_dialog (&qdlg);
    }

    g_strfreev (list_of_types);

    if ((dialog_result == B_CANCEL) || (search_text == NULL) || (search_text[0] == '\0'))
    {
        g_free (search_text);
        return FALSE;
    }

    if (dialog_result == B_USER)
        search_create_bookmark = TRUE;

#ifdef HAVE_CHARSET
    {
        GString *tmp;

        tmp = str_convert_to_input (search_text);
        if (tmp != NULL)
        {
            g_free (search_text);
            search_text = g_string_free (tmp, FALSE);
        }
    }
#endif

    g_free (edit->last_search_string);
    edit->last_search_string = search_text;
    mc_search_free (edit->search);

    edit->search = mc_search_new (edit->last_search_string, -1);
    if (edit->search != NULL)
    {
        edit->search->search_type = edit_search_options.type;
        edit->search->is_all_charsets = edit_search_options.all_codepages;
        edit->search->is_case_sensitive = edit_search_options.case_sens;
        edit->search->whole_words = edit_search_options.whole_words;
        edit->search->search_fn = edit_search_cmd_callback;
    }

    return (edit->search != NULL);
}

/* --------------------------------------------------------------------------------------------- */

void
editcmd_dialog_replace_show (WEdit * edit, const char *search_default, const char *replace_default,
                             /*@out@ */ char **search_text, /*@out@ */ char **replace_text)
{
    size_t num_of_types;
    gchar **list_of_types;

    if ((search_default == NULL) || (*search_default == '\0'))
        search_default = INPUT_LAST_TEXT;

    list_of_types = mc_search_get_types_strings_array (&num_of_types);

    {
        quick_widget_t quick_widgets[] = {
            /* *INDENT-OFF* */
            QUICK_LABELED_INPUT (N_("Enter search string:"), input_label_above,
                                 search_default, 0, MC_HISTORY_SHARED_SEARCH, search_text, NULL),
            QUICK_LABELED_INPUT (N_("Enter replacement string:"), input_label_above,
                                 replace_default, 0, "replace", replace_text, NULL),
            QUICK_SEPARATOR (TRUE),
            QUICK_START_COLUMNS,
                QUICK_RADIO (num_of_types, (const char **) list_of_types,
                             (int *) &edit_search_options.type, NULL),
            QUICK_NEXT_COLUMN,
                QUICK_CHECKBOX (N_("Cas&e sensitive"), &edit_search_options.case_sens, NULL),
                QUICK_CHECKBOX (N_("&Backwards"), &edit_search_options.backwards, NULL),
                QUICK_CHECKBOX (N_("In se&lection"), &edit_search_options.only_in_selection, NULL),
                QUICK_CHECKBOX (N_("&Whole words"), &edit_search_options.whole_words, NULL),
#ifdef HAVE_CHARSET
                QUICK_CHECKBOX (N_("&All charsets"), &edit_search_options.all_codepages, NULL),
#endif
            QUICK_STOP_COLUMNS,
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END
            /* *INDENT-ON* */
        };

        quick_dialog_t qdlg = {
            -1, -1, 58,
            N_("Replace"), "[Input Line Keys]",
            quick_widgets, NULL, NULL
        };

        if (quick_dialog (&qdlg) != B_CANCEL)
            edit->replace_mode = 0;
        else
        {
            *replace_text = NULL;
            *search_text = NULL;
        }
    }

    g_strfreev (list_of_types);
}

/* --------------------------------------------------------------------------------------------- */

int
editcmd_dialog_replace_prompt_show (WEdit * edit, char *from_text, char *to_text, int xpos,
                                    int ypos)
{
    Widget *w = WIDGET (edit);

    /* dialog size */
    int dlg_height = 10;

    char tmp[BUF_MEDIUM];
    char *repl_from, *repl_to;
    int retval;

    if (xpos == -1)
        xpos = w->x + option_line_state_width + 1;
    if (ypos == -1)
        ypos = w->y + w->lines / 2;
    /* Sometimes menu can hide replaced text. I don't like it */
    if ((edit->curs_row >= ypos - 1) && (edit->curs_row <= ypos + dlg_height - 1))
        ypos -= dlg_height;

    g_snprintf (tmp, sizeof (tmp), "\"%s\"", from_text);
    repl_from = g_strdup (str_trunc (tmp,  - 7));

    g_snprintf (tmp, sizeof (tmp), "\"%s\"", to_text);
    repl_to = g_strdup (str_trunc (tmp,  - 7));

    {
        quick_widget_t quick_widgets[] = {
            /* *INDENT-OFF* */
            QUICK_LABEL (repl_from, NULL),
            QUICK_LABEL (N_("Replace with:"), NULL),
            QUICK_LABEL (repl_to, NULL),
            QUICK_START_BUTTONS (TRUE, TRUE),
                QUICK_BUTTON (N_("&Replace"), B_ENTER, NULL, NULL),
                QUICK_BUTTON (N_("A&ll"), B_REPLACE_ALL, NULL, NULL),
                QUICK_BUTTON (N_("&Skip"), B_SKIP_REPLACE, NULL, NULL),
                QUICK_BUTTON (N_("&Cancel"), B_CANCEL, NULL, NULL),
            QUICK_END
            /* *INDENT-ON* */
        };

        quick_dialog_t qdlg = {
            ypos, xpos, -1,
            N_("Confirm replace"), NULL,
            quick_widgets, NULL, NULL
        };

        retval = quick_dialog (&qdlg);
    }

    g_free (repl_from);
    g_free (repl_to);

    return retval;
}

/* --------------------------------------------------------------------------------------------- */
/* gets a raw key from the keyboard. Passing cancel = 1 draws
   a cancel button thus allowing c-c etc.  Alternatively, cancel = 0
   will return the next key pressed.  ctrl-a (=B_CANCEL), ctrl-g, ctrl-c,
   and Esc are cannot returned */

int
editcmd_dialog_raw_key_query (const char *heading, const char *query, gboolean cancel)
{
    int w;
    struct Dlg_head *raw_dlg;

    w = str_term_width1 (query) + 7;

    raw_dlg =
        create_dlg (TRUE, 0, 0, 7, w, dialog_colors, editcmd_dialog_raw_key_query_cb, NULL,
                    NULL, heading, DLG_CENTER | DLG_TRYUP | DLG_WANT_TAB);
    add_widget (raw_dlg, input_new (3 - cancel, w - 5, input_get_default_colors (),
                                    2, "", 0, INPUT_COMPLETE_DEFAULT));
    add_widget (raw_dlg, label_new (3 - cancel, 2, query));
    if (cancel)
        add_widget (raw_dlg, button_new (4, w / 2 - 5, B_CANCEL, NORMAL_BUTTON, _("Cancel"), 0));
    w = run_dlg (raw_dlg);
    destroy_dlg (raw_dlg);

    return (cancel && (w == ESC_CHAR || w == B_CANCEL)) ? 0 : w;
}

/* --------------------------------------------------------------------------------------------- */
/* let the user select its preferred completion */

void
editcmd_dialog_completion_show (WEdit * edit, int max_len, int word_len,
                                struct selection *compl, int num_compl)
{

    int start_x, start_y, offset, i;
    char *curr = NULL;
    Dlg_head *compl_dlg;
    WListbox *compl_list;
    int compl_dlg_h;            /* completion dialog height */
    int compl_dlg_w;            /* completion dialog width */

    /* calculate the dialog metrics */
    compl_dlg_h = num_compl + 2;
    compl_dlg_w = max_len + 4;
    start_x = edit->curs_col + edit->start_col - (compl_dlg_w / 2) +
        EDIT_TEXT_HORIZONTAL_OFFSET + (edit->fullscreen ? 0 : 1) + option_line_state_width;
    start_y = edit->curs_row + EDIT_TEXT_VERTICAL_OFFSET + (edit->fullscreen ? 0 : 1) + 1;

    if (start_x < 0)
        start_x = 0;
    if (compl_dlg_w > COLS)
        compl_dlg_w = COLS;
    if (compl_dlg_h > LINES - 2)
        compl_dlg_h = LINES - 2;

    offset = start_x + compl_dlg_w - COLS;
    if (offset > 0)
        start_x -= offset;
    offset = start_y + compl_dlg_h - LINES;
    if (offset > 0)
        start_y -= (offset + 1);

    /* create the dialog */
    compl_dlg =
        create_dlg (TRUE, start_y, start_x, compl_dlg_h, compl_dlg_w,
                    dialog_colors, NULL, NULL, "[Completion]", NULL, DLG_COMPACT);

    /* create the listbox */
    compl_list = listbox_new (1, 1, compl_dlg_h - 2, compl_dlg_w - 2, FALSE, NULL);

    /* add the dialog */
    add_widget (compl_dlg, compl_list);

    /* fill the listbox with the completions */
    for (i = num_compl - 1; i >= 0; i--)        /* reverse order */
        listbox_add_item (compl_list, LISTBOX_APPEND_AT_END, 0, (char *) compl[i].text, NULL);

    /* pop up the dialog and apply the choosen completion */
    if (run_dlg (compl_dlg) == B_ENTER)
    {
        listbox_get_current (compl_list, &curr, NULL);
        if (curr)
        {
#ifdef HAVE_CHARSET
            GString *temp, *temp2;
            temp = g_string_new ("");
            for (curr += word_len; *curr; curr++)
                g_string_append_c (temp, *curr);

            temp2 = str_convert_to_input (temp->str);

            if (temp2 && temp2->len)
            {
                g_string_free (temp, TRUE);
                temp = temp2;
            }
            else
                g_string_free (temp2, TRUE);
            for (curr = temp->str; *curr; curr++)
                edit_insert (edit, *curr);
            g_string_free (temp, TRUE);
#else
            for (curr += word_len; *curr; curr++)
                edit_insert (edit, *curr);
#endif
        }
    }

    /* destroy dialog before return */
    destroy_dlg (compl_dlg);
}

/* --------------------------------------------------------------------------------------------- */
/* let the user select where function definition */

void
editcmd_dialog_select_definition_show (WEdit * edit, char *match_expr, int max_len, int word_len,
                                       etags_hash_t * def_hash, int num_lines)
{

    int start_x, start_y, offset, i;
    char *curr = NULL;
    etags_hash_t *curr_def = NULL;
    Dlg_head *def_dlg;
    WListbox *def_list;
    int def_dlg_h;              /* dialog height */
    int def_dlg_w;              /* dialog width */
    char *label_def = NULL;

    (void) word_len;
    /* calculate the dialog metrics */
    def_dlg_h = num_lines + 2;
    def_dlg_w = max_len + 4;
    start_x = edit->curs_col + edit->start_col - (def_dlg_w / 2) +
        EDIT_TEXT_HORIZONTAL_OFFSET + (edit->fullscreen ? 0 : 1) + option_line_state_width;
    start_y = edit->curs_row + EDIT_TEXT_VERTICAL_OFFSET + (edit->fullscreen ? 0 : 1) + 1;

    if (start_x < 0)
        start_x = 0;
    if (def_dlg_w > COLS)
        def_dlg_w = COLS;
    if (def_dlg_h > LINES - 2)
        def_dlg_h = LINES - 2;

    offset = start_x + def_dlg_w - COLS;
    if (offset > 0)
        start_x -= offset;
    offset = start_y + def_dlg_h - LINES;
    if (offset > 0)
        start_y -= (offset + 1);

    /* create the dialog */
    def_dlg = create_dlg (TRUE, start_y, start_x, def_dlg_h, def_dlg_w,
                          dialog_colors, NULL, NULL, "[Definitions]", match_expr, DLG_COMPACT);

    /* create the listbox */
    def_list = listbox_new (1, 1, def_dlg_h - 2, def_dlg_w - 2, FALSE, NULL);

    /* add the dialog */
    add_widget (def_dlg, def_list);

    /* fill the listbox with the completions */
    for (i = 0; i < num_lines; i++)
    {
        label_def =
            g_strdup_printf ("%s -> %s:%ld", def_hash[i].short_define, def_hash[i].filename,
                             def_hash[i].line);
        listbox_add_item (def_list, LISTBOX_APPEND_AT_END, 0, label_def, &def_hash[i]);
        g_free (label_def);
    }

    /* pop up the dialog and apply the choosen completion */
    if (run_dlg (def_dlg) == B_ENTER)
    {
        char *tmp_curr_def = (char *) curr_def;
        int do_moveto = 0;

        listbox_get_current (def_list, &curr, (void **) &tmp_curr_def);
        curr_def = (etags_hash_t *) tmp_curr_def;
        if (edit->modified)
        {
            if (!edit_query_dialog2
                (_("Warning"),
                 _("Current text was modified without a file save.\n"
                   "Continue discards these changes."), _("C&ontinue"), _("&Cancel")))
            {
                edit->force |= REDRAW_COMPLETELY;
                do_moveto = 1;
            }
        }
        else
        {
            do_moveto = 1;
        }

        if (curr && do_moveto)
        {
            if (edit_stack_iterator + 1 < MAX_HISTORY_MOVETO)
            {
                vfs_path_free (edit_history_moveto[edit_stack_iterator].filename_vpath);
                if (edit->dir_vpath != NULL)
                {
                    edit_history_moveto[edit_stack_iterator].filename_vpath =
                        vfs_path_append_vpath_new (edit->dir_vpath, edit->filename_vpath, NULL);
                }
                else
                {
                    edit_history_moveto[edit_stack_iterator].filename_vpath =
                        vfs_path_clone (edit->filename_vpath);
                }
                edit_history_moveto[edit_stack_iterator].line = edit->start_line +
                    edit->curs_row + 1;
                edit_stack_iterator++;
                vfs_path_free (edit_history_moveto[edit_stack_iterator].filename_vpath);
                edit_history_moveto[edit_stack_iterator].filename_vpath =
                    vfs_path_from_str ((char *) curr_def->fullpath);
                edit_history_moveto[edit_stack_iterator].line = curr_def->line;
                edit_reload_line (edit, edit_history_moveto[edit_stack_iterator].filename_vpath,
                                  edit_history_moveto[edit_stack_iterator].line);
            }
        }
    }

    /* clear definition hash */
    for (i = 0; i < MAX_DEFINITIONS; i++)
    {
        g_free (def_hash[i].filename);
    }

    /* destroy dialog before return */
    destroy_dlg (def_dlg);
}

/* --------------------------------------------------------------------------------------------- */
