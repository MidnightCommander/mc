/*
   Internal file viewer for the Midnight Commander
   Functions for handle cursor movement

   Copyright (C) 1994-2014
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
   Andrew Borodin <aborodin@vmail.ru>, 2009, 2013
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

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
mcview_scroll_to_cursor (mcview_t * view)
{
    if (view->hex_mode)
    {
        off_t bytes = view->bytes_per_line;
        off_t cursor = view->hex_cursor;
        off_t topleft = view->dpy_start;
        off_t displaysize;

        displaysize = view->data_area.height * bytes;
        if (topleft + displaysize <= cursor)
            topleft = mcview_offset_rounddown (cursor, bytes) - (displaysize - bytes);
        if (cursor < topleft)
            topleft = mcview_offset_rounddown (cursor, bytes);
        view->dpy_start = topleft;
        view->dpy_paragraph_skip_lines = 0;
        view->dpy_wrap_dirty = TRUE;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
mcview_movement_fixups (mcview_t * view, gboolean reset_search)
{
    mcview_scroll_to_cursor (view);
    if (reset_search)
    {
        view->search_start = view->hex_mode ? view->hex_cursor : view->dpy_start;
        view->search_end = view->search_start;
    }
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mcview_move_up (mcview_t * view, off_t lines)
{
    if (view->hex_mode)
    {
        off_t bytes = lines * view->bytes_per_line;
        if (view->hex_cursor >= bytes)
        {
            view->hex_cursor -= bytes;
            if (view->hex_cursor < view->dpy_start)
            {
                view->dpy_start = mcview_offset_doz (view->dpy_start, bytes);
                view->dpy_paragraph_skip_lines = 0;
                view->dpy_wrap_dirty = TRUE;
            }
        }
        else
        {
            view->hex_cursor %= view->bytes_per_line;
        }
    }
    else
    {
        mcview_ascii_move_up (view, lines);
    }
    mcview_movement_fixups (view, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_move_down (mcview_t * view, off_t lines)
{
    off_t last_byte;
    last_byte = mcview_get_filesize (view);
    if (view->hex_mode)
    {
        off_t i, limit;

        limit = mcview_offset_doz (last_byte, (off_t) view->bytes_per_line);

        for (i = 0; i < lines && view->hex_cursor < limit; i++)
        {
            view->hex_cursor += view->bytes_per_line;
            if (lines != 1)
            {
                view->dpy_start += view->bytes_per_line;
                view->dpy_paragraph_skip_lines = 0;
                view->dpy_wrap_dirty = TRUE;
            }
        }
    }
    else
    {
        mcview_ascii_move_down (view, lines);
    }
    mcview_movement_fixups (view, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_move_left (mcview_t * view, off_t columns)
{
    if (view->hex_mode)
    {
        off_t old_cursor = view->hex_cursor;
#ifdef HAVE_ASSERT_H
        assert (columns == 1);
#endif
        if (view->hexview_in_text || !view->hexedit_lownibble)
        {
            if (view->hex_cursor > 0)
                view->hex_cursor--;
        }
        if (!view->hexview_in_text)
            if (old_cursor > 0 || view->hexedit_lownibble)
                view->hexedit_lownibble = !view->hexedit_lownibble;
    }
    else if (!view->text_wrap_mode)
        view->dpy_text_column = mcview_offset_doz (view->dpy_text_column, columns);
    mcview_movement_fixups (view, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_move_right (mcview_t * view, off_t columns)
{
    if (view->hex_mode)
    {
        off_t last_byte;
        off_t old_cursor = view->hex_cursor;

        last_byte = mcview_offset_doz (mcview_get_filesize (view), 1);
#ifdef HAVE_ASSERT_H
        assert (columns == 1);
#endif
        if (view->hexview_in_text || view->hexedit_lownibble)
        {
            if (view->hex_cursor < last_byte)
                view->hex_cursor++;
        }
        if (!view->hexview_in_text)
            if (old_cursor < last_byte || !view->hexedit_lownibble)
                view->hexedit_lownibble = !view->hexedit_lownibble;
    }
    else if (!view->text_wrap_mode)
    {
        view->dpy_text_column += columns;
    }
    mcview_movement_fixups (view, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_moveto_top (mcview_t * view)
{
    view->dpy_start = 0;
    view->dpy_paragraph_skip_lines = 0;
    mcview_state_machine_init (&view->dpy_state_top, 0);
    view->hex_cursor = 0;
    view->dpy_text_column = 0;
    mcview_movement_fixups (view, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_moveto_bottom (mcview_t * view)
{
    off_t filesize;

    mcview_update_filesize (view);

    if (view->growbuf_in_use)
        mcview_growbuf_read_until (view, OFFSETTYPE_MAX);

    filesize = mcview_get_filesize (view);

    if (view->hex_mode)
    {
        view->hex_cursor = mcview_offset_doz (filesize, 1);
        mcview_movement_fixups (view, TRUE);
    }
    else
    {
        const off_t datalines = view->data_area.height;

        view->dpy_start = filesize;
        view->dpy_paragraph_skip_lines = 0;
        view->dpy_wrap_dirty = TRUE;
        mcview_move_up (view, datalines);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_moveto_bol (mcview_t * view)
{
    if (view->hex_mode)
    {
        view->hex_cursor -= view->hex_cursor % view->bytes_per_line;
        view->dpy_text_column = 0;
    }
    else
    {
        mcview_ascii_moveto_bol (view);
    }
    mcview_movement_fixups (view, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_moveto_eol (mcview_t * view)
{
    off_t bol;
    if (view->hex_mode)
    {
        off_t filesize;

        bol = mcview_offset_rounddown (view->hex_cursor, view->bytes_per_line);
        if (mcview_get_byte_indexed (view, bol, view->bytes_per_line - 1, NULL) == TRUE)
        {
            view->hex_cursor = bol + view->bytes_per_line - 1;
        }
        else
        {
            filesize = mcview_get_filesize (view);
            view->hex_cursor = mcview_offset_doz (filesize, 1);
        }
    }
    else
    {
        mcview_ascii_moveto_eol (view);
    }
    mcview_movement_fixups (view, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_moveto_offset (mcview_t * view, off_t offset)
{
    if (view->hex_mode)
    {
        view->hex_cursor = offset;
        view->dpy_start = offset - offset % view->bytes_per_line;
        view->dpy_paragraph_skip_lines = 0;
        view->dpy_wrap_dirty = TRUE;
    }
    else
    {
        view->dpy_start = offset;
        view->dpy_paragraph_skip_lines = 0;
        view->dpy_wrap_dirty = TRUE;
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
    *ret_column = (view->text_nroff_mode) ? coord.cc_nroff_column : coord.cc_column;
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
    widget_move (view, top + view->cursor_row, left + col);
}

/* --------------------------------------------------------------------------------------------- */
/** we have set view->search_start and view->search_end and must set
 * view->dpy_text_column and view->dpy_start
 * try to display maximum of match */

void
mcview_moveto_match (mcview_t * view)
{
    if (view->hex_mode)
    {
        view->hex_cursor = view->search_start;
        view->hexedit_lownibble = FALSE;
        view->dpy_start = view->search_start - view->search_start % view->bytes_per_line;
        view->dpy_end = view->search_end - view->search_end % view->bytes_per_line;
        view->dpy_paragraph_skip_lines = 0;
        view->dpy_wrap_dirty = TRUE;
    }
    else
    {
        view->dpy_start = mcview_bol (view, view->search_start, 0);
        view->dpy_paragraph_skip_lines = 0;
        view->dpy_wrap_dirty = TRUE;
    }

    mcview_scroll_to_cursor (view);
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */
