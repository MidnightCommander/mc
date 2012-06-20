/*
   Editor dialogs for high level editing commands

   Copyright (C) 2009, 2011
   The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2009.

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

#define SEARCH_DLG_WIDTH 58
#define SEARCH_DLG_MIN_HEIGHT 13
#define SEARCH_DLG_HEIGHT_SUPPLY 3

#define REPLACE_DLG_WIDTH 58
#define REPLACE_DLG_MIN_HEIGHT 17
#define REPLACE_DLG_HEIGHT_SUPPLY 5

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

void
editcmd_dialog_replace_show (WEdit * edit, const char *search_default, const char *replace_default,
                             /*@out@ */ char **search_text, /*@out@ */ char **replace_text)
{
    if ((search_default == NULL) || (*search_default == '\0'))
        search_default = INPUT_LAST_TEXT;

    {
        size_t num_of_types;
        gchar **list_of_types = mc_search_get_types_strings_array (&num_of_types);
        int REPLACE_DLG_HEIGHT = REPLACE_DLG_MIN_HEIGHT + num_of_types - REPLACE_DLG_HEIGHT_SUPPLY;

        QuickWidget quick_widgets[] = {
            /*  0 */ QUICK_BUTTON (6, 10, 13, REPLACE_DLG_HEIGHT, N_("&Cancel"), B_CANCEL, NULL),
            /*  1 */ QUICK_BUTTON (2, 10, 13, REPLACE_DLG_HEIGHT, N_("&OK"), B_ENTER, NULL),
#ifdef HAVE_CHARSET
            /*  2 */ QUICK_CHECKBOX (33, REPLACE_DLG_WIDTH, 11, REPLACE_DLG_HEIGHT,
                                     N_("&All charsets"),
                                     &edit_search_options.all_codepages),
#endif
            /*  3 */ QUICK_CHECKBOX (33, REPLACE_DLG_WIDTH, 10, REPLACE_DLG_HEIGHT,
                                     N_("&Whole words"),
                                     &edit_search_options.whole_words),
            /*  4 */ QUICK_CHECKBOX (33, REPLACE_DLG_WIDTH, 9, REPLACE_DLG_HEIGHT,
                                     N_("In se&lection"),
                                     &edit_search_options.only_in_selection),
            /*  5 */ QUICK_CHECKBOX (33, REPLACE_DLG_WIDTH, 8, REPLACE_DLG_HEIGHT, N_("&Backwards"),
                                     &edit_search_options.backwards),
            /*  6 */ QUICK_CHECKBOX (33, REPLACE_DLG_WIDTH, 7, REPLACE_DLG_HEIGHT,
                                     N_("Cas&e sensitive"),
                                     &edit_search_options.case_sens),
            /*  7 */ QUICK_RADIO (3, REPLACE_DLG_WIDTH, 7, REPLACE_DLG_HEIGHT,
                                  num_of_types, (const char **) list_of_types,
                                  (int *) &edit_search_options.type),
            /*  8 */ QUICK_LABEL (3, REPLACE_DLG_WIDTH, 4, REPLACE_DLG_HEIGHT,
                                  N_("Enter replacement string:")),
            /*  9 */ QUICK_INPUT (3, REPLACE_DLG_WIDTH, 5, REPLACE_DLG_HEIGHT,
                                  replace_default, REPLACE_DLG_WIDTH - 6, 0, "replace",
                                  replace_text),
            /* 10 */ QUICK_LABEL (3, REPLACE_DLG_WIDTH, 2, REPLACE_DLG_HEIGHT,
                                  N_("Enter search string:")),
            /* 11 */ QUICK_INPUT (3, REPLACE_DLG_WIDTH, 3, REPLACE_DLG_HEIGHT,
                                  search_default, REPLACE_DLG_WIDTH - 6, 0,
                                  MC_HISTORY_SHARED_SEARCH, search_text),
            QUICK_END
        };

        QuickDialog Quick_input = {
            REPLACE_DLG_WIDTH, REPLACE_DLG_HEIGHT, -1, -1, N_("Replace"),
            "[Input Line Keys]", quick_widgets, NULL, NULL, FALSE
        };

        if (quick_dialog (&Quick_input) != B_CANCEL)
        {
            edit->replace_mode = 0;
        }
        else
        {
            *replace_text = NULL;
            *search_text = NULL;
        }

        g_strfreev (list_of_types);
    }
}

/* --------------------------------------------------------------------------------------------- */

gboolean
editcmd_dialog_search_show (WEdit * edit)
{
    char *search_text;

    size_t num_of_types;
    gchar **list_of_types = mc_search_get_types_strings_array (&num_of_types);
    int SEARCH_DLG_HEIGHT = SEARCH_DLG_MIN_HEIGHT + num_of_types - SEARCH_DLG_HEIGHT_SUPPLY;
    size_t i;

    int dialog_result;

    QuickWidget quick_widgets[] = {
        /* 0 */
        QUICK_BUTTON (6, 10, 11, SEARCH_DLG_HEIGHT, N_("&Cancel"), B_CANCEL, NULL),
        /* 1 */
        QUICK_BUTTON (4, 10, 11, SEARCH_DLG_HEIGHT, N_("&Find all"), B_USER, NULL),
        /* 2 */
        QUICK_BUTTON (2, 10, 11, SEARCH_DLG_HEIGHT, N_("&OK"), B_ENTER, NULL),
#ifdef HAVE_CHARSET
        /* 3 */
        QUICK_CHECKBOX (33, SEARCH_DLG_WIDTH, 9, SEARCH_DLG_HEIGHT, N_("&All charsets"),
                        &edit_search_options.all_codepages),
#endif
        /* 4 */
        QUICK_CHECKBOX (33, SEARCH_DLG_WIDTH, 8, SEARCH_DLG_HEIGHT, N_("&Whole words"),
                        &edit_search_options.whole_words),
        /* 5 */
        QUICK_CHECKBOX (33, SEARCH_DLG_WIDTH, 7, SEARCH_DLG_HEIGHT, N_("In se&lection"),
                        &edit_search_options.only_in_selection),
        /* 6 */
        QUICK_CHECKBOX (33, SEARCH_DLG_WIDTH, 6, SEARCH_DLG_HEIGHT, N_("&Backwards"),
                        &edit_search_options.backwards),
        /* 7 */
        QUICK_CHECKBOX (33, SEARCH_DLG_WIDTH, 5, SEARCH_DLG_HEIGHT, N_("Cas&e sensitive"),
                        &edit_search_options.case_sens),
        /* 8 */
        QUICK_RADIO (3, SEARCH_DLG_WIDTH, 5, SEARCH_DLG_HEIGHT,
                     num_of_types, (const char **) list_of_types,
                     (int *) &edit_search_options.type),
        /* 9 */
        QUICK_INPUT (3, SEARCH_DLG_WIDTH, 3, SEARCH_DLG_HEIGHT,
                     INPUT_LAST_TEXT, SEARCH_DLG_WIDTH - 6, 0,
                     MC_HISTORY_SHARED_SEARCH, &search_text),
        /* 10 */
        QUICK_LABEL (3, SEARCH_DLG_WIDTH, 2, SEARCH_DLG_HEIGHT, N_("Enter search string:")),
        QUICK_END
    };

#ifdef HAVE_CHARSET
    size_t last_checkbox = 7;
#else
    size_t last_checkbox = 6;
#endif

    QuickDialog Quick_input = {
        SEARCH_DLG_WIDTH, SEARCH_DLG_HEIGHT, -1, -1, N_("Search"),
        "[Input Line Keys]", quick_widgets, NULL, NULL, TRUE
    };

#ifdef ENABLE_NLS
    char **list_of_types_nls;

    /* header title */
    Quick_input.title = _(Quick_input.title);
    /* buttons */
    for (i = 0; i < 3; i++)
        quick_widgets[i].u.button.text = _(quick_widgets[i].u.button.text);
    /* checkboxes */
    for (i = 3; i <= last_checkbox; i++)
        quick_widgets[i].u.checkbox.text = _(quick_widgets[i].u.checkbox.text);
    /* label */
    quick_widgets[10].u.label.text = _(quick_widgets[10].u.label.text);

    /* radiobuttons */
    /* create copy of radio items to avoid memory leak */
    list_of_types_nls = g_new0 (char *, num_of_types + 1);
    for (i = 0; i < num_of_types; i++)
        list_of_types_nls[i] = g_strdup (_(list_of_types[i]));
    g_strfreev (list_of_types);
    list_of_types = list_of_types_nls;
    quick_widgets[last_checkbox + 1].u.radio.items = (const char **) list_of_types;
#endif

    /* calculate widget coordinates */
    {
        int len = 0;
        int dlg_width;
        gchar **radio = list_of_types;
        int b0_len, b1_len, b2_len;
        const int button_gap = 2;

        /* length of radiobuttons */
        while (*radio != NULL)
        {
            len = max (len, str_term_width1 (*radio));
            radio++;
        }
        /* length of checkboxes */
        for (i = 3; i <= last_checkbox; i++)
            len = max (len, str_term_width1 (quick_widgets[i].u.checkbox.text) + 4);

        /* preliminary dialog width */
        dlg_width = max (len * 2, str_term_width1 (Quick_input.title)) + 4;

        /* length of buttons */
        b0_len = str_term_width1 (quick_widgets[0].u.button.text) + 3;
        b1_len = str_term_width1 (quick_widgets[1].u.button.text) + 3;
        b2_len = str_term_width1 (quick_widgets[2].u.button.text) + 5;  /* default button */
        len = b0_len + b1_len + b2_len + button_gap * 2;

        /* dialog width */
        Quick_input.xlen = max (SEARCH_DLG_WIDTH, max (dlg_width, len + 6));

        /* correct widget coordinates */
        for (i = 0; i < sizeof (quick_widgets) / sizeof (quick_widgets[0]); i++)
            quick_widgets[i].x_divisions = Quick_input.xlen;

        /* checkbox positions */
        for (i = 3; i <= last_checkbox; i++)
            quick_widgets[i].relative_x = Quick_input.xlen / 2 + 2;
        /* input length */
        quick_widgets[last_checkbox + 2].u.input.len = Quick_input.xlen - 6;
        /* button positions */
        quick_widgets[2].relative_x = Quick_input.xlen / 2 - len / 2;
        quick_widgets[1].relative_x = quick_widgets[2].relative_x + b2_len + button_gap;
        quick_widgets[0].relative_x = quick_widgets[1].relative_x + b1_len + button_gap;
    }

    dialog_result = quick_dialog (&Quick_input);

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

int
editcmd_dialog_replace_prompt_show (WEdit * edit, char *from_text, char *to_text, int xpos,
                                    int ypos)
{
    /* dialog sizes */
    int dlg_height = 9;
    int dlg_width = 8;

    int retval;
    int i;
    int btn_pos;

    char *repl_from, *repl_to;
    char tmp[BUF_MEDIUM];

    QuickWidget quick_widgets[] = {
        /* 0 */ QUICK_BUTTON (44, dlg_width, 6, dlg_height, N_("&Cancel"), B_CANCEL, NULL),
        /* 1 */ QUICK_BUTTON (29, dlg_width, 6, dlg_height, N_("&Skip"), B_SKIP_REPLACE, NULL),
        /* 2 */ QUICK_BUTTON (21, dlg_width, 6, dlg_height, N_("A&ll"), B_REPLACE_ALL, NULL),
        /* 3 */ QUICK_BUTTON (4, dlg_width, 6, dlg_height, N_("&Replace"), B_ENTER, NULL),
        /* 4 */ QUICK_LABEL (3, dlg_width, 2, dlg_height, NULL),
        /* 5 */ QUICK_LABEL (3, dlg_width, 3, dlg_height, N_("Replace with:")),
        /* 6 */ QUICK_LABEL (3, dlg_width, 4, dlg_height, NULL),
        QUICK_END
    };

#ifdef ENABLE_NLS
    for (i = 0; i < 4; i++)
        quick_widgets[i].u.button.text = _(quick_widgets[i].u.button.text);
#endif

    /* calculate button positions */
    btn_pos = 4;

    for (i = 3; i > -1; i--)
    {
        quick_widgets[i].relative_x = btn_pos;
        btn_pos += str_term_width1 (quick_widgets[i].u.button.text) + 5;
        if (i == 3)             /* default button */
            btn_pos += 2;
    }

    dlg_width = btn_pos + 2;

    /* correct widget coordinates */
    for (i = 0; i < 7; i++)
        quick_widgets[i].x_divisions = dlg_width;

    g_snprintf (tmp, sizeof (tmp), "\"%s\"", from_text);
    repl_from = g_strdup (str_fit_to_term (tmp, dlg_width - 7, J_LEFT));

    g_snprintf (tmp, sizeof (tmp), "\"%s\"", to_text);
    repl_to = g_strdup (str_fit_to_term (tmp, dlg_width - 7, J_LEFT));

    quick_widgets[4].u.label.text = repl_from;
    quick_widgets[6].u.label.text = repl_to;

    if (xpos == -1)
        xpos = (WIDGET (edit)->cols - dlg_width) / 2;

    if (ypos == -1)
        ypos = WIDGET (edit)->lines * 2 / 3;

    {
        QuickDialog Quick_input = {
            dlg_width, dlg_height, 0, 0, N_("Confirm replace"),
            "[Input Line Keys]", quick_widgets, NULL, NULL, FALSE
        };

        /* Sometimes menu can hide replaced text. I don't like it */
        if ((edit->curs_row >= ypos - 1) && (edit->curs_row <= ypos + dlg_height - 1))
            ypos -= dlg_height;

        Quick_input.ypos = ypos;
        Quick_input.xpos = xpos;

        retval = quick_dialog (&Quick_input);
        g_free (repl_from);
        g_free (repl_to);

        return retval;
    }
}

/* --------------------------------------------------------------------------------------------- */
