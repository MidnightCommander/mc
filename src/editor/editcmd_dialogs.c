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
