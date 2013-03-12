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

/** \file hline.c
 *  \brief Source: WHLine widget (horizontal line)
 */

#include <config.h>

#include <stdlib.h>

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

static cb_ret_t
hline_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WHLine *l = HLINE (w);
    WDialog *h = w->owner;

    switch (msg)
    {
    case MSG_INIT:
    case MSG_RESIZE:
        if (l->auto_adjust_cols)
        {
            Widget *wo = WIDGET (h);

            if (((h->flags & DLG_COMPACT) != 0))
            {
                w->x = wo->x;
                w->cols = wo->cols;
            }
            else
            {
                w->x = wo->x + 1;
                w->cols = wo->cols - 2;
            }
        }

    case MSG_FOCUS:
        /* We don't want to get the focus */
        return MSG_NOT_HANDLED;

    case MSG_DRAW:
        if (l->transparent)
            tty_setcolor (DEFAULT_COLOR);
        else
            tty_setcolor (h->color[DLG_COLOR_NORMAL]);

        tty_draw_hline (w->y, w->x + 1, ACS_HLINE, w->cols - 2);

        if (l->auto_adjust_cols)
        {
            widget_move (w, 0, 0);
            tty_print_alt_char (ACS_LTEE, FALSE);
            widget_move (w, 0, w->cols - 1);
            tty_print_alt_char (ACS_RTEE, FALSE);
        }

        if (l->text != NULL)
        {
            int text_width;

            text_width = str_term_width1 (l->text);
            widget_move (w, 0, (w->cols - text_width) / 2);
            tty_print_string (l->text);
        }
        return MSG_HANDLED;

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

WHLine *
hline_new (int y, int x, int width)
{
    WHLine *l;
    Widget *w;
    int lines = 1;

    l = g_new (WHLine, 1);
    w = WIDGET (l);
    init_widget (w, y, x, lines, width < 0 ? 1 : width, hline_callback, NULL);
    l->text = NULL;
    l->auto_adjust_cols = (width < 0);
    l->transparent = FALSE;
    widget_want_cursor (w, FALSE);
    widget_want_hotkey (w, FALSE);

    return l;
}

/* --------------------------------------------------------------------------------------------- */

void
hline_set_text (WHLine * l, const char *text)
{
    g_free (l->text);

    if (text == NULL || *text == '\0')
        l->text = NULL;
    else
        l->text = g_strdup (text);

    widget_redraw (WIDGET (l));
}

/* --------------------------------------------------------------------------------------------- */
