/*
   Editor dialogs for high level editing commands

   Copyright (C) 2009-2021
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2009
   Andrew Borodin, <aborodin@vmail.ru>, 2012, 2013

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

#include "src/history.h"

#include "src/editor/editwidget.h"
#include "src/editor/etags.h"
#include "src/editor/editcmd_dialogs.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
editcmd_dialog_raw_key_query_cb (Widget * w, Widget * sender, widget_msg_t msg, int parm,
                                 void *data)
{
    WDialog *h = DIALOG (w);

    switch (msg)
    {
    case MSG_KEY:
        h->ret_value = parm;
        dlg_stop (h);
        return MSG_HANDLED;
    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* gets a raw key from the keyboard. Passing cancel = 1 draws
   a cancel button thus allowing c-c etc.  Alternatively, cancel = 0
   will return the next key pressed.  ctrl-a (=B_CANCEL), ctrl-g, ctrl-c,
   and Esc are cannot returned */

int
editcmd_dialog_raw_key_query (const char *heading, const char *query, gboolean cancel)
{
    int w, wq;
    int y = 2;
    WDialog *raw_dlg;
    WGroup *g;

    w = str_term_width1 (heading) + 6;
    wq = str_term_width1 (query);
    w = MAX (w, wq + 3 * 2 + 1 + 2);

    raw_dlg =
        dlg_create (TRUE, 0, 0, cancel ? 7 : 5, w, WPOS_CENTER | WPOS_TRYUP, FALSE, dialog_colors,
                    editcmd_dialog_raw_key_query_cb, NULL, NULL, heading);
    g = GROUP (raw_dlg);
    widget_want_tab (WIDGET (raw_dlg), TRUE);

    group_add_widget (g, label_new (y, 3, query));
    group_add_widget (g,
                      input_new (y++, 3 + wq + 1, input_colors, w - (6 + wq + 1), "", 0,
                                 INPUT_COMPLETE_NONE));
    if (cancel)
    {
        group_add_widget (g, hline_new (y++, -1, -1));
        /* Button w/o hotkey to allow use any key as raw or macro one */
        group_add_widget_autopos (g, button_new (y, 1, B_CANCEL, NORMAL_BUTTON, _("Cancel"), NULL),
                                  WPOS_KEEP_TOP | WPOS_CENTER_HORZ, NULL);
    }

    w = dlg_run (raw_dlg);
    widget_destroy (WIDGET (raw_dlg));

    return (cancel && (w == ESC_CHAR || w == B_CANCEL)) ? 0 : w;
}

/* --------------------------------------------------------------------------------------------- */
/* let the user select its preferred completion */

char *
editcmd_dialog_completion_show (const WEdit * edit, GQueue * compl, int max_width)
{
    const Widget *we = CONST_WIDGET (edit);
    int start_x, start_y, offset;
    char *curr = NULL;
    WDialog *compl_dlg;
    WListbox *compl_list;
    int compl_dlg_h;            /* completion dialog height */
    int compl_dlg_w;            /* completion dialog width */
    GList *i;

    /* calculate the dialog metrics */
    compl_dlg_h = g_queue_get_length (compl) + 2;
    compl_dlg_w = max_width + 4;
    start_x = we->x + edit->curs_col + edit->start_col + EDIT_TEXT_HORIZONTAL_OFFSET +
        (edit->fullscreen ? 0 : 1) + option_line_state_width;
    start_y = we->y + edit->curs_row + EDIT_TEXT_VERTICAL_OFFSET + (edit->fullscreen ? 0 : 1) + 1;

    if (start_x < 0)
        start_x = 0;
    if (start_x < we->x + 1)
        start_x = we->x + 1 + option_line_state_width;
    if (compl_dlg_w > COLS)
        compl_dlg_w = COLS;
    if (compl_dlg_h > LINES - 2)
        compl_dlg_h = LINES - 2;

    offset = start_x + compl_dlg_w - COLS;
    if (offset > 0)
        start_x -= offset;
    offset = start_y + compl_dlg_h - LINES;
    if (offset > 0)
        start_y -= offset;

    /* create the dialog */
    compl_dlg =
        dlg_create (TRUE, start_y, start_x, compl_dlg_h, compl_dlg_w, WPOS_KEEP_DEFAULT, TRUE,
                    dialog_colors, NULL, NULL, "[Completion]", NULL);

    /* create the listbox */
    compl_list = listbox_new (1, 1, compl_dlg_h - 2, compl_dlg_w - 2, FALSE, NULL);

    /* fill the listbox with the completions in the reverse order */
    for (i = g_queue_peek_tail_link (compl); i != NULL; i = g_list_previous (i))
        listbox_add_item (compl_list, LISTBOX_APPEND_AT_END, 0, ((GString *) i->data)->str, NULL,
                          FALSE);

    group_add_widget (GROUP (compl_dlg), compl_list);

    /* pop up the dialog and apply the chosen completion */
    if (dlg_run (compl_dlg) == B_ENTER)
    {
        listbox_get_current (compl_list, &curr, NULL);
        curr = g_strdup (curr);
    }

    /* destroy dialog before return */
    widget_destroy (WIDGET (compl_dlg));

    return curr;
}

/* --------------------------------------------------------------------------------------------- */
