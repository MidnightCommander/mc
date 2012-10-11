/*
   Widgets for the Midnight Commander

   Copyright (C) 1994, 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003,
   2004, 2005, 2006, 2007, 2009, 2010, 2011
   The Free Software Foundation, Inc.

   Authors:
   Radek Doulik, 1994, 1995
   Miguel de Icaza, 1994, 1995
   Jakub Jelinek, 1995
   Andrej Borsenkow, 1996
   Norbert Warmuth, 1997
   Andrew Borodin <aborodin@vmail.ru>, 2009, 2010

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

/** \file check.c
 *  \brief Source: WCheck widget (checkbutton)
 */

#include <config.h>

#include <stdlib.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/tty/mouse.h"
#include "lib/widget.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

static cb_ret_t
check_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WCheck *c = CHECK (w);

    switch (msg)
    {
    case MSG_HOTKEY:
        if (c->text.hotkey != NULL)
        {
            if (g_ascii_tolower ((gchar) c->text.hotkey[0]) == parm)
            {
                /* make action */
                send_message (w, sender, MSG_KEY, ' ', data);
                return MSG_HANDLED;
            }
        }
        return MSG_NOT_HANDLED;

    case MSG_KEY:
        if (parm != ' ')
            return MSG_NOT_HANDLED;
        c->state ^= C_BOOL;
        c->state ^= C_CHANGE;
        send_message (WIDGET (w)->owner, w, MSG_ACTION, 0, NULL);
        send_message (w, sender, MSG_FOCUS, ' ', data);
        return MSG_HANDLED;

    case MSG_CURSOR:
        widget_move (c, 0, 1);
        return MSG_HANDLED;

    case MSG_FOCUS:
    case MSG_UNFOCUS:
    case MSG_DRAW:
        widget_selectcolor (w, msg == MSG_FOCUS, FALSE);
        widget_move (c, 0, 0);
        tty_print_string ((c->state & C_BOOL) ? "[x] " : "[ ] ");
        hotkey_draw (w, c->text, msg == MSG_FOCUS);
        return MSG_HANDLED;

    case MSG_DESTROY:
        release_hotkey (c->text);
        return MSG_HANDLED;

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
check_event (Gpm_Event * event, void *data)
{
    Widget *w = WIDGET (data);

    if (!mouse_global_in_widget (event, w))
        return MOU_UNHANDLED;

    if ((event->type & (GPM_DOWN | GPM_UP)) != 0)
    {
        dlg_select_widget (w);
        if ((event->type & GPM_UP) != 0)
        {
            send_message (w, NULL, MSG_KEY, ' ', NULL);
            send_message (w, NULL, MSG_FOCUS, 0, NULL);
            send_message (w->owner, w, MSG_POST_KEY, ' ', NULL);
        }
    }

    return MOU_NORMAL;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

WCheck *
check_new (int y, int x, int state, const char *text)
{
    WCheck *c;
    Widget *w;

    c = g_new (WCheck, 1);
    w = WIDGET (c);
    c->text = parse_hotkey (text);
    init_widget (w, y, x, 1, 4 + hotkey_width (c->text), check_callback, check_event);
    /* 4 is width of "[X] " */
    c->state = state ? C_BOOL : 0;
    widget_want_hotkey (w, TRUE);

    return c;
}

/* --------------------------------------------------------------------------------------------- */
