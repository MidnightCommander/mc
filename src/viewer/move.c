/*
   Internal file viewer for the Midnight Commander
   Functions for handle cursor movement

   Copyright (C) 1994, 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003,
   2004, 2005, 2006, 2007, 2009, 2011, 2013
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
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
mcview_movement_fixups (mcview_t * view, gboolean reset_search)
{
    mcview_scroll_to_cursor (view);
    if (reset_search)
    {
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

    if (view->hex_mode)
    {
        off_t bytes = lines * view->bytes_per_line;
        if (view->hex_cursor >= bytes)
        {
            view->hex_cursor -= bytes;
            if (view->hex_cursor < view->dpy_start)
                view->dpy_start = mcview_offset_doz (view->dpy_start, bytes);
        }
        else
        {
            view->hex_cursor %= view->bytes_per_line;
        }
    }
    else
    {
        off_t i;

        for (i = 0; i < lines; i++)
        {
            if (view->dpy_start == 0)
                break;
            if (view->text_wrap_mode)
            {
                new_offset = mcview_bol (view, view->dpy_start, view->dpy_start - (off_t) 1);
                /* check if dpy_start == BOL or not (then new_offset = dpy_start - 1,
                 * no need to check more) */
                if (new_offset == view->dpy_start)
                {
                    size_t last_row_length;

                    new_offset = mcview_bol (view, new_offset - 1, 0);
                    last_row_length = (view->dpy_start - new_offset) % view->data_area.width;
                    if (last_row_length != 0)
                    {
                        /* if dpy_start == BOL in wrapped mode, find BOL of previous line
                         * and move down all but the last rows */
                        new_offset = view->dpy_start - (off_t) last_row_length;
                    }
                }
                else
                {
                    /* if dpy_start != BOL in wrapped mode, just move one row up;
                     * no need to check if > 0 as there is at least exactly one wrap
                     * between dpy_start and BOL */
                    new_offset = view->dpy_start - (off_t) view->data_area.width;
                }
                view->dpy_start = new_offset;
            }
            else
            {
                /* if unwrapped -> current BOL equals dpy_start, just find BOL of previous line */
                new_offset = view->dpy_start - 1;
                view->dpy_start = mcview_bol (view, new_offset, 0);
            }
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
    if (view->hex_mode)
    {
        off_t i, limit;

        if (last_byte >= (off_t) view->bytes_per_line)
            limit = last_byte - view->bytes_per_line;
        else
            limit = 0;
        for (i = 0; i < lines && view->hex_cursor < limit; i++)
        {
            view->hex_cursor += view->bytes_per_line;
            if (lines != 1)
                view->dpy_start += view->bytes_per_line;
        }
    }
    else
    {
        off_t new_offset = 0;

        if (view->dpy_end - view->dpy_start > last_byte - view->dpy_end)
        {
            while (lines-- > 0)
            {
                if (view->text_wrap_mode)
                    view->dpy_end =
                        mcview_eol (view, view->dpy_end,
                                    view->dpy_end + (off_t) view->data_area.width);
                else
                    view->dpy_end = mcview_eol (view, view->dpy_end, last_byte);

                if (view->text_wrap_mode)
                    new_offset =
                        mcview_eol (view, view->dpy_start,
                                    view->dpy_start + (off_t) view->data_area.width);
                else
                    new_offset = mcview_eol (view, view->dpy_start, last_byte);
                if (new_offset < last_byte)
                    view->dpy_start = new_offset;
                if (view->dpy_end >= last_byte)
                    break;
            }
        }
        else
        {
            off_t i;
            for (i = 0; i < lines && new_offset < last_byte; i++)
            {
                if (view->text_wrap_mode)
                    new_offset =
                        mcview_eol (view, view->dpy_start,
                                    view->dpy_start + (off_t) view->data_area.width);
                else
                    new_offset = mcview_eol (view, view->dpy_start, last_byte);
                if (new_offset < last_byte)
                    view->dpy_start = new_offset;
            }
        }
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
    else
    {
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
    else
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
    }
    else if (!view->text_wrap_mode)
    {
        view->dpy_start = mcview_bol (view, view->dpy_start, 0);
    }
    view->dpy_text_column = 0;
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
        off_t eol;
        bol = mcview_bol (view, view->dpy_start, 0);
        eol = mcview_eol (view, view->dpy_start, mcview_get_filesize (view));
        if (!view->utf8)
        {
            if (eol > bol)
                view->dpy_text_column = eol - bol;
        }
        else
        {
            char *str = NULL;
            switch (view->datasource)
            {
            case DS_STDIO_PIPE:
            case DS_VFS_PIPE:
                str = mcview_get_ptr_growing_buffer (view, bol);
                break;
            case DS_FILE:
                str = mcview_get_ptr_file (view, bol);
                break;
            case DS_STRING:
                str = mcview_get_ptr_string (view, bol);
                break;
            case DS_NONE:
                break;
            }
            if (str != NULL && eol > bol)
                view->dpy_text_column = g_utf8_strlen (str, eol - bol);
            else
                view->dpy_text_column = eol - bol;
        }

        if (view->dpy_text_column < (off_t) view->data_area.width)
            view->dpy_text_column = 0;
        else
            view->dpy_text_column = view->dpy_text_column - (off_t) view->data_area.width;
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
    }
    else
    {
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
    off_t offset;

    offset = view->search_start;

    if (view->hex_mode)
    {
        view->hex_cursor = offset;
        view->dpy_start = offset - offset % view->bytes_per_line;
    }
    else
    {
        view->dpy_start = mcview_bol (view, offset, 0);
    }

    mcview_scroll_to_cursor (view);
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */
