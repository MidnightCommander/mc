/*
   Editor dialogs for high level editing commands

   Copyright (C) 2009 The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2009.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA.
 */

#include <config.h>

#include "../src/global.h"

#include "edit-widget.h"

#include "../src/strutil.h"
#include "../src/tty.h"
#include "../src/widget.h"
#include "../src/color.h"
#include "../src/wtools.h"
#include "../src/dialog.h"      /* do_refresh() */
#include "../src/key.h"
#include "../src/main.h"
#include "../src/search/search.h"

#include "../edit/etags.h"
#include "editcmd_dialogs.h"


/*** global variables **************************************************/

/*** file scope macro definitions **************************************/

#define SEARCH_DLG_WIDTH 58
#define SEARCH_DLG_HEIGHT 12
#define REPLACE_DLG_WIDTH 58
#define REPLACE_DLG_HEIGHT 15

/*** file scope type declarations **************************************/

/*** file scope variables **********************************************/

static const char *replace_mode_str[] = { N_("pro&Mpt on replace"), N_("replace &All") };

static QuickWidget *editcmd_dialog__widgets_for_searches_cb = NULL;
static int editcmd_dialog__widgets_for_searches_index = 0;

static mc_search_type_t editcmd_dialog__type_of_search = -1;

/*** file scope functions **********************************************/

static cb_ret_t
editcmd_dialog_raw_key_query_cb (struct Dlg_head *h, dlg_msg_t msg, int parm)
{
    switch (msg) {
    case DLG_KEY:
        h->running = 0;
        h->ret_value = parm;
        return MSG_HANDLED;
    default:
        return default_dlg_callback (h, msg, parm);
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
edit_replace__select_search_type_cb (int action)
{
    int i = 0;
    gboolean fnd = FALSE;
    mc_search_type_str_t *type_str;
    mc_search_type_str_t *types_str = mc_search_types_list_get ();


    (void) action;

    Listbox *listbox =
        create_listbox_window_delta (0, 1, 15, 4, _(" Search type "), "[Search Types]");

    type_str = types_str;
    while (type_str->str) {
        LISTBOX_APPEND_TEXT (listbox, 0, type_str->str, NULL);
        if (type_str->type == editcmd_dialog__type_of_search)
            fnd = TRUE;
        if (!fnd)
            i++;

        type_str++;
    }
    listbox_select_by_number (listbox->list, i);

    i = run_listbox (listbox);
    if (i < 0)
        return 0;

    editcmd_dialog__type_of_search = types_str[i].type;

    label_set_text ((WLabel *)
                    editcmd_dialog__widgets_for_searches_cb
                    [editcmd_dialog__widgets_for_searches_index].widget, types_str[i].str);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

char *
editcmd_dialog__get_search_type_label (void)
{
    mc_search_type_str_t *types_str;
    if ((int) editcmd_dialog__type_of_search < 0)
        editcmd_dialog__type_of_search = 0;

    types_str = mc_search_types_list_get ();

    return types_str[editcmd_dialog__type_of_search].str;
}

/*** public functions **************************************************/

void
editcmd_dialog_replace_show (WEdit * edit, const char *search_default, const char *replace_default,
                             /*@out@ */ char **search_text, /*@out@ */ char **replace_text)
{
    int treplace_backwards = edit->replace_backwards;
    int treplace_case = edit->replace_case;
    int treplace_mode = edit->replace_mode;
    int dialog_result;

    char *search_type_label = editcmd_dialog__get_search_type_label ();
    char *search_button_caption = _("&Select");
    int search_button_caption_len = str_term_width1 (search_button_caption);



    QuickWidget quick_widgets[] = {

        {quick_button, 6, 10, 12, REPLACE_DLG_HEIGHT, N_("&Cancel"), 0, B_CANCEL,
         0, 0, NULL, NULL, NULL},

        {quick_button, 2, 10, 12, REPLACE_DLG_HEIGHT, N_("&OK"), 0, B_ENTER,
         0, 0, NULL, NULL, NULL},



        {quick_radio, 33, REPLACE_DLG_WIDTH, 9, REPLACE_DLG_HEIGHT, "", 2, 0,
         &treplace_mode, const_cast (char **, replace_mode_str), NULL, NULL, NULL},

        {quick_checkbox, 4, REPLACE_DLG_WIDTH, 10, REPLACE_DLG_HEIGHT, N_("&Backwards"), 0, 0,
         &treplace_backwards, 0, NULL, NULL, NULL},

        {quick_checkbox, 4, REPLACE_DLG_WIDTH, 9, REPLACE_DLG_HEIGHT, N_("case &Sensitive"), 0, 0,
         &treplace_case, 0, NULL, NULL, NULL},


        {quick_label, 2, REPLACE_DLG_WIDTH, 6, REPLACE_DLG_HEIGHT, N_(" Enter search type:"),
         0, 0, 0, 0, 0, NULL, NULL},

        {quick_label, 3 + search_button_caption_len + 5, REPLACE_DLG_WIDTH, 7, REPLACE_DLG_HEIGHT,
         search_type_label, 52, 0, 0, 0, NULL, NULL, NULL},

        {quick_button, 3, REPLACE_DLG_WIDTH, 7, REPLACE_DLG_HEIGHT, search_button_caption, 0,
         B_USER, 0, 0, NULL, &edit_replace__select_search_type_cb, NULL},


        {quick_label, 2, REPLACE_DLG_WIDTH, 4, REPLACE_DLG_HEIGHT, N_(" Enter replacement string:"),
         0, 0, 0, 0, 0, NULL, NULL},

        {quick_input, 3, REPLACE_DLG_WIDTH, 5, REPLACE_DLG_HEIGHT, replace_default, 52, 0,
         0, replace_text, "edit-replace", NULL, NULL},


        {quick_label, 2, REPLACE_DLG_WIDTH, 2, REPLACE_DLG_HEIGHT, N_(" Enter search string:"), 0,
         0, 0, 0, 0, NULL, NULL},

        {quick_input, 3, REPLACE_DLG_WIDTH, 3, REPLACE_DLG_HEIGHT, search_default, 52, 0,
         0, search_text, "edit-search", NULL, NULL},


        NULL_QuickWidget
    };


    QuickDialog Quick_input = {
        REPLACE_DLG_WIDTH,
        REPLACE_DLG_HEIGHT,
        -1,
        0,
        N_(" Replace "),
        "[Input Line Keys]",
        quick_widgets,
        0
    };

    editcmd_dialog__widgets_for_searches_cb = quick_widgets;
    editcmd_dialog__widgets_for_searches_index = 6;

    dialog_result = quick_dialog (&Quick_input);

    if (dialog_result != B_CANCEL) {
        edit->search_type = editcmd_dialog__type_of_search;
        edit->replace_mode = treplace_mode;
        edit->replace_backwards = treplace_backwards;
        edit->replace_case = treplace_case;
        return;
    } else {
        *replace_text = NULL;
        *search_text = NULL;
        return;
    }
}

/* --------------------------------------------------------------------------------------------- */

void
editcmd_dialog_search_show (WEdit * edit, char **search_text)
{
    int treplace_mode = edit->replace_mode;
    int treplace_case = edit->replace_case;
    int treplace_backwards = edit->replace_backwards;

    char *search_type_label = editcmd_dialog__get_search_type_label ();
    char *search_button_caption = _("&Select");
    int search_button_caption_len = str_term_width1 (search_button_caption);



    QuickWidget quick_widgets[] = {
        {quick_button, 6, 10, 9, SEARCH_DLG_HEIGHT, N_("&Cancel"), 0, B_CANCEL, 0,
         0, NULL, NULL, NULL},
        {quick_button, 2, 10, 9, SEARCH_DLG_HEIGHT, N_("&OK"), 0, B_ENTER, 0,
         0, NULL, NULL, NULL},

        {quick_checkbox, 33, SEARCH_DLG_WIDTH, 7, SEARCH_DLG_HEIGHT, N_("&Backwards"), 0, 0,
         &treplace_backwards, 0, NULL, NULL, NULL},
        {quick_checkbox, 4, SEARCH_DLG_WIDTH, 7, SEARCH_DLG_HEIGHT, N_("case &Sensitive"), 0, 0,
         &treplace_case, 0, NULL, NULL, NULL},


        {quick_label, 2, REPLACE_DLG_WIDTH, 6, REPLACE_DLG_HEIGHT, N_(" Enter search type:"),
         0, 0, 0, 0, 0, NULL, NULL},

        {quick_label, 3 + search_button_caption_len + 5, REPLACE_DLG_WIDTH, 7, REPLACE_DLG_HEIGHT,
         search_type_label, 52, 0, 0, 0, NULL, NULL, NULL},

        {quick_button, 3, REPLACE_DLG_WIDTH, 7, REPLACE_DLG_HEIGHT, search_button_caption, 0,
         B_USER, 0, 0, NULL, &edit_replace__select_search_type_cb, NULL},


        {quick_input, 3, SEARCH_DLG_WIDTH, 3, SEARCH_DLG_HEIGHT, *search_text, 52, 0, 0,
         search_text, "edit-search", NULL, NULL},
        {quick_label, 2, SEARCH_DLG_WIDTH, 2, SEARCH_DLG_HEIGHT, N_(" Enter search string:"), 0, 0,
         0, 0, 0, NULL, NULL},

        NULL_QuickWidget
    };


    QuickDialog Quick_input = { SEARCH_DLG_WIDTH, SEARCH_DLG_HEIGHT, -1, 0, N_("Search"),
        "[Input Line Keys]", quick_widgets, 0
    };

    editcmd_dialog__widgets_for_searches_cb = quick_widgets;
    editcmd_dialog__widgets_for_searches_index = 5;


    if (quick_dialog (&Quick_input) != B_CANCEL) {
        edit->search_type = editcmd_dialog__type_of_search;
        edit->replace_mode = treplace_mode;
        edit->replace_backwards = treplace_backwards;
        edit->replace_case = treplace_case;
    } else {
        *search_text = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* gets a raw key from the keyboard. Passing cancel = 1 draws
   a cancel button thus allowing c-c etc.  Alternatively, cancel = 0
   will return the next key pressed.  ctrl-a (=B_CANCEL), ctrl-g, ctrl-c,
   and Esc are cannot returned */

int
editcmd_dialog_raw_key_query (const char *heading, const char *query, int cancel)
{
    int w = str_term_width1 (query) + 7;
    struct Dlg_head *raw_dlg =
        create_dlg (0, 0, 7, w, dialog_colors, editcmd_dialog_raw_key_query_cb,
                    NULL, heading,
                    DLG_CENTER | DLG_TRYUP | DLG_WANT_TAB);
    add_widget (raw_dlg,
                input_new (3 - cancel, w - 5, INPUT_COLOR, 2, "", 0, INPUT_COMPLETE_DEFAULT));
    add_widget (raw_dlg, label_new (3 - cancel, 2, query));
    if (cancel)
        add_widget (raw_dlg, button_new (4, w / 2 - 5, B_CANCEL, NORMAL_BUTTON, _("Cancel"), 0));
    run_dlg (raw_dlg);
    w = raw_dlg->ret_value;
    destroy_dlg (raw_dlg);
    if (cancel) {
        if (w == XCTRL ('g') || w == XCTRL ('c') || w == ESC_CHAR || w == B_CANCEL)
            return 0;
    }

    return w;
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
    start_x = edit->curs_col + edit->start_col - (compl_dlg_w / 2);
    start_y = edit->curs_row + EDIT_TEXT_VERTICAL_OFFSET + 1;

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
        create_dlg (start_y, start_x, compl_dlg_h, compl_dlg_w,
                    dialog_colors, NULL, "[Completion]", NULL, DLG_COMPACT);

    /* create the listbox */
    compl_list = listbox_new (1, 1, compl_dlg_h - 2, compl_dlg_w - 2, NULL);

    /* add the dialog */
    add_widget (compl_dlg, compl_list);

    /* fill the listbox with the completions */
    for (i = 0; i < num_compl; i++)
        listbox_add_item (compl_list, LISTBOX_APPEND_AT_END, 0, (char *) compl[i].text, NULL);

    /* pop up the dialog */
    run_dlg (compl_dlg);

    /* apply the choosen completion */
    if (compl_dlg->ret_value == B_ENTER) {
        listbox_get_current (compl_list, &curr, NULL);
        if (curr)
            for (curr += word_len; *curr; curr++)
                edit_insert (edit, *curr);
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
    start_x = edit->curs_col + edit->start_col - (def_dlg_w / 2);
    start_y = edit->curs_row + EDIT_TEXT_VERTICAL_OFFSET + 1;

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
    def_dlg = create_dlg (start_y, start_x, def_dlg_h, def_dlg_w,
                          dialog_colors, NULL, "[Definitions]", match_expr, DLG_COMPACT);

    /* create the listbox */
    def_list = listbox_new (1, 1, def_dlg_h - 2, def_dlg_w - 2, NULL);

    /* add the dialog */
    add_widget (def_dlg, def_list);


    /* fill the listbox with the completions */
    for (i = 0; i < num_lines; i++) {
        label_def =
            g_strdup_printf ("%s -> %s:%ld", def_hash[i].short_define, def_hash[i].filename,
                             def_hash[i].line);
        listbox_add_item (def_list, LISTBOX_APPEND_AT_END, 0, label_def, &def_hash[i]);
        g_free (label_def);
    }
    /* pop up the dialog */
    run_dlg (def_dlg);

    /* apply the choosen completion */
    if (def_dlg->ret_value == B_ENTER) {
        char *tmp_curr_def = (char *) curr_def;
        int do_moveto = 0;

        listbox_get_current (def_list, &curr, &tmp_curr_def);
        curr_def = (etags_hash_t *) tmp_curr_def;
        if (edit->modified) {
            if (!edit_query_dialog2
                (_("Warning"),
                 _(" Current text was modified without a file save. \n"
                   " Continue discards these changes. "), _("C&ontinue"), _("&Cancel"))) {
                edit->force |= REDRAW_COMPLETELY;
                do_moveto = 1;
            }
        } else {
            do_moveto = 1;
        }
        if (curr && do_moveto) {
            if (edit_stack_iterator + 1 < MAX_HISTORY_MOVETO) {
                g_free (edit_history_moveto[edit_stack_iterator].filename);
                if (edit->dir) {
                    edit_history_moveto[edit_stack_iterator].filename =
                        g_strdup_printf ("%s/%s", edit->dir, edit->filename);
                } else {
                    edit_history_moveto[edit_stack_iterator].filename = g_strdup (edit->filename);
                }
                edit_history_moveto[edit_stack_iterator].line = edit->start_line +
                    edit->curs_row + 1;
                edit_stack_iterator++;
                g_free (edit_history_moveto[edit_stack_iterator].filename);
                edit_history_moveto[edit_stack_iterator].filename =
                    g_strdup ((char *) curr_def->fullpath);
                edit_history_moveto[edit_stack_iterator].line = curr_def->line;
                edit_reload_line (edit, edit_history_moveto[edit_stack_iterator].filename,
                                  edit_history_moveto[edit_stack_iterator].line);
            }
        }
    }

    /* clear definition hash */
    for (i = 0; i < MAX_DEFINITIONS; i++) {
        g_free (def_hash[i].filename);
    }

    /* destroy dialog before return */
    destroy_dlg (def_dlg);
}
