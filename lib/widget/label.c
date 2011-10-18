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

/** \file label.c
 *  \brief Source: WLabel widget
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

static cb_ret_t
label_callback (Widget * w, widget_msg_t msg, int parm)
{
    WLabel *l = (WLabel *) w;
    Dlg_head *h = l->widget.owner;

    switch (msg)
    {
    case WIDGET_INIT:
        return MSG_HANDLED;

        /* We don't want to get the focus */
    case WIDGET_FOCUS:
        return MSG_NOT_HANDLED;

    case WIDGET_DRAW:
        {
            char *p = l->text;
            int y = 0;
            gboolean disabled = (w->options & W_DISABLED) != 0;

            if (l->text == NULL)
                return MSG_HANDLED;

            if (l->transparent)
                tty_setcolor (disabled ? DISABLED_COLOR : DEFAULT_COLOR);
            else
                tty_setcolor (disabled ? DISABLED_COLOR : h->color[DLG_COLOR_NORMAL]);

            while (TRUE)
            {
                char *q;
                char c = '\0';

                q = strchr (p, '\n');
                if (q != NULL)
                {
                    c = q[0];
                    q[0] = '\0';
                }

                widget_move (&l->widget, y, 0);
                tty_print_string (str_fit_to_term (p, l->widget.cols, J_LEFT));

                if (q == NULL)
                    break;

                q[0] = c;
                p = q + 1;
                y++;
            }
            return MSG_HANDLED;
        }

    case WIDGET_DESTROY:
        g_free (l->text);
        return MSG_HANDLED;

    default:
        return default_proc (msg, parm);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

WLabel *
label_new (int y, int x, const char *text)
{
    WLabel *l;
    int cols = 1;
    int lines = 1;

    if (text != NULL)
        str_msg_term_size (text, &lines, &cols);

    l = g_new (WLabel, 1);
    init_widget (&l->widget, y, x, lines, cols, label_callback, NULL);
    l->text = g_strdup (text);
    l->auto_adjust_cols = TRUE;
    l->transparent = FALSE;
    widget_want_cursor (l->widget, FALSE);
    widget_want_hotkey (l->widget, FALSE);

    return l;
}

/* --------------------------------------------------------------------------------------------- */

void
label_set_text (WLabel * label, const char *text)
{
    int newcols = label->widget.cols;
    int newlines;

    if (label->text != NULL && text != NULL && strcmp (label->text, text) == 0)
        return;                 /* Flickering is not nice */

    g_free (label->text);

    if (text == NULL)
        label->text = NULL;
    else
    {
        label->text = g_strdup (text);
        if (label->auto_adjust_cols)
        {
            str_msg_term_size (text, &newlines, &newcols);
            if (newcols > label->widget.cols)
                label->widget.cols = newcols;
            if (newlines > label->widget.lines)
                label->widget.lines = newlines;
        }
    }

    if (label->widget.owner != NULL)
        label_callback ((Widget *) label, WIDGET_DRAW, 0);

    if (newcols < label->widget.cols)
        label->widget.cols = newcols;
}

/* --------------------------------------------------------------------------------------------- */
