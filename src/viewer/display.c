/*
   Internal file viewer for the Midnight Commander
   Function for whow info on display

   Copyright (C) 1994, 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003,
   2004, 2005, 2006, 2007, 2009 Free Software Foundation, Inc.

   Written by: 1994, 1995, 1998 Miguel de Icaza
   1994, 1995 Janne Kukonlehto
   1995 Jakub Jelinek
   1996 Joseph M. Hinkle
   1997 Norbert Warmuth
   1998 Pavel Machek
   2004 Roland Illig <roland.illig@gmx.de>
   2005 Roland Illig <roland.illig@gmx.de>
   2009 Slava Zanko <slavazanko@google.com>
   2009 Andrew Borodin <aborodin@vmail.ru>
   2009, 2010 Ilia Maslakov <il.smind@gmail.com>

   This file is part of the Midnight Commander.

   The Midnight Commander is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA.
 */

#include <config.h>

#include "lib/global.h"
#include "lib/skin.h"
#include "lib/tty/tty.h"
#include "lib/tty/key.h"
#include "lib/strutil.h"

#include "src/main.h"
#include "src/dialog.h"         /* Dlg_head */
#include "src/charsets.h"
#include "src/widget.h"         /* WButtonBar */

#include "internal.h"
#include "mcviewer.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* If set, show a ruler */
static enum ruler_type
{
    RULER_NONE,
    RULER_TOP,
    RULER_BOTTOM
} ruler = RULER_NONE;

/*** file scope functions ************************************************************************/

/* --------------------------------------------------------------------------------------------- */

/* Define labels and handlers for functional keys */
static void
mcview_set_buttonbar (mcview_t * view)
{
    Dlg_head *h = view->widget.parent;
    WButtonBar *b = find_buttonbar (h);
    const global_keymap_t *keymap = view->hex_mode ? view->hex_map : view->plain_map;

    buttonbar_set_label (b, 1, Q_ ("ButtonBar|Help"), keymap, (Widget *) view);

    if (view->hex_mode)
    {
        if (view->hexedit_mode)
            buttonbar_set_label (b, 2, Q_ ("ButtonBar|View"), keymap, (Widget *) view);
        else if (view->datasource == DS_FILE)
            buttonbar_set_label (b, 2, Q_ ("ButtonBar|Edit"), keymap, (Widget *) view);
        else
            buttonbar_set_label (b, 2, "", keymap, (Widget *) view);

        buttonbar_set_label (b, 4, Q_ ("ButtonBar|Ascii"), keymap, (Widget *) view);
        buttonbar_set_label (b, 6, Q_ ("ButtonBar|Save"), keymap, (Widget *) view);
        buttonbar_set_label (b, 7, Q_ ("ButtonBar|HxSrch"), keymap, (Widget *) view);

    }
    else
    {
        buttonbar_set_label (b, 2, view->text_wrap_mode ? Q_ ("ButtonBar|UnWrap")
                             : Q_ ("ButtonBar|Wrap"), keymap, (Widget *) view);
        buttonbar_set_label (b, 4, Q_ ("ButtonBar|Hex"), keymap, (Widget *) view);
        buttonbar_set_label (b, 6, "", keymap, (Widget *) view);
        buttonbar_set_label (b, 7, Q_ ("ButtonBar|Search"), keymap, (Widget *) view);
    }

    buttonbar_set_label (b, 5, Q_ ("ButtonBar|Goto"), keymap, (Widget *) view);
    buttonbar_set_label (b, 8, view->magic_mode ? Q_ ("ButtonBar|Raw")
                         : Q_ ("ButtonBar|Parse"), keymap, (Widget *) view);

    if (mcview_is_in_panel (view))
        buttonbar_set_label (b, 10, "", keymap, (Widget *) view);
    else
    {
        /* don't override some panel buttonbar keys  */
        buttonbar_set_label (b, 3, Q_ ("ButtonBar|Quit"), keymap, (Widget *) view);
        buttonbar_set_label (b, 9, view->text_nroff_mode ? Q_ ("ButtonBar|Unform")
                             : Q_ ("ButtonBar|Format"), keymap, (Widget *) view);
        buttonbar_set_label (b, 10, Q_ ("ButtonBar|Quit"), keymap, (Widget *) view);
    }
}

/* --------------------------------------------------------------------------------------------- */


static void
mcview_display_status (mcview_t * view)
{
    const screen_dimen top = view->status_area.top;
    const screen_dimen left = view->status_area.left;
    const screen_dimen width = view->status_area.width;
    const screen_dimen height = view->status_area.height;
    const char *file_label;
    screen_dimen file_label_width;

    if (height < 1)
        return;

    tty_setcolor (SELECTED_COLOR);
    widget_move (view, top, left);
    tty_draw_hline (-1, -1, ' ', width);

    file_label = view->filename ? view->filename : view->command ? view->command : "";
    file_label_width = str_term_width1 (file_label) - 2;
    if (width > 40)
    {
        char buffer[BUF_TINY];
        widget_move (view, top, width - 32);
        if (view->hex_mode)
        {
            tty_printf ("0x%08lx", (unsigned long) view->hex_cursor);
        }
        else
        {
            size_trunc_len (buffer, 5, mcview_get_filesize (view), 0);
            tty_printf ("%9lli/%s%s %s", view->dpy_end,
                        buffer, mcview_may_still_grow (view) ? "+" : " ",
#ifdef HAVE_CHARSET
                        source_codepage >= 0 ? get_codepage_id (source_codepage) : ""
#else
                        ""
#endif
                );
        }
    }
    widget_move (view, top, left);
    if (width > 40)
        tty_print_string (str_fit_to_term (file_label, width - 34, J_LEFT_FIT));
    else
        tty_print_string (str_fit_to_term (file_label, width - 5, J_LEFT_FIT));
    if (width > 26)
        mcview_percent (view, view->hex_mode ? view->hex_cursor : view->dpy_end);
}

/* --------------------------------------------------------------------------------------------- */

/*** public functions ****************************************************************************/

/* --------------------------------------------------------------------------------------------- */

void
mcview_update (mcview_t * view)
{
    static int dirt_limit = 1;

    if (view->dpy_bbar_dirty)
    {
        view->dpy_bbar_dirty = FALSE;
        mcview_set_buttonbar (view);
        buttonbar_redraw (find_buttonbar (view->widget.parent));
    }

    if (view->dirty > dirt_limit)
    {
        /* Too many updates skipped -> force a update */
        mcview_display (view);
        view->dirty = 0;
        /* Raise the update skipping limit */
        dirt_limit++;
        if (dirt_limit > mcview_max_dirt_limit)
            dirt_limit = mcview_max_dirt_limit;
    }
    else if (view->dirty > 0)
    {
        if (is_idle ())
        {
            /* We have time to update the screen properly */
            mcview_display (view);
            view->dirty = 0;
            if (dirt_limit > 1)
                dirt_limit--;
        }
        else
        {
            /* We are busy -> skipping full update,
               only the status line is updated */
            mcview_display_status (view);
        }
        /* Here we had a refresh, if fast scrolling does not work
           restore the refresh, although this should not happen */
    }
}

/* --------------------------------------------------------------------------------------------- */

