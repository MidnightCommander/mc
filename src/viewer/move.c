/*
   Internal file viewer for the Midnight Commander
   Functions for handle cursor movement

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

/*
   The following variables have to do with the current position and are
   updated by the cursor movement functions.

   In hex view and wrapped text view mode, dpy_start marks the offset of
   the top-left corner on the screen, in non-wrapping text mode it is
   the beginning of the current line.  In hex mode, hex_cursor is the
   offset of the cursor.  In non-wrapping text mode, dpy_text_column is
   the number of columns that are hidden on the left side on the screen.

   In hex mode, dpy_start is updated by the view_fix_cursor_position()
   function in order to keep the other functions simple.  In
   non-wrapping text mode dpy_start and dpy_text_column are normalized
   such that dpy_text_column < view_get_datacolumns().
 */

#include <config.h>

#include "src/global.h"
#include "lib/tty/tty.h"
#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

static void
mcview_movement_fixups (mcview_t * view, gboolean reset_search)
{
    mcview_scroll_to_cursor (view);
    if (reset_search) {
        view->search_start = view->dpy_start;
        view->search_end = view->dpy_start;
    }
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */

/*** public functions ****************************************************************************/

/* --------------------------------------------------------------------------------------------- */

void
mcview_move_up (mcview_t * view, off_t lines)
{
    off_t new_offset;
    if (view->hex_mode) {
        off_t bytes = lines * view->bytes_per_line;
        if (view->hex_cursor >= bytes) {
            view->hex_cursor -= bytes;
            if (view->hex_cursor < view->dpy_start)
                view->dpy_start = mcview_offset_doz (view->dpy_start, bytes);
        } else {
            view->hex_cursor %= view->bytes_per_line;
        }
    } else {
        off_t i;
        for (i = 0; i < lines; i++) {
            new_offset = mcview_bol (view, view->dpy_start);
            if (new_offset > 0)
                new_offset--;
            new_offset = mcview_bol (view, new_offset);
            if (new_offset < 0)
                new_offset = 0;
            view->dpy_start = new_offset;
        }
    }
    mcview_movement_fixups (view, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_move_down (mcview_t * view, off_t lines)
{
    off_t last_byte;
    last_byte = mcview_get_filesize (view);
    if (view->hex_mode) {
        off_t i, limit;

        if (last_byte >= (off_t) view->bytes_per_line)
            limit = last_byte - view->bytes_per_line;
        else
            limit = 0;
        for (i = 0; i < lines && view->hex_cursor < limit; i++) {
            view->hex_cursor += view->bytes_per_line;
            if (lines != 1)
                view->dpy_start += view->bytes_per_line;
        }
    } else {
        off_t i;
        for (i = 0; i < lines; i++) {
            off_t new_offset;
            new_offset = mcview_eol (view, view->dpy_start);
            view->dpy_start = new_offset;
        }
    }
    mcview_movement_fixups (view, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_move_left (mcview_t * view, off_t columns)
{
    if (view->hex_mode) {
        off_t old_cursor = view->hex_cursor;
        assert (columns == 1);
        if (view->hexview_in_text || !view->hexedit_lownibble) {
            if (view->hex_cursor > 0)
                view->hex_cursor--;
        }
        if (!view->hexview_in_text)
            if (old_cursor > 0 || view->hexedit_lownibble)
                view->hexedit_lownibble = !view->hexedit_lownibble;
    } else if (view->text_wrap_mode) {
        /* nothing to do */
    } else {
        if (view->dpy_text_column >= columns)
            view->dpy_text_column -= columns;
        else
            view->dpy_text_column = 0;
    }
    mcview_movement_fixups (view, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_move_right (mcview_t * view, off_t columns)
{
    if (view->hex_mode) {
        off_t last_byte;
        off_t old_cursor = view->hex_cursor;
        last_byte = mcview_offset_doz(mcview_get_filesize (view), 1);
        assert (columns == 1);
        if (view->hexview_in_text || view->hexedit_lownibble) {
            if (view->hex_cursor < last_byte)
                view->hex_cursor++;
        }
        if (!view->hexview_in_text)
            if (old_cursor < last_byte || !view->hexedit_lownibble)
                view->hexedit_lownibble = !view->hexedit_lownibble;
    } else if (view->text_wrap_mode) {
        /* nothing to do */
    } else {
        view->dpy_text_column += columns;
    }
    mcview_movement_fixups (view, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_scroll_to_cursor (mcview_t * view)
{
    if (view->hex_mode) {
        const off_t bytes = view->bytes_per_line;
        const off_t displaysize = view->data_area.height * bytes;
        const off_t cursor = view->hex_cursor;
        off_t topleft = view->dpy_start;

        if (topleft + displaysize <= cursor)
            topleft = mcview_offset_rounddown (cursor, bytes)
                - (displaysize - bytes);
        if (cursor < topleft)
            topleft = mcview_offset_rounddown (cursor, bytes);
        view->dpy_start = topleft;
    }
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_moveto_top (mcview_t * view)
{
    view->dpy_start = 0;
    view->hex_cursor = 0;
    view->dpy_text_column = 0;
    mcview_movement_fixups (view, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_moveto_bottom (mcview_t * view)
{
    off_t datalines, lines_up, filesize, last_offset;

    mcview_update_filesize (view);

    if (view->growbuf_in_use)
        mcview_growbuf_read_until (view, OFFSETTYPE_MAX);

    filesize = mcview_get_filesize (view);
    datalines = view->data_area.height;
    lines_up = mcview_offset_doz (datalines, 1);

    if (view->hex_mode) {
        last_offset = mcview_offset_doz (filesize, 1);
        view->hex_cursor = filesize;
        mcview_move_up (view, lines_up);
        view->hex_cursor = last_offset;
    } else {
        view->dpy_start = filesize;
        mcview_move_up (view, 1);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_moveto_bol (mcview_t * view)
{
    if (view->hex_mode) {
        view->hex_cursor -= view->hex_cursor % view->bytes_per_line;
    } else if (!view->text_wrap_mode) {
        view->dpy_start = mcview_bol (view, view->dpy_start);
    }
    mcview_movement_fixups (view, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_moveto_eol (mcview_t * view)
{
    if (view->hex_mode) {
        off_t filesize, bol;

        bol = mcview_offset_rounddown (view->hex_cursor, view->bytes_per_line);
        if (mcview_get_byte_indexed (view, bol, view->bytes_per_line - 1, NULL) == TRUE) {
            view->hex_cursor = bol + view->bytes_per_line - 1;
        } else {
            filesize = mcview_get_filesize (view);
            view->hex_cursor = mcview_offset_doz (filesize, 1);
        }
    }
    mcview_movement_fixups (view, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_moveto_offset (mcview_t * view, off_t offset)
{
    if (view->hex_mode) {
        view->hex_cursor = offset;
        view->dpy_start = offset - offset % view->bytes_per_line;
    } else {
        view->dpy_start = offset;
    }
    mcview_movement_fixups (view, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_moveto (mcview_t * view, off_t line, off_t col)
{
    off_t offset;

    mcview_coord_to_offset (view, &offset, line, col);
    mcview_moveto_offset (view, offset);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_coord_to_offset (mcview_t * view, off_t * ret_offset, off_t line, off_t column)
{
    coord_cache_entry_t coord;

    coord.cc_line = line;
    coord.cc_column = column;
    coord.cc_nroff_column = column;
    mcview_ccache_lookup (view, &coord, CCACHE_OFFSET);
    *ret_offset = coord.cc_offset;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_offset_to_coord (mcview_t * view, off_t * ret_line, off_t * ret_column, off_t offset)
{
    coord_cache_entry_t coord;

    coord.cc_offset = offset;
    mcview_ccache_lookup (view, &coord, CCACHE_LINECOL);

    *ret_line = coord.cc_line;
    *ret_column = (view->text_nroff_mode)
                    ? coord.cc_nroff_column : coord.cc_column;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_place_cursor (mcview_t * view)
{
    const screen_dimen top = view->data_area.top;
    const screen_dimen left = view->data_area.left;
    screen_dimen col = view->cursor_col;
    if (!view->hexview_in_text && view->hexedit_lownibble)
        col++;
    widget_move (&view->widget, top + view->cursor_row, left + col);
}

/* --------------------------------------------------------------------------------------------- */

/* we have set view->search_start and view->search_end and must set 
 * view->dpy_text_column and view->dpy_start
 * try to display maximum of match */
void
mcview_moveto_match (mcview_t * view)
{
    off_t offset;

    offset = view->search_start;

    if (view->hex_mode) {
        view->hex_cursor = offset;
        view->dpy_start = offset - offset % view->bytes_per_line;
    } else {
        view->dpy_start = mcview_bol (view, offset);
    }

    mcview_scroll_to_cursor (view);
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */
