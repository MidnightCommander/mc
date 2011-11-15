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

/** \file widget-common.c
 *  \brief Source: shared stuff of widgets
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/tty/color.h"
#include "lib/skin.h"
#include "lib/strutil.h"
#include "lib/widget.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

struct hotkey_t
parse_hotkey (const char *text)
{
    hotkey_t result;
    const char *cp, *p;

    if (text == NULL)
        text = "";

    /* search for '&', that is not on the of text */
    cp = strchr (text, '&');
    if (cp != NULL && cp[1] != '\0')
    {
        result.start = g_strndup (text, cp - text);

        /* skip '&' */
        cp++;
        p = str_cget_next_char (cp);
        result.hotkey = g_strndup (cp, p - cp);

        cp = p;
        result.end = g_strdup (cp);
    }
    else
    {
        result.start = g_strdup (text);
        result.hotkey = NULL;
        result.end = NULL;
    }

    return result;
}

/* --------------------------------------------------------------------------------------------- */

void
release_hotkey (const hotkey_t hotkey)
{
    g_free (hotkey.start);
    g_free (hotkey.hotkey);
    g_free (hotkey.end);
}

/* --------------------------------------------------------------------------------------------- */

int
hotkey_width (const hotkey_t hotkey)
{
    int result;

    result = str_term_width1 (hotkey.start);
    result += (hotkey.hotkey != NULL) ? str_term_width1 (hotkey.hotkey) : 0;
    result += (hotkey.end != NULL) ? str_term_width1 (hotkey.end) : 0;
    return result;
}

/* --------------------------------------------------------------------------------------------- */

void
hotkey_draw (Widget * w, const hotkey_t hotkey, gboolean focused)
{
    widget_selectcolor (w, focused, FALSE);
    tty_print_string (hotkey.start);

    if (hotkey.hotkey != NULL)
    {
        widget_selectcolor (w, focused, TRUE);
        tty_print_string (hotkey.hotkey);
        widget_selectcolor (w, focused, FALSE);
    }

    if (hotkey.end != NULL)
        tty_print_string (hotkey.end);
}

/* --------------------------------------------------------------------------------------------- */

void
init_widget (Widget * w, int y, int x, int lines, int cols,
             callback_fn callback, mouse_h mouse_handler)
{
    w->x = x;
    w->y = y;
    w->cols = cols;
    w->lines = lines;
    w->callback = callback;
    w->mouse = mouse_handler;
    w->owner = NULL;

    /* Almost all widgets want to put the cursor in a suitable place */
    w->options = W_WANT_CURSOR;
}

/* --------------------------------------------------------------------------------------------- */

/* Default callback for widgets */
cb_ret_t
default_proc (widget_msg_t msg, int parm)
{
    (void) parm;

    switch (msg)
    {
    case WIDGET_INIT:
    case WIDGET_FOCUS:
    case WIDGET_UNFOCUS:
    case WIDGET_DRAW:
    case WIDGET_DESTROY:
    case WIDGET_CURSOR:
    case WIDGET_IDLE:
        return MSG_HANDLED;

    default:
        return MSG_NOT_HANDLED;
    }
}

/* --------------------------------------------------------------------------------------------- */

void
widget_set_size (Widget * widget, int y, int x, int lines, int cols)
{
    widget->x = x;
    widget->y = y;
    widget->cols = cols;
    widget->lines = lines;
    send_message (widget, WIDGET_RESIZED, 0 /* unused */ );
}

/* --------------------------------------------------------------------------------------------- */

void
widget_selectcolor (Widget * w, gboolean focused, gboolean hotkey)
{
    Dlg_head *h = w->owner;
    int color;

    if ((w->options & W_DISABLED) != 0)
        color = DISABLED_COLOR;
    else if (hotkey)
    {
        if (focused)
            color = h->color[DLG_COLOR_HOT_FOCUS];
        else
            color = h->color[DLG_COLOR_HOT_NORMAL];
    }
    else
    {
        if (focused)
            color = h->color[DLG_COLOR_FOCUS];
        else
            color = h->color[DLG_COLOR_NORMAL];
    }

    tty_setcolor (color);
}

/* --------------------------------------------------------------------------------------------- */

void
widget_erase (Widget * w)
{
    if (w != NULL)
        tty_fill_region (w->y, w->x, w->lines, w->cols, ' ');
}

/* --------------------------------------------------------------------------------------------- */

/* get mouse pointer location within widget */
Gpm_Event
mouse_get_local (const Gpm_Event * global, const Widget * w)
{
    Gpm_Event local;

    local.buttons = global->buttons;
    local.x = global->x - w->x;
    local.y = global->y - w->y;
    local.type = global->type;

    return local;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mouse_global_in_widget (const Gpm_Event * event, const Widget * w)
{
    return (event->x > w->x) && (event->y > w->y) && (event->x <= w->x + w->cols)
        && (event->y <= w->y + w->lines);
}

/* --------------------------------------------------------------------------------------------- */