/* Displays as much data from view->dpy_start as fits on the screen */
void
mcview_display (mcview_t * view)
{
    if (view->hex_mode)
    {
        mcview_display_hex (view);
    }
    else if (view->text_nroff_mode)
    {
        mcview_display_nroff (view);
    }
    else
    {
        mcview_display_text (view);
    }
    mcview_display_status (view);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_compute_areas (mcview_t * view)
{
    struct area view_area;
    screen_dimen height, rest, y;

    /* The viewer is surrounded by a frame of size view->dpy_frame_size.
     * Inside that frame, there are: The status line (at the top),
     * the data area and an optional ruler, which is shown above or
     * below the data area. */

    view_area.top = view->dpy_frame_size;
    view_area.left = view->dpy_frame_size;
    view_area.height = mcview_dimen_doz (view->widget.lines, 2 * view->dpy_frame_size);
    view_area.width = mcview_dimen_doz (view->widget.cols, 2 * view->dpy_frame_size);

    /* Most coordinates of the areas equal those of the whole viewer */
    view->status_area = view_area;
    view->ruler_area = view_area;
    view->data_area = view_area;

    /* Compute the heights of the areas */
    rest = view_area.height;

    height = mcview_dimen_min (rest, 1);
    view->status_area.height = height;
    rest -= height;

    height = mcview_dimen_min (rest, (ruler == RULER_NONE || view->hex_mode) ? 0 : 2);
    view->ruler_area.height = height;
    rest -= height;

    view->data_area.height = rest;

    /* Compute the position of the areas */
    y = view_area.top;

    view->status_area.top = y;
    y += view->status_area.height;

    if (ruler == RULER_TOP)
    {
        view->ruler_area.top = y;
        y += view->ruler_area.height;
    }

    view->data_area.top = y;
    y += view->data_area.height;

    if (ruler == RULER_BOTTOM)
        view->ruler_area.top = y;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_update_bytes_per_line (mcview_t * view)
{
    const screen_dimen cols = view->data_area.width;
    int bytes;

    if (cols < 8 + 17)
        bytes = 4;
    else
        bytes = 4 * ((cols - 8) / ((cols < 80) ? 17 : 18));
    assert (bytes != 0);

    view->bytes_per_line = bytes;
    view->dirty = mcview_max_dirt_limit + 1;    /* To force refresh */
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_display_toggle_ruler (mcview_t * view)
{
    static const enum ruler_type next[3] =
    {
        RULER_TOP,
        RULER_BOTTOM,
        RULER_NONE
    };

    assert ((size_t) ruler < 3);
    ruler = next[(size_t) ruler];
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_display_clean (mcview_t * view)
{
    tty_setcolor (NORMAL_COLOR);
    widget_erase ((Widget *) view);
    if (view->dpy_frame_size != 0)
    {
        tty_draw_box (view->widget.y, view->widget.x, view->widget.lines, view->widget.cols);
        /*        draw_double_box (view->widget.parent, view->widget.y,
           view->widget.x, view->widget.lines, view->widget.cols); */
    }
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_display_ruler (mcview_t * view)
{
    static const char ruler_chars[] = "|----*----";
    const screen_dimen top = view->ruler_area.top;
    const screen_dimen left = view->ruler_area.left;
    const screen_dimen width = view->ruler_area.width;
    const screen_dimen height = view->ruler_area.height;
    const screen_dimen line_row = (ruler == RULER_TOP) ? 0 : 1;
    const screen_dimen nums_row = (ruler == RULER_TOP) ? 1 : 0;

    char r_buff[10];
    off_t cl;
    screen_dimen c;

    if (ruler == RULER_NONE || height < 1)
        return;

    tty_setcolor (MARKED_COLOR);
    for (c = 0; c < width; c++)
    {
        cl = view->dpy_text_column + c;
        if (line_row < height)
        {
            widget_move (view, top + line_row, left + c);
            tty_print_char (ruler_chars[cl % 10]);
        }

        if ((cl != 0) && (cl % 10) == 0)
        {
            g_snprintf (r_buff, sizeof (r_buff), "%" OFFSETTYPE_PRId, (long unsigned int) cl);
            if (nums_row < height)
            {
                widget_move (view, top + nums_row, left + c - 1);
                tty_print_string (r_buff);
            }
        }
    }
    tty_setcolor (NORMAL_COLOR);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_percent (mcview_t * view, off_t p)
{
    const screen_dimen top = view->status_area.top;
    const screen_dimen right = view->status_area.left + view->status_area.width;
    const screen_dimen height = view->status_area.height;
    int percent;
    off_t filesize;

    if (height < 1 || right < 4)
        return;
    if (mcview_may_still_grow (view))
        return;
    filesize = mcview_get_filesize (view);

    if (filesize == 0 || view->dpy_end == filesize)
        percent = 100;
    else if (p > (INT_MAX / 100))
        percent = p / (filesize / 100);
    else
        percent = p * 100 / filesize;

    widget_move (view, top, right - 4);
    tty_printf ("%3d%%", percent);
}

/* --------------------------------------------------------------------------------------------- */
