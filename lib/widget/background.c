/*
   Widgets for the Midnight Commander

   Copyright (C) 2020-2024
   The Free Software Foundation, Inc.

   Authors:
   Andrew Borodin <aborodin@vmail.ru>, 2020-2022

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

/** \file background.c
 *  \brief Source: WBackground widget (background area of dialog)
 */

#include <config.h>

#include <stdlib.h>

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/tty/color.h"
#include "lib/widget.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static const int *
background_get_colors (const Widget * w)
{
    return &(CONST_BACKGROUND (w)->color);
}

/* --------------------------------------------------------------------------------------------- */

static void
background_adjust (WBackground * b)
{
    Widget *w = WIDGET (b);

    w->rect = WIDGET (w->owner)->rect;
    w->pos_flags |= WPOS_KEEP_ALL;
}

/* --------------------------------------------------------------------------------------------- */

static void
background_draw (const WBackground * b)
{
    const Widget *w = CONST_WIDGET (b);

    tty_setcolor (b->color);
    tty_fill_region (w->rect.y, w->rect.x, w->rect.lines, w->rect.cols, b->pattern);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

cb_ret_t
background_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WBackground *b = BACKGROUND (w);

    switch (msg)
    {
    case MSG_INIT:
        background_adjust (b);
        return MSG_HANDLED;

    case MSG_DRAW:
        background_draw (b);
        return MSG_HANDLED;

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

WBackground *
background_new (int y, int x, int lines, int cols, int color, unsigned char pattern,
                widget_cb_fn callback)
{
    WRect r = { y, x, lines, cols };
    WBackground *b;
    Widget *w;

    b = g_new (WBackground, 1);
    w = WIDGET (b);
    widget_init (w, &r, callback != NULL ? callback : background_callback, NULL);
    w->get_colors = background_get_colors;

    b->color = color;
    b->pattern = pattern;

    return b;
}

/* --------------------------------------------------------------------------------------------- */
