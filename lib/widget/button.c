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

/** \file button.c
 *  \brief Source: WButton widget
 */

#include <config.h>

#include <stdlib.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/tty/mouse.h"
#include "lib/strutil.h"
#include "lib/widget.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

static cb_ret_t
button_callback (Widget * w, widget_msg_t msg, int parm)
{
    WButton *b = (WButton *) w;
    int stop = 0;
    int off = 0;
    Dlg_head *h = b->widget.owner;

    switch (msg)
    {
    case WIDGET_HOTKEY:
        /*
         * Don't let the default button steal Enter from the current
         * button.  This is a workaround for the flawed event model
         * when hotkeys are sent to all widgets before the key is
         * handled by the current widget.
         */
        if (parm == '\n' && (Widget *) h->current->data == &b->widget)
        {
            button_callback (w, WIDGET_KEY, ' ');
            return MSG_HANDLED;
        }

        if (parm == '\n' && b->flags == DEFPUSH_BUTTON)
        {
            button_callback (w, WIDGET_KEY, ' ');
            return MSG_HANDLED;
        }

        if (b->text.hotkey != NULL && g_ascii_tolower ((gchar) b->text.hotkey[0]) == parm)
        {
            button_callback (w, WIDGET_KEY, ' ');
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    case WIDGET_KEY:
        if (parm != ' ' && parm != '\n')
            return MSG_NOT_HANDLED;

        if (b->callback != NULL)
            stop = b->callback (b, b->action);
        if (b->callback == NULL || stop != 0)
        {
            h->ret_value = b->action;
            dlg_stop (h);
        }
        return MSG_HANDLED;

    case WIDGET_CURSOR:
        switch (b->flags)
        {
        case DEFPUSH_BUTTON:
            off = 3;
            break;
        case NORMAL_BUTTON:
            off = 2;
            break;
        case NARROW_BUTTON:
            off = 1;
            break;
        case HIDDEN_BUTTON:
        default:
            off = 0;
            break;
        }
        widget_move (&b->widget, 0, b->hotpos + off);
        return MSG_HANDLED;

    case WIDGET_UNFOCUS:
    case WIDGET_FOCUS:
    case WIDGET_DRAW:
        if (msg == WIDGET_UNFOCUS)
            b->selected = FALSE;
        else if (msg == WIDGET_FOCUS)
            b->selected = TRUE;

        widget_selectcolor (w, b->selected, FALSE);
        widget_move (w, 0, 0);

        switch (b->flags)
        {
        case DEFPUSH_BUTTON:
            tty_print_string ("[< ");
            break;
        case NORMAL_BUTTON:
            tty_print_string ("[ ");
            break;
        case NARROW_BUTTON:
            tty_print_string ("[");
            break;
        case HIDDEN_BUTTON:
        default:
            return MSG_HANDLED;
        }

        hotkey_draw (w, b->text, b->selected);

        switch (b->flags)
        {
        case DEFPUSH_BUTTON:
            tty_print_string (" >]");
            break;
        case NORMAL_BUTTON:
            tty_print_string (" ]");
            break;
        case NARROW_BUTTON:
            tty_print_string ("]");
            break;
        default:
            break;
        }
        return MSG_HANDLED;

    case WIDGET_DESTROY:
        release_hotkey (b->text);
        return MSG_HANDLED;

    default:
        return default_proc (msg, parm);
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
button_event (Gpm_Event * event, void *data)
{
    Widget *w = (Widget *) data;

    if (!mouse_global_in_widget (event, w))
        return MOU_UNHANDLED;

    if ((event->type & (GPM_DOWN | GPM_UP)) != 0)
    {
        dlg_select_widget (w);
        if ((event->type & GPM_UP) != 0)
        {
            button_callback (w, WIDGET_KEY, ' ');
            w->owner->callback (w->owner, w, DLG_POST_KEY, ' ', NULL);
        }
    }

    return MOU_NORMAL;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

WButton *
button_new (int y, int x, int action, button_flags_t flags, const char *text, bcback_fn callback)
{
    WButton *b;

    b = g_new (WButton, 1);
    b->action = action;
    b->flags = flags;
    b->text = parse_hotkey (text);

    init_widget (&b->widget, y, x, 1, button_get_len (b), button_callback, button_event);

    b->selected = FALSE;
    b->callback = callback;
    widget_want_hotkey (b->widget, TRUE);
    b->hotpos = (b->text.hotkey != NULL) ? str_term_width1 (b->text.start) : -1;

    return b;
}

/* --------------------------------------------------------------------------------------------- */

const char *
button_get_text (const WButton * b)
{
    if (b->text.hotkey != NULL)
        return g_strconcat (b->text.start, "&", b->text.hotkey, b->text.end, (char *) NULL);
    return g_strdup (b->text.start);
}

/* --------------------------------------------------------------------------------------------- */

void
button_set_text (WButton * b, const char *text)
{
    release_hotkey (b->text);
    b->text = parse_hotkey (text);
    b->widget.cols = button_get_len (b);
    dlg_redraw (b->widget.owner);
}

/* --------------------------------------------------------------------------------------------- */

int
button_get_len (const WButton * b)
{
    int ret = hotkey_width (b->text);

    switch (b->flags)
    {
    case DEFPUSH_BUTTON:
        ret += 6;
        break;
    case NORMAL_BUTTON:
        ret += 4;
        break;
    case NARROW_BUTTON:
        ret += 2;
        break;
    case HIDDEN_BUTTON:
    default:
        return 0;
    }

    return ret;
}

/* --------------------------------------------------------------------------------------------- */
