/*
   Internal file viewer for the Midnight Commander
   Function for whow info on display

   Copyright (C) 1994-2024
   Free Software Foundation, Inc.

   Written by:
   Miguel de Icaza, 1994, 1995, 1998
   Janne Kukonlehto, 1994, 1995
   Jakub Jelinek, 1995
   Joseph M. Hinkle, 1996
   Norbert Warmuth, 1997
   Pavel Machek, 1998
   Roland Illig <roland.illig@gmx.de>, 2004, 2005
   Slava Zanko <slavazanko@google.com>, 2009
   Andrew Borodin <aborodin@vmail.ru>, 2009-2022
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
#include "src/keymap.h"

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define BUF_TRUNC_LEN 5         /* The length of the line displays the file size */

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* If set, show a ruler */
static enum ruler_type
{
    RULER_NONE,
    RULER_TOP,
    RULER_BOTTOM
} ruler = RULER_NONE;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/** Define labels and handlers for functional keys */

static void
mcview_set_buttonbar (WView * view)
{
    Widget *w = WIDGET (view);
    WDialog *h = DIALOG (w->owner);
    WButtonBar *b;
    const global_keymap_t *keymap = view->mode_flags.hex ? view->hex_keymap : w->keymap;

    b = buttonbar_find (h);
    buttonbar_set_label (b, 1, Q_ ("ButtonBar|Help"), keymap, w);

    if (view->mode_flags.hex)
    {
        if (view->hexedit_mode)
            buttonbar_set_label (b, 2, Q_ ("ButtonBar|View"), keymap, w);
        else if (view->datasource == DS_FILE)
            buttonbar_set_label (b, 2, Q_ ("ButtonBar|Edit"), keymap, w);
        else
            buttonbar_set_label (b, 2, "", keymap, WIDGET (view));

        buttonbar_set_label (b, 4, Q_ ("ButtonBar|Ascii"), keymap, w);
        buttonbar_set_label (b, 6, Q_ ("ButtonBar|Save"), keymap, w);
        buttonbar_set_label (b, 7, Q_ ("ButtonBar|HxSrch"), keymap, w);

    }
    else
    {
        buttonbar_set_label (b, 2, view->mode_flags.wrap ? Q_ ("ButtonBar|UnWrap")
                             : Q_ ("ButtonBar|Wrap"), keymap, w);
        buttonbar_set_label (b, 4, Q_ ("ButtonBar|Hex"), keymap, w);
        buttonbar_set_label (b, 6, "", keymap, WIDGET (view));
        buttonbar_set_label (b, 7, Q_ ("ButtonBar|Search"), keymap, w);
    }

    buttonbar_set_label (b, 5, Q_ ("ButtonBar|Goto"), keymap, w);
    buttonbar_set_label (b, 8, view->mode_flags.magic ? Q_ ("ButtonBar|Raw")
                         : Q_ ("ButtonBar|Parse"), keymap, w);

    if (!mcview_is_in_panel (view))     /* don't override some panel buttonbar keys  */
    {
        buttonbar_set_label (b, 3, Q_ ("ButtonBar|Quit"), keymap, w);
        buttonbar_set_label (b, 9, view->mode_flags.nroff ? Q_ ("ButtonBar|Unform")
                             : Q_ ("ButtonBar|Format"), keymap, w);
        buttonbar_set_label (b, 10, Q_ ("ButtonBar|Quit"), keymap, w);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
mcview_display_percent (WView * view, off_t p)
{
    int percent;

    percent = mcview_calc_percent (view, p);
    if (percent >= 0)
    {
        int top = view->status_area.y;
        int right;

        right = view->status_area.x + view->status_area.cols;
        widget_gotoyx (view, top, right - 4);
        tty_printf ("%3d%%", percent);
        /* avoid cursor wrapping in NCurses-base MC */
        widget_gotoyx (view, top, right - 1);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
mcview_display_status (WView * view)
{
    const WRect *r = &view->status_area;
    const char *file_label;

    if (r->lines < 1)
        return;

    tty_setcolor (STATUSBAR_COLOR);
    tty_draw_hline (WIDGET (view)->rect.y + r->y, WIDGET (view)->rect.x + r->x, ' ', r->cols);

    file_label =
        view->filename_vpath != NULL ?
        vfs_path_get_last_path_str (view->filename_vpath) : view->command != NULL ?
        view->command : "";

    if (r->cols > 40)
    {
        widget_gotoyx (view, r->y, r->cols - 32);
        if (view->mode_flags.hex)
            tty_printf ("0x%08" PRIxMAX, (uintmax_t) view->hex_cursor);
        else
        {
            char buffer[BUF_TRUNC_LEN + 1];

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
    widget_gotoyx (view, r->y, r->x);
    if (r->cols > 40)
        tty_print_string (str_fit_to_term (file_label, r->cols - 34, J_LEFT_FIT));
    else
        tty_print_string (str_fit_to_term (file_label, r->cols - 5, J_LEFT_FIT));
    if (r->cols > 26)
        mcview_display_percent (view, view->mode_flags.hex ? view->hex_cursor : view->dpy_end);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mcview_update (WView * view)
{
    static int dirt_limit = 1;

    if (view->dpy_bbar_dirty)
    {
        view->dpy_bbar_dirty = FALSE;
        mcview_set_buttonbar (view);
        widget_draw (WIDGET (buttonbar_find (DIALOG (WIDGET (view)->owner))));
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
mcview_display (WView * view)
{
    if (view->mode_flags.hex)
        mcview_display_hex (view);
    else
        mcview_display_text (view);
    mcview_display_status (view);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_compute_areas (WView * view)
{
    WRect view_area;
    int height, rest, y;

    /* The viewer is surrounded by a frame of size view->dpy_frame_size.
     * Inside that frame, there are: The status line (at the top),
     * the data area and an optional ruler, which is shown above or
     * below the data area. */

    view_area.y = view->dpy_frame_size;
    view_area.x = view->dpy_frame_size;
    view_area.lines = DOZ (WIDGET (view)->rect.lines, 2 * view->dpy_frame_size);
    view_area.cols = DOZ (WIDGET (view)->rect.cols, 2 * view->dpy_frame_size);

    /* Most coordinates of the areas equal those of the whole viewer */
    view->status_area = view_area;
    view->ruler_area = view_area;
    view->data_area = view_area;

    /* Compute the heights of the areas */
    rest = view_area.lines;

    height = MIN (rest, 1);
    view->status_area.lines = height;
    rest -= height;

    height = (ruler == RULER_NONE || view->mode_flags.hex) ? 0 : 2;
    height = MIN (rest, height);
    view->ruler_area.lines = height;
    rest -= height;

    view->data_area.lines = rest;

    /* Compute the position of the areas */
    y = view_area.y;

    view->status_area.y = y;
    y += view->status_area.lines;

    if (ruler == RULER_TOP)
    {
        view->ruler_area.y = y;
        y += view->ruler_area.lines;
    }

    view->data_area.y = y;
    y += view->data_area.lines;

    if (ruler == RULER_BOTTOM)
        view->ruler_area.y = y;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_update_bytes_per_line (WView * view)
{
    int cols = view->data_area.cols;
    int bytes;

    if (cols < 9 + 17)
        bytes = 4;
    else
        bytes = 4 * ((cols - 9) / ((cols <= 80) ? 17 : 18));

    g_assert (bytes != 0);

    view->bytes_per_line = bytes;
    view->dirty = mcview_max_dirt_limit + 1;    /* To force refresh */
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_display_toggle_ruler (WView * view)
{
    static const enum ruler_type next[3] =
    {
        RULER_TOP,
        RULER_BOTTOM,
        RULER_NONE
    };

    g_assert ((size_t) ruler < 3);

    ruler = next[(size_t) ruler];
    mcview_compute_areas (view);
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_display_clean (WView * view)
{
    Widget *w = WIDGET (view);

    tty_setcolor (VIEW_NORMAL_COLOR);
    widget_erase (w);
    if (view->dpy_frame_size != 0)
        tty_draw_box (w->rect.y, w->rect.x, w->rect.lines, w->rect.cols, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_display_ruler (WView * view)
{
    static const char ruler_chars[] = "|----*----";
    const WRect *r = &view->ruler_area;
    const int line_row = (ruler == RULER_TOP) ? 0 : 1;
    const int nums_row = (ruler == RULER_TOP) ? 1 : 0;

    char r_buff[10];
    off_t cl;
    int c;

    if (ruler == RULER_NONE || r->lines < 1)
        return;

    tty_setcolor (VIEW_BOLD_COLOR);
    for (c = 0; c < r->cols; c++)
    {
        cl = view->dpy_text_column + c;
        if (line_row < r->lines)
        {
            widget_gotoyx (view, r->y + line_row, r->x + c);
            tty_print_char (ruler_chars[cl % 10]);
        }

        if ((cl != 0) && (cl % 10) == 0)
        {
            g_snprintf (r_buff, sizeof (r_buff), "%" PRIuMAX, (uintmax_t) cl);
            if (nums_row < r->lines)
            {
                widget_gotoyx (view, r->y + nums_row, r->x + c - 1);
                tty_print_string (r_buff);
            }
        }
    }
    tty_setcolor (VIEW_NORMAL_COLOR);
}

/* --------------------------------------------------------------------------------------------- */
