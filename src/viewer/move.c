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

#include "../src/global.h"
#include "../src/tty/tty.h"
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
    if (view->hex_mode) {
        off_t bytes = lines * view->bytes_per_line;
        if (view->hex_cursor >= bytes) {
            view->hex_cursor -= bytes;
            if (view->hex_cursor < view->dpy_start)
                view->dpy_start = mcview_offset_doz (view->dpy_start, bytes);
        } else {
            view->hex_cursor %= view->bytes_per_line;
        }
    } else if (view->text_wrap_mode) {
        const screen_dimen width = view->data_area.width;
        off_t i, col, line, linestart;

        for (i = 0; i < lines; i++) {
            mcview_offset_to_coord (view, &line, &col, view->dpy_start);
            if (col >= width) {
                col -= width;
            } else if (line >= 1) {
                mcview_coord_to_offset (view, &linestart, line, 0);
                mcview_offset_to_coord (view, &line, &col, linestart - 1);

                /* if the only thing that would be displayed were a
                 * single newline character, advance to the previous
                 * part of the line. */
                if (col > 0 && col % width == 0)
                    col -= width;
                else
                    col -= col % width;
            } else {
                /* nothing to do */
            }
            mcview_coord_to_offset (view, &(view->dpy_start), line, col);
        }
    } else {
        off_t line, column;

        mcview_offset_to_coord (view, &line, &column, view->dpy_start);
        line = mcview_offset_doz (line, lines);
        mcview_coord_to_offset (view, &(view->dpy_start), line, column);
    }
    mcview_movement_fixups (view, (lines != 1));
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_move_down (mcview_t * view, off_t lines)
{
    if (view->hex_mode) {
        off_t i, limit, last_byte;

        last_byte = mcview_get_filesize (view);
        if (last_byte >= (off_t) view->bytes_per_line)
            limit = last_byte - view->bytes_per_line;
        else
            limit = 0;
        for (i = 0; i < lines && view->hex_cursor < limit; i++) {
            view->hex_cursor += view->bytes_per_line;
            if (lines != 1)
                view->dpy_start += view->bytes_per_line;
        }

    } else if (view->dpy_end == mcview_get_filesize (view)) {
        /* don't move further down. There's nothing more to see. */

    } else if (view->text_wrap_mode) {
        off_t line, col, i;
        int c;

        for (i = 0; i < lines; i++) {
            off_t new_offset, chk_line, chk_col;

            mcview_offset_to_coord (view, &line, &col, view->dpy_start);
            col += view->data_area.width;
            mcview_coord_to_offset (view, &new_offset, line, col);

            /* skip to the next line if the only thing that would be
             * displayed is the newline character. */
            mcview_offset_to_coord (view, &chk_line, &chk_col, new_offset);
            if (chk_line == line && chk_col == col && mcview_get_byte (view, new_offset, &c) == TRUE
                && c == '\n')
                new_offset++;

            view->dpy_start = new_offset;
        }

    } else {
        off_t line, col;

        mcview_offset_to_coord (view, &line, &col, view->dpy_start);
        line += lines;
        mcview_coord_to_offset (view, &(view->dpy_start), line, col);
    }
    mcview_movement_fixups (view, (lines != 1));
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_move_left (mcview_t * view, off_t columns)
{
    if (view->hex_mode) {
        assert (columns == 1);
        if (view->hexview_in_text || !view->hexedit_lownibble) {
            if (view->hex_cursor > 0)
                view->hex_cursor--;
        }
        if (!view->hexview_in_text)
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
        assert (columns == 1);
        if (view->hexview_in_text || view->hexedit_lownibble) {
            view->hex_cursor++;
        }
        if (!view->hexview_in_text)
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
    } else if (view->text_wrap_mode) {
        off_t line, col, columns;

        columns = view->data_area.width;
        mcview_offset_to_coord (view, &line, &col, view->dpy_start + view->dpy_text_column);
        if (columns != 0)
            col = mcview_offset_rounddown (col, columns);
        mcview_coord_to_offset (view, &(view->dpy_start), line, col);
        view->dpy_text_column = 0;
    } else {
        /* nothing to do */
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

    if (view->growbuf_in_use)
        mcview_growbuf_read_until (view, OFFSETTYPE_MAX);

    filesize = mcview_get_filesize (view);
    last_offset = mcview_offset_doz (filesize, 1);
    datalines = view->data_area.height;
    lines_up = mcview_offset_doz (datalines, 1);

    if (view->hex_mode) {
        view->hex_cursor = filesize;
        mcview_move_up (view, lines_up);
        view->hex_cursor = last_offset;
    } else {
        view->dpy_start = last_offset;
        mcview_moveto_bol (view);
        mcview_move_up (view, lines_up);
    }
    mcview_movement_fixups (view, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_moveto_bol (mcview_t * view)
{
    if (view->hex_mode) {
        view->hex_cursor -= view->hex_cursor % view->bytes_per_line;
    } else if (view->text_wrap_mode) {
        /* do nothing */
    } else {
        off_t line, column;
        mcview_offset_to_coord (view, &line, &column, view->dpy_start);
        mcview_coord_to_offset (view, &(view->dpy_start), line, 0);
        view->dpy_text_column = 0;
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
    } else if (view->text_wrap_mode) {
        /* nothing to do */
    } else {
        off_t line, col;

        mcview_offset_to_coord (view, &line, &col, view->dpy_start);
        mcview_coord_to_offset (view, &(view->dpy_start), line, OFFSETTYPE_MAX);
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
    struct coord_cache_entry coord;

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
    struct coord_cache_entry coord;

    coord.cc_offset = offset;
    mcview_ccache_lookup (view, &coord, CCACHE_LINECOL);

    if (ret_line)
        *ret_line = coord.cc_line;

    if (ret_column)
        *ret_column = (view->text_nroff_mode)
            ? coord.cc_nroff_column : coord.cc_column;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_place_cursor (mcview_t * view)
{
    const screen_dimen top = view->data_area.top;
    const screen_dimen left = view->data_area.left;
    screen_dimen col;

    col = view->cursor_col;
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
    off_t search_line, search_col, offset;

    mcview_offset_to_coord (view, &search_line, &search_col, view->search_start);
    if (!view->hex_mode)
        search_col = 0;

    mcview_coord_to_offset (view, &offset, search_line, search_col);

    if (view->hex_mode) {
        view->hex_cursor = offset;
        view->dpy_start = offset - offset % view->bytes_per_line;
    } else {
        view->dpy_start = offset;
    }

    mcview_scroll_to_cursor (view);
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */
