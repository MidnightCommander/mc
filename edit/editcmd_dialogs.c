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

#include "../src/tty/tty.h"
#include "../src/tty/color.h"
#include "../src/tty/key.h"

#include "../src/search/search.h"

#include "../src/strutil.h"
#include "../src/widget.h"
#include "../src/wtools.h"
#include "../src/dialog.h"      /* do_refresh() */
#include "../src/main.h"
#include "../src/history.h"

#include "../edit/edit-widget.h"
#include "../edit/etags.h"
#include "../edit/editcmd_dialogs.h"


/*** global variables **************************************************/

/*** file scope macro definitions **************************************/

#define SEARCH_DLG_WIDTH 58
#define SEARCH_DLG_MIN_HEIGHT 11
#define SEARCH_DLG_HEIGHT_SUPPLY 3

#define REPLACE_DLG_WIDTH 58
#define REPLACE_DLG_MIN_HEIGHT 16
#define REPLACE_DLG_HEIGHT_SUPPLY 5

/*** file scope type declarations **************************************/

/*** file scope variables **********************************************/


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

/*** public functions **************************************************/

void
editcmd_dialog_replace_show (WEdit * edit, const char *search_default, const char *replace_default,
                             /*@out@ */ char **search_text, /*@out@ */ char **replace_text)
{
    int treplace_backwards = edit->replace_backwards;
    int treplace_case = edit->replace_case;
    int tall_codepages = edit->all_codepages;
    mc_search_type_t ttype_of_search = edit->search_type;
    int dialog_result;
    gchar **list_of_types = mc_search_get_types_strings_array();

    int REPLACE_DLG_HEIGHT = REPLACE_DLG_MIN_HEIGHT + g_strv_length (list_of_types) - REPLACE_DLG_HEIGHT_SUPPLY;

    if (!*search_default)
        search_default = INPUT_LAST_TEXT;

    QuickWidget quick_widgets[] = {

        {quick_button, 6, 10, 12, REPLACE_DLG_HEIGHT, N_("&Cancel"), 0, B_CANCEL,
         0, 0, NULL, NULL, NULL},

        {quick_button, 2, 10, 12, REPLACE_DLG_HEIGHT, N_("&OK"), 0, B_ENTER,
         0, 0, NULL, NULL, NULL},

#ifdef HAVE_CHARSET
        {quick_checkbox, 33, REPLACE_DLG_WIDTH, 9, REPLACE_DLG_HEIGHT, N_("All charsets"), 0, 0,
         &tall_codepages, 0, NULL, NULL, NULL},
#endif

        {quick_checkbox, 33, REPLACE_DLG_WIDTH, 8, REPLACE_DLG_HEIGHT, N_("&Backwards"), 0, 0,
         &treplace_backwards, 0, NULL, NULL, NULL},

        {quick_checkbox, 33, REPLACE_DLG_WIDTH, 7, REPLACE_DLG_HEIGHT, N_("case &Sensitive"), 0, 0,
         &treplace_case, 0, NULL, NULL, NULL},


        {quick_radio, 3, REPLACE_DLG_WIDTH, 7, REPLACE_DLG_HEIGHT, "", g_strv_length (list_of_types), ttype_of_search,
         (void *) &ttype_of_search, const_cast (char **, list_of_types), NULL, NULL, NULL},

        {quick_label, 2, REPLACE_DLG_WIDTH, 4, REPLACE_DLG_HEIGHT, N_(" Enter replacement string:"),
         0, 0, 0, 0, 0, NULL, NULL},

        {quick_input, 3, REPLACE_DLG_WIDTH, 5, REPLACE_DLG_HEIGHT, replace_default, 52, 0,
         0, replace_text, "replace", NULL, NULL},


        {quick_label, 2, REPLACE_DLG_WIDTH, 2, REPLACE_DLG_HEIGHT, N_(" Enter search string:"), 0,
         0, 0, 0, 0, NULL, NULL},

        {quick_input, 3, REPLACE_DLG_WIDTH, 3, REPLACE_DLG_HEIGHT, search_default, 52, 0,
         0, search_text, MC_HISTORY_SHARED_SEARCH, NULL, NULL},


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

    dialog_result = quick_dialog (&Quick_input);
    g_strfreev (list_of_types);

    if (dialog_result != B_CANCEL) {
        edit->search_type = ttype_of_search;
        edit->replace_mode = 0;
        edit->all_codepages = tall_codepages;
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
    int tsearch_case = edit->replace_case;
    int tsearch_backwards = edit->replace_backwards;
    int tall_codepages = edit->all_codepages;
    mc_search_type_t ttype_of_search = edit->search_type;
    gchar **list_of_types = mc_search_get_types_strings_array();
    int SEARCH_DLG_HEIGHT = SEARCH_DLG_MIN_HEIGHT + g_strv_length (list_of_types) - SEARCH_DLG_HEIGHT_SUPPLY;

    if (!*search_text)
        *search_text = INPUT_LAST_TEXT;

    QuickWidget quick_widgets[] = {
        {quick_button, 6, 10, 9, SEARCH_DLG_HEIGHT, N_("&Cancel"), 0, B_CANCEL, 0,
         0, NULL, NULL, NULL},
        {quick_button, 2, 10, 9, SEARCH_DLG_HEIGHT, N_("&OK"), 0, B_ENTER, 0,
         0, NULL, NULL, NULL},

#ifdef HAVE_CHARSET
        {quick_checkbox, 33, SEARCH_DLG_WIDTH, 7, SEARCH_DLG_HEIGHT, N_("All charsets"), 0, 0,
         &tall_codepages, 0, NULL, NULL, NULL},
#endif

        {quick_checkbox, 33, SEARCH_DLG_WIDTH, 6, SEARCH_DLG_HEIGHT, N_("&Backwards"), 0, 0,
         &tsearch_backwards, 0, NULL, NULL, NULL},
        {quick_checkbox, 33, SEARCH_DLG_WIDTH, 5, SEARCH_DLG_HEIGHT, N_("case &Sensitive"), 0, 0,
         &tsearch_case, 0, NULL, NULL, NULL},


        {quick_radio, 3, SEARCH_DLG_WIDTH, 5, SEARCH_DLG_HEIGHT, 0, g_strv_length (list_of_types), ttype_of_search,
         (void *) &ttype_of_search, const_cast (char **, list_of_types), NULL, NULL, NULL},

        {quick_input, 3, SEARCH_DLG_WIDTH, 3, SEARCH_DLG_HEIGHT, *search_text, 52, 0, 0,
         search_text, MC_HISTORY_SHARED_SEARCH, NULL, NULL},
        {quick_label, 2, SEARCH_DLG_WIDTH, 2, SEARCH_DLG_HEIGHT, N_(" Enter search string:"), 0, 0,
         0, 0, 0, NULL, NULL},

        NULL_QuickWidget
    };


    QuickDialog Quick_input = { SEARCH_DLG_WIDTH, SEARCH_DLG_HEIGHT, -1, 0, N_("Search"),
        "[Input Line Keys]", quick_widgets, 0
    };


    if (quick_dialog (&Quick_input) != B_CANCEL) {
        edit->search_type = ttype_of_search;
        edit->replace_backwards = tsearch_backwards;
        edit->all_codepages = tall_codepages;
        edit->replace_case = tsearch_case;
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

/* --------------------------------------------------------------------------------------------- */

int
editcmd_dialog_replace_prompt_show (WEdit * edit, char *from_text, char *to_text, int xpos, int ypos)
{
    /* dialog sizes */
    int dlg_height = 9;
    int dlg_width = 8;

    int retval;
    int i;
    int btn_pos;

    char *repl_from, *repl_to;
    char tmp [BUF_MEDIUM];

    QuickWidget quick_widgets[] = {
	{quick_button, 44, dlg_width, 6, dlg_height, N_("&Cancel"),
	0, B_CANCEL, 0, 0, NULL, NULL, NULL},
	{quick_button, 29, dlg_width, 6, dlg_height, N_("&Skip"),
	0, B_SKIP_REPLACE, 0, 0, NULL, NULL, NULL},
	{quick_button, 21, dlg_width, 6, dlg_height, N_("A&ll"),
	0, B_REPLACE_ALL, 0, 0, NULL, NULL, NULL},
	{quick_button, 4, dlg_width, 6, dlg_height, N_("&Replace"),
	0, B_ENTER, 0, 0, NULL, NULL, NULL},
	{quick_label, 3, dlg_width, 2, dlg_height, 0,
	0, 0, 0, 0, 0, NULL, NULL},
	{quick_label, 2, dlg_width, 3, dlg_height, 0,
	0, 0, 0, 0, 0, NULL, NULL},
	{quick_label, 3, dlg_width, 4, dlg_height, 0,
	0, 0, 0, 0, 0, NULL, NULL},
	NULL_QuickWidget
    };

#ifdef ENABLE_NLS
	for (i = 0; i < 4; i++)
	    quick_widgets[i].text = _(quick_widgets[i].text);
#endif

    /* calculate button positions */
    btn_pos = 4;

    for (i = 3; i > -1; i--) {
        quick_widgets[i].relative_x = btn_pos;

        btn_pos += str_term_width1 (quick_widgets[i].text) + 5;

        if (i == 3) /* default button */
            btn_pos += 2;
    }

    dlg_width = btn_pos + 2;

    /* correct widget coordinates */
    for (i = 0; i <  7; i++)
	quick_widgets[i].x_divisions = dlg_width;

    g_snprintf (tmp, sizeof (tmp), " '%s'", from_text);
    repl_from = g_strdup (str_fit_to_term (tmp, dlg_width - 7, J_LEFT));

    g_snprintf (tmp, sizeof (tmp), " '%s'", to_text);
    repl_to =  g_strdup (str_fit_to_term (tmp, dlg_width - 7, J_LEFT));

    quick_widgets[4].text = repl_from;
    quick_widgets[5].text = _(" Replace with: ");
    quick_widgets[6].text = repl_to;

    if (xpos == -1)
	xpos = (edit->num_widget_columns - dlg_width) / 2;

    if (ypos == -1)
	ypos = edit->num_widget_lines * 2 / 3;

    {
	QuickDialog Quick_input =
	{dlg_width, dlg_height, 0, 0, N_ (" Confirm replace "),
	 "[Input Line Keys]", 0 /*quick_widgets */, 0 };

	Quick_input.widgets = quick_widgets;

	Quick_input.xpos = xpos;

	/* Sometimes menu can hide replaced text. I don't like it */

	if ((edit->curs_row >= ypos - 1) && (edit->curs_row <= ypos + dlg_height - 1))
	    ypos -= dlg_height;

	Quick_input.ypos = ypos;
	retval = quick_dialog (&Quick_input);
	g_free (repl_from);
	g_free (repl_to);
	return retval;
    }
}

/* --------------------------------------------------------------------------------------------- */
