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
	       2009 Ilia Maslakov <il.smind@gmail.com>

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

#include "../src/global.h"
#include "../src/skin/skin.h"
#include "../src/tty/tty.h"
#include "../src/tty/key.h"
#include "../src/strutil.h"
#include "../src/main.h"
#include "../src/dialog.h"		/* Dlg_head */
#include "../src/widget.h"		/* WButtonBar */

#include "internal.h"
#include "mcviewer.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* If set, show a ruler */
static enum ruler_type {
    RULER_NONE,
    RULER_TOP,
    RULER_BOTTOM
} ruler = RULER_NONE;

/*** file scope functions ************************************************************************/

/* --------------------------------------------------------------------------------------------- */

/* Define labels and handlers for functional keys */
static void
mcview_set_buttonbar (mcview_t *view)
{
    const char *text;
    Dlg_head *h = view->widget.parent;
    WButtonBar *b = find_buttonbar (h);

    buttonbar_set_label (b, 1, Q_ ("ButtonBar|Help"), mcview_help_cmd);

    if (view->hex_mode) {
        if (view->hexedit_mode)
            buttonbar_set_label_data (b, 2, Q_ ("ButtonBar|View"),
                                        (buttonbarfn) mcview_toggle_hexedit_mode_cmd, view);
        else if (view->datasource == DS_FILE)
            buttonbar_set_label_data (b, 2, Q_ ("ButtonBar|Edit"),
                                        (buttonbarfn) mcview_toggle_hexedit_mode_cmd, view);
        else
            buttonbar_clear_label (b, 2);

        buttonbar_set_label_data (b, 4, Q_ ("ButtonBar|Ascii"),
                                    (buttonbarfn) mcview_toggle_hex_mode_cmd, view);
        buttonbar_set_label_data (b, 6, Q_ ("ButtonBar|Save"),
                                    (buttonbarfn) mcview_hexedit_save_changes_cmd, view);
        buttonbar_set_label_data (b, 7, Q_ ("ButtonBar|HxSrch"),
                                    (buttonbarfn) mcview_search_cmd, view);
    } else {
        text = view->text_wrap_mode ? Q_ ("ButtonBar|UnWrap") : Q_ ("ButtonBar|Wrap");
        buttonbar_set_label_data (b, 2, text,
                                    (buttonbarfn) mcview_toggle_wrap_mode_cmd, view);
        buttonbar_set_label_data (b, 4, Q_ ("ButtonBar|Hex"),
                                    (buttonbarfn) mcview_toggle_hex_mode_cmd, view);
        buttonbar_clear_label (b, 6);
        buttonbar_set_label_data (b, 7, Q_ ("ButtonBar|Search"),
                                    (buttonbarfn) mcview_search_cmd, view);
    }

    buttonbar_set_label_data (b, 5, Q_ ("ButtonBar|Goto"),
                                    (buttonbarfn) mcview_goto, view);

    /* don't override the key to access the main menu */
    if (!mcview_is_in_panel (view)) {
        buttonbar_set_label_data (b, 3, Q_ ("ButtonBar|Quit"),
                                    (buttonbarfn) mcview_quit_cmd, view);

        text = view->text_nroff_mode ? Q_ ("ButtonBar|Unform") : Q_ ("ButtonBar|Format");
        buttonbar_set_label_data (b, 9, text,
                                    (buttonbarfn) mcview_toggle_nroff_mode_cmd, view);
    }

    text = view->magic_mode ? Q_ ("ButtonBar|Raw") : Q_ ("ButtonBar|Parse");
    buttonbar_set_label_data (b, 8, text,
                                (buttonbarfn) mcview_toggle_magic_mode_cmd, view);

    buttonbar_set_label_data (b, 10, Q_ ("ButtonBar|Quit"),
                                (buttonbarfn) mcview_quit_cmd, view);
}

/* --------------------------------------------------------------------------------------------- */


static void
mcview_display_status (mcview_t * view)
{
    const screen_dimen top = view->status_area.top;
    const screen_dimen left = view->status_area.left;
    const screen_dimen width = view->status_area.width;
    const screen_dimen height = view->status_area.height;
    const char *file_label, *file_name;
    screen_dimen file_label_width;
    int i;
    char *tmp;

    if (height < 1)
        return;

    tty_setcolor (SELECTED_COLOR);
    widget_move (view, top, left);
    tty_draw_hline (top, left, ' ', width);

    file_label = _("File: %s");
    file_label_width = str_term_width1 (file_label) - 2;
    file_name = view->filename ? view->filename : view->command ? view->command : "";

    if (width < file_label_width + 6)
        tty_print_string (str_fit_to_term (file_name, width, J_LEFT_FIT));
    else {
        i = (width > 22 ? 22 : width) - file_label_width;

        tmp = g_strdup_printf (file_label, str_fit_to_term (file_name, i, J_LEFT_FIT));
        tty_print_string (tmp);
        g_free (tmp);
        if (width > 46) {
            widget_move (view, top, left + 24);
            /* FIXME: the format strings need to be changed when off_t changes */
            if (view->hex_mode)
                tty_printf (_("Offset 0x%08lx"), (unsigned long) view->hex_cursor);
            else {
                off_t line, col;
                mcview_offset_to_coord (view, &line, &col, view->dpy_start);
                tty_printf (_("Line %lu Col %lu"),
                            (unsigned long) line + 1,
                            (unsigned long) (view->text_wrap_mode ? col : view->dpy_text_column));
            }
        }
        if (width > 62) {
            off_t filesize;
            filesize = mcview_get_filesize (view);
            widget_move (view, top, left + 43);
            if (!mcview_may_still_grow (view)) {
                tty_printf (_("%s bytes"), size_trunc (filesize));
            } else {
                tty_printf (_(">= %s bytes"), size_trunc (filesize));
            }
        }
        if (width > 26) {
            mcview_percent (view, view->hex_mode ? view->hex_cursor : view->dpy_end);
        }
    }
    tty_setcolor (SELECTED_COLOR);
}

/* --------------------------------------------------------------------------------------------- */

/*** public functions ****************************************************************************/

/* --------------------------------------------------------------------------------------------- */

void
mcview_update (mcview_t * view)
{
    static int dirt_limit = 1;

    if (view->dpy_bbar_dirty) {
        view->dpy_bbar_dirty = FALSE;
        mcview_set_buttonbar (view);
        buttonbar_redraw (find_buttonbar (view->widget.parent));
    }

    if (view->dirty > dirt_limit) {
        /* Too many updates skipped -> force a update */
        mcview_display (view);
        view->dirty = 0;
        /* Raise the update skipping limit */
        dirt_limit++;
        if (dirt_limit > mcview_max_dirt_limit)
            dirt_limit = mcview_max_dirt_limit;
    }
    if (view->dirty) {
        if (is_idle ()) {
            /* We have time to update the screen properly */
            mcview_display (view);
            view->dirty = 0;
            if (dirt_limit > 1)
                dirt_limit--;
        } else {
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
    mcview_compute_areas (view);
    if (view->hex_mode) {
        mcview_display_hex (view);
    } else if (view->text_nroff_mode) {
        mcview_display_nroff (view);
    } else {
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

    if (ruler == RULER_TOP) {
        view->ruler_area.top = y;
        y += view->ruler_area.height;
    }

    view->data_area.top = y;
    y += view->data_area.height;

    if (ruler == RULER_BOTTOM) {
        view->ruler_area.top = y;
        y += view->ruler_area.height;
    }
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
    static const enum ruler_type next[3] = {
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
    if (view->dpy_frame_size != 0) {
        tty_draw_box (view->widget.y, view->widget.x, view->widget.lines, view->widget.cols);
/*        draw_double_box (view->widget.parent, view->widget.y,
                         view->widget.x, view->widget.lines, view->widget.cols);*/
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
    for (c = 0; c < width; c++) {
        cl = view->dpy_text_column + c;
        if (line_row < height) {
            widget_move (view, top + line_row, left + c);
            tty_print_char (ruler_chars[cl % 10]);
        }

        if ((cl != 0) && (cl % 10) == 0) {
            g_snprintf (r_buff, sizeof (r_buff), "%" OFFSETTYPE_PRId, (long unsigned int) cl);
            if (nums_row < height) {
                widget_move (view, top + nums_row, left + c - 1);
                tty_print_string (r_buff);
            }
        }
    }
    tty_setcolor (NORMAL_COLOR);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_adjust_size (Dlg_head *h)
{
    mcview_t *view;
    WButtonBar *b;

    /* Look up the viewer and the buttonbar, we assume only two widgets here */
    view = (mcview_t *) find_widget_type (h, mcview_callback);
    b = find_buttonbar (h);

    widget_set_size (&view->widget, 0, 0, LINES - 1, COLS);
    widget_set_size (&b->widget , LINES - 1, 0, 1, COLS);

    mcview_compute_areas (view);
    mcview_update_bytes_per_line (view);
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
