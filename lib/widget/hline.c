/*
   Widgets for the Midnight Commander

   Copyright (C) 1994-2024
   Free Software Foundation, Inc.

   Authors:
   Radek Doulik, 1994, 1995
   Miguel de Icaza, 1994, 1995
   Jakub Jelinek, 1995
   Andrej Borsenkow, 1996
   Norbert Warmuth, 1997
   Andrew Borodin <aborodin@vmail.ru>, 2009-2022

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

#include <stdarg.h>
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

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
hline_adjust_cols (WHLine * l)
{
    if (l->auto_adjust_cols)
    {
        Widget *wl = WIDGET (l);
        const Widget *o = CONST_WIDGET (wl->owner);
        WRect *w = &wl->rect;
        const WRect *wo = &o->rect;

        if (CONST_DIALOG (o)->compact)
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
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
hline_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WHLine *l = HLINE (w);

    switch (msg)
    {
    case MSG_INIT:
        hline_adjust_cols (l);
        return MSG_HANDLED;

    case MSG_RESIZE:
        hline_adjust_cols (l);
        w->rect.y = RECT (data)->y;
        return MSG_HANDLED;

    case MSG_DRAW:
        if (l->transparent)
            tty_setcolor (DEFAULT_COLOR);
        else
        {
            const int *colors;

            colors = widget_get_colors (w);
            tty_setcolor (colors[DLG_COLOR_NORMAL]);
        }

        tty_draw_hline (w->rect.y, w->rect.x + 1, ACS_HLINE, w->rect.cols - 2);

        if (l->auto_adjust_cols)
        {
            widget_gotoyx (w, 0, 0);
            tty_print_alt_char (ACS_LTEE, FALSE);
            widget_gotoyx (w, 0, w->rect.cols - 1);
            tty_print_alt_char (ACS_RTEE, FALSE);
        }

        if (l->text != NULL)
        {
            int text_width;

            text_width = str_term_width1 (l->text);
            widget_gotoyx (w, 0, (w->rect.cols - text_width) / 2);
            tty_print_string (l->text);
        }
        return MSG_HANDLED;

    case MSG_DESTROY:
        g_free (l->text);
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
    WRect r = { y, x, 1, width };
    WHLine *l;
    Widget *w;

    l = g_new (WHLine, 1);
    w = WIDGET (l);
    r.cols = width < 0 ? 1 : width;
    widget_init (w, &r, hline_callback, NULL);
    l->text = NULL;
    l->auto_adjust_cols = (width < 0);
    l->transparent = FALSE;

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

    widget_draw (WIDGET (l));
}

/* --------------------------------------------------------------------------------------------- */

void
hline_set_textv (WHLine * l, const char *format, ...)
{
    va_list args;
    char buf[BUF_1K];           /* FIXME: is it enough? */

    va_start (args, format);
    g_vsnprintf (buf, sizeof (buf), format, args);
    va_end (args);

    hline_set_text (l, buf);
}

/* --------------------------------------------------------------------------------------------- */
