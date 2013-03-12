/*
   Internal file viewer for the Midnight Commander
   Function for whow info on display

   Copyright (C) 1994, 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003,
   2004, 2005, 2006, 2007, 2009, 2011
   The Free Software Foundation, Inc.

   Written by:
   Miguel de Icaza, 1994, 1995, 1998
   Janne Kukonlehto, 1994, 1995
   Jakub Jelinek, 1995
   Joseph M. Hinkle, 1996
   Norbert Warmuth, 1997
   Pavel Machek, 1998
   Roland Illig <roland.illig@gmx.de>, 2004, 2005
   Slava Zanko <slavazanko@google.com>, 2009
   Andrew Borodin <aborodin@vmail.ru>, 2009
   Ilia Maslakov <il.smind@gmail.com>, 2009, 2010

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

#include <config.h>
#include <inttypes.h>           /* uintmax_t */

#include "lib/global.h"
#include "lib/skin.h"
#include "lib/tty/tty.h"
#include "lib/tty/key.h"
#include "lib/strutil.h"
#include "lib/util.h"
#include "lib/widget.h"
#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#endif

#include "src/setup.h"          /* panels_options */
#include "src/keybind-defaults.h"

#include "internal.h"
#include "mcviewer.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define BUF_TRUNC_LEN 5         /* The length of the line displays the file size */

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

/** Define labels and handlers for functional keys */

static void
mcview_set_buttonbar (mcview_t * view)
{
    WDialog *h = WIDGET (view)->owner;
    WButtonBar *b = find_buttonbar (h);
    const global_keymap_t *keymap = view->hex_mode ? viewer_hex_map : viewer_map;

    buttonbar_set_label (b, 1, Q_ ("ButtonBar|Help"), keymap, WIDGET (view));

    if (view->hex_mode)
    {
        if (view->hexedit_mode)
            buttonbar_set_label (b, 2, Q_ ("ButtonBar|View"), keymap, WIDGET (view));
        else if (view->datasource == DS_FILE)
            buttonbar_set_label (b, 2, Q_ ("ButtonBar|Edit"), keymap, WIDGET (view));
        else
            buttonbar_set_label (b, 2, "", keymap, WIDGET (view));

        buttonbar_set_label (b, 4, Q_ ("ButtonBar|Ascii"), keymap, WIDGET (view));
        buttonbar_set_label (b, 6, Q_ ("ButtonBar|Save"), keymap, WIDGET (view));
        buttonbar_set_label (b, 7, Q_ ("ButtonBar|HxSrch"), keymap, WIDGET (view));

    }
    else
    {
        buttonbar_set_label (b, 2, view->text_wrap_mode ? Q_ ("ButtonBar|UnWrap")
                             : Q_ ("ButtonBar|Wrap"), keymap, WIDGET (view));
        buttonbar_set_label (b, 4, Q_ ("ButtonBar|Hex"), keymap, WIDGET (view));
        buttonbar_set_label (b, 6, "", keymap, WIDGET (view));
        buttonbar_set_label (b, 7, Q_ ("ButtonBar|Search"), keymap, WIDGET (view));
    }

    buttonbar_set_label (b, 5, Q_ ("ButtonBar|Goto"), keymap, WIDGET (view));
    buttonbar_set_label (b, 8, view->magic_mode ? Q_ ("ButtonBar|Raw")
                         : Q_ ("ButtonBar|Parse"), keymap, WIDGET (view));

    if (mcview_is_in_panel (view))
        buttonbar_set_label (b, 10, "", keymap, WIDGET (view));
    else
    {
        /* don't override some panel buttonbar keys  */
        buttonbar_set_label (b, 3, Q_ ("ButtonBar|Quit"), keymap, WIDGET (view));
        buttonbar_set_label (b, 9, view->text_nroff_mode ? Q_ ("ButtonBar|Unform")
                             : Q_ ("ButtonBar|Format"), keymap, WIDGET (view));
        buttonbar_set_label (b, 10, Q_ ("ButtonBar|Quit"), keymap, WIDGET (view));
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

    if (height < 1)
        return;

    tty_setcolor (STATUSBAR_COLOR);
    tty_draw_hline (WIDGET (view)->y + top, WIDGET (view)->x + left, ' ', width);

    file_label =
        view->filename_vpath != NULL ?
        vfs_path_get_last_path_str (view->filename_vpath) : view->command != NULL ?
        view->command : "";

    if (width > 40)
    {
        char buffer[BUF_TRUNC_LEN + 1];

        widget_move (view, top, width - 32);
        if (view->hex_mode)
            tty_printf ("0x%08" PRIxMAX, (uintmax_t) view->hex_cursor);
        else
        {
            size_trunc_len (buffer, BUF_TRUNC_LEN, mcview_get_filesize (view), 0,
                            panels_options.kilobyte_si);
            tty_printf ("%9" PRIuMAX "/%s%s %s", (uintmax_t) view->dpy_end,
                        buffer, mcview_may_still_grow (view) ? "+" : " ",
#ifdef HAVE_CHARSET
                        mc_global.source_codepage >= 0 ?
                        get_codepage_id (mc_global.source_codepage) :
#endif
                        "");
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
        widget_redraw (WIDGET (find_buttonbar (WIDGET (view)->owner)));
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
/** Displays as much data from view->dpy_start as fits on the screen */

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
    view_area.height = mcview_dimen_doz (WIDGET (view)->lines, 2 * view->dpy_frame_size);
    view_area.width = mcview_dimen_doz (WIDGET (view)->cols, 2 * view->dpy_frame_size);

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
#ifdef HAVE_ASSERT_H
    assert (bytes != 0);
#endif

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

#ifdef HAVE_ASSERT_H
    assert ((size_t) ruler < 3);
#endif
    ruler = next[(size_t) ruler];
    mcview_compute_areas (view);
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_display_clean (mcview_t * view)
{
    Widget *w = WIDGET (view);

    tty_setcolor (NORMAL_COLOR);
    widget_erase (w);
    if (view->dpy_frame_size != 0)
        tty_draw_box (w->y, w->x, w->lines, w->cols, FALSE);
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

    tty_setcolor (VIEW_BOLD_COLOR);
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
            g_snprintf (r_buff, sizeof (r_buff), "%" PRIuMAX, (uintmax_t) cl);
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
