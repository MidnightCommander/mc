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

/** \file buttonbar.c
 *  \brief Source: WButtonBar widget
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/tty/mouse.h"
#include "lib/tty/key.h"        /* XCTRL and ALT macros  */
#include "lib/skin.h"
#include "lib/strutil.h"
#include "lib/util.h"
#include "lib/keybind.h"        /* global_keymap_t */
#include "lib/widget.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* calculate positions of buttons; width is never less than 7 */
static void
buttonbar_init_button_positions (WButtonBar * bb)
{
    int i;
    int pos = 0;

    if (COLS < BUTTONBAR_LABELS_NUM * 7)
    {
        for (i = 0; i < BUTTONBAR_LABELS_NUM; i++)
        {
            if (pos + 7 <= COLS)
                pos += 7;

            bb->labels[i].end_coord = pos;
        }
    }
    else
    {
        /* Distribute the extra width in a way that the middle vertical line
           (between F5 and F6) aligns with the two panels. The extra width
           is distributed in this order: F10, F5, F9, F4, ..., F6, F1. */
        int dv, md;

        dv = COLS / BUTTONBAR_LABELS_NUM;
        md = COLS % BUTTONBAR_LABELS_NUM;

        for (i = 0; i < BUTTONBAR_LABELS_NUM / 2; i++)
        {
            pos += dv;
            if (BUTTONBAR_LABELS_NUM / 2 - 1 - i < md / 2)
                pos++;

            bb->labels[i].end_coord = pos;
        }

        for (; i < BUTTONBAR_LABELS_NUM; i++)
        {
            pos += dv;
            if (BUTTONBAR_LABELS_NUM - 1 - i < (md + 1) / 2)
                pos++;

            bb->labels[i].end_coord = pos;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

/* return width of one button */
static int
buttonbar_get_button_width (const WButtonBar * bb, int i)
{
    if (i == 0)
        return bb->labels[0].end_coord;
    return bb->labels[i].end_coord - bb->labels[i - 1].end_coord;
}

/* --------------------------------------------------------------------------------------------- */

static int
buttonbar_get_button_by_x_coord (const WButtonBar * bb, int x)
{
    int i;

    for (i = 0; i < BUTTONBAR_LABELS_NUM; i++)
        if (bb->labels[i].end_coord > x)
            return i;

    return (-1);
}

/* --------------------------------------------------------------------------------------------- */

static void
set_label_text (WButtonBar * bb, int idx, const char *text)
{
    g_free (bb->labels[idx - 1].text);
    bb->labels[idx - 1].text = g_strdup (text);
}

/* --------------------------------------------------------------------------------------------- */

/* returns TRUE if a function has been called, FALSE otherwise. */
static gboolean
buttonbar_call (WButtonBar * bb, int i)
{
    cb_ret_t ret = MSG_NOT_HANDLED;

    if ((bb != NULL) && (bb->labels[i].command != CK_IgnoreKey))
        ret = bb->widget.owner->callback (bb->widget.owner,
                                          (Widget *) bb, DLG_ACTION,
                                          bb->labels[i].command, bb->labels[i].receiver);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
buttonbar_callback (Widget * w, widget_msg_t msg, int parm)
{
    WButtonBar *bb = (WButtonBar *) w;
    int i;
    const char *text;

    switch (msg)
    {
    case WIDGET_FOCUS:
        return MSG_NOT_HANDLED;

    case WIDGET_HOTKEY:
        for (i = 0; i < BUTTONBAR_LABELS_NUM; i++)
            if (parm == KEY_F (i + 1) && buttonbar_call (bb, i))
                return MSG_HANDLED;
        return MSG_NOT_HANDLED;

    case WIDGET_DRAW:
        if (bb->visible)
        {
            buttonbar_init_button_positions (bb);
            widget_move (&bb->widget, 0, 0);
            tty_setcolor (DEFAULT_COLOR);
            tty_printf ("%-*s", bb->widget.cols, "");
            widget_move (&bb->widget, 0, 0);

            for (i = 0; i < BUTTONBAR_LABELS_NUM; i++)
            {
                int width;

                width = buttonbar_get_button_width (bb, i);
                if (width <= 0)
                    break;

                tty_setcolor (BUTTONBAR_HOTKEY_COLOR);
                tty_printf ("%2d", i + 1);

                tty_setcolor (BUTTONBAR_BUTTON_COLOR);
                text = (bb->labels[i].text != NULL) ? bb->labels[i].text : "";
                tty_print_string (str_fit_to_term (text, width - 2, J_LEFT_FIT));
            }
        }
        return MSG_HANDLED;

    case WIDGET_DESTROY:
        for (i = 0; i < BUTTONBAR_LABELS_NUM; i++)
            g_free (bb->labels[i].text);
        return MSG_HANDLED;

    default:
        return default_proc (msg, parm);
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
buttonbar_event (Gpm_Event * event, void *data)
{
    Widget *w = (Widget *) data;

    if (!mouse_global_in_widget (event, w))
        return MOU_UNHANDLED;

    if ((event->type & GPM_UP) != 0)
    {
        WButtonBar *bb = (WButtonBar *) data;
        Gpm_Event local;
        int button;

        local = mouse_get_local (event, w);
        button = buttonbar_get_button_by_x_coord (bb, local.x - 1);
        if (button >= 0)
            buttonbar_call (bb, button);
    }

    return MOU_NORMAL;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

WButtonBar *
buttonbar_new (gboolean visible)
{
    WButtonBar *bb;

    bb = g_new0 (WButtonBar, 1);

    init_widget (&bb->widget, LINES - 1, 0, 1, COLS, buttonbar_callback, buttonbar_event);
    bb->widget.pos_flags = WPOS_KEEP_HORZ | WPOS_KEEP_BOTTOM;
    bb->visible = visible;
    widget_want_hotkey (bb->widget, 1);
    widget_want_cursor (bb->widget, 0);

    return bb;
}

/* --------------------------------------------------------------------------------------------- */

void
buttonbar_set_label (WButtonBar * bb, int idx, const char *text,
                     const struct global_keymap_t *keymap, const Widget * receiver)
{
    if ((bb != NULL) && (idx >= 1) && (idx <= BUTTONBAR_LABELS_NUM))
    {
        unsigned long command = CK_IgnoreKey;

        if (keymap != NULL)
            command = keybind_lookup_keymap_command (keymap, KEY_F (idx));

        if ((text == NULL) || (text[0] == '\0'))
            set_label_text (bb, idx, "");
        else
            set_label_text (bb, idx, text);

        bb->labels[idx - 1].command = command;
        bb->labels[idx - 1].receiver = (Widget *) receiver;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* Find ButtonBar widget in the dialog */
WButtonBar *
find_buttonbar (const Dlg_head * h)
{
    return (WButtonBar *) find_widget_type (h, buttonbar_callback);
}
