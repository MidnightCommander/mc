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

/** \file gauge.c
 *  \brief Source: WGauge widget (progress indicator)
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/tty/color.h"
#include "lib/skin.h"
#include "lib/widget.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/* Currently width is hardcoded here for text mode */
#define gauge_len 47

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

static cb_ret_t
gauge_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WGauge *g = (WGauge *) w;
    WDialog *h = w->owner;

    if (msg == WIDGET_INIT)
        return MSG_HANDLED;

    /* We don't want to get the focus */
    if (msg == WIDGET_FOCUS)
        return MSG_NOT_HANDLED;

    if (msg == WIDGET_DRAW)
    {
        widget_move (w, 0, 0);
        tty_setcolor (h->color[DLG_COLOR_NORMAL]);
        if (!g->shown)
            tty_printf ("%*s", gauge_len, "");
        else
        {
            int percentage, columns;
            long total = g->max;
            long done = g->current;

            if (total <= 0 || done < 0)
            {
                done = 0;
                total = 100;
            }
            if (done > total)
                done = total;
            while (total > 65535)
            {
                total /= 256;
                done /= 256;
            }
            percentage = (200 * done / total + 1) / 2;
            columns = (2 * (gauge_len - 7) * done / total + 1) / 2;
            tty_print_char ('[');
            if (g->from_left_to_right)
            {
                tty_setcolor (GAUGE_COLOR);
                tty_printf ("%*s", (int) columns, "");
                tty_setcolor (h->color[DLG_COLOR_NORMAL]);
                tty_printf ("%*s] %3d%%", (int) (gauge_len - 7 - columns), "", (int) percentage);
            }
            else
            {
                tty_setcolor (h->color[DLG_COLOR_NORMAL]);
                tty_printf ("%*s", gauge_len - columns - 7, "");
                tty_setcolor (GAUGE_COLOR);
                tty_printf ("%*s", columns, "");
                tty_setcolor (h->color[DLG_COLOR_NORMAL]);
                tty_printf ("] %3d%%", 100 * columns / (gauge_len - 7), percentage);
            }
        }
        return MSG_HANDLED;
    }

    return widget_default_callback (sender, msg, parm, data);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

WGauge *
gauge_new (int y, int x, gboolean shown, int max, int current)
{
    WGauge *g;
    Widget *w;

    g = g_new (WGauge, 1);
    w = WIDGET (g);
    init_widget (w, y, x, 1, gauge_len, gauge_callback, NULL);

    g->shown = shown;
    if (max == 0)
        max = 1;                /* I do not like division by zero :) */
    g->max = max;
    g->current = current;
    g->from_left_to_right = TRUE;

    widget_want_cursor (w, FALSE);
    widget_want_hotkey (w, FALSE);

    return g;
}

/* --------------------------------------------------------------------------------------------- */

void
gauge_set_value (WGauge * g, int max, int current)
{
    if (g->current == current && g->max == max)
        return;                 /* Do not flicker */

    if (max == 0)
        max = 1;                /* I do not like division by zero :) */
    g->current = current;
    g->max = max;
    gauge_callback (WIDGET (g), NULL, WIDGET_DRAW, 0, NULL);
}

/* --------------------------------------------------------------------------------------------- */

void
gauge_show (WGauge * g, gboolean shown)
{
    if (g->shown != shown)
    {
        g->shown = shown;
        gauge_callback (WIDGET (g), NULL, WIDGET_DRAW, 0, NULL);
    }
}

/* --------------------------------------------------------------------------------------------- */
