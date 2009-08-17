/*
   Internal file viewer for the Midnight Commander
   Function for nroff-like view

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
#include "../src/main.h"
#include "../src/charsets.h"
#include "../src/tty/tty.h"
#include "../src/tty/color.h"
#include "../src/viewer/internal.h"


/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

/*** public functions ****************************************************************************/

/* --------------------------------------------------------------------------------------------- */

void
mcview_display_nroff (mcview_t * view)
{
    const screen_dimen left = view->data_area.left;
    const screen_dimen top = view->data_area.top;
    const screen_dimen width = view->data_area.width;
    const screen_dimen height = view->data_area.height;
    screen_dimen row, col;
    off_t from;
    int cw = 1;
    int c;
    gboolean read_res = TRUE;
    struct hexedit_change_node *curr = view->change_list;

    mcview_display_clean (view);
    mcview_display_ruler (view);

    /* Find the first displayable changed byte */
    from = view->dpy_start;
    while (curr && (curr->offset < from)) {
        curr = curr->next;
    }

    tty_setcolor (NORMAL_COLOR);
    for (row = 0, col = 0; row < height;) {
#ifdef HAVE_CHARSET
        if (view->utf8) {
            c = mcview_get_utf (view, from, &cw, &read_res);
            if (!read_res)
                break;
        } else
#endif
        {
            if ((c = mcview_get_byte (view, from)) == -1)
                break;
        }
        from++;
        if (cw > 1)
            from += cw - 1;

        if (c == '\b') {
            int c_prev;
            int c_next;
            if (from > 1) {
                c_prev = mcview_get_byte (view, from - 2);
                c_next = mcview_get_byte (view, from);
            }
            if (g_ascii_isprint (c_prev) && g_ascii_isprint (c_prev)
                && (c_prev == c_next || c_prev == '_' || (c_prev == '+' && c_next == 'o'))) {
                if (col == 0) {
                    if (row == 0) {
                        /* We're inside an nroff character sequence at the
                         * beginning of the screen -- just skip the
                         * backspace and continue with the next character. */
                        continue;
                    }
                    row--;
                    col = width;
                }
                col--;
                if (c_prev == '_' && (c_next != '_' || mcview_count_backspaces (view, from + 1) == 1))
                    tty_setcolor (VIEW_UNDERLINED_COLOR);
                else
                    tty_setcolor (MARKED_COLOR);
                continue;
            }
        }

        if ((c == '\n') || (col >= width && view->text_wrap_mode)) {
            col = 0;
            row++;
            if (c == '\n' || row >= height)
                continue;
        }

        if (c == '\r') {
            c = mcview_get_byte_indexed (view, from, 1);
            if (c == '\r' || c == '\n')
                continue;
            col = 0;
            row++;
            continue;
        }

        if (c == '\t') {
            off_t line, column;
            mcview_offset_to_coord (view, &line, &column, from);
            col += (8 - column % 8);
            if (view->text_wrap_mode && col >= width && width != 0) {
                row += col / width;
                col %= width;
            }
            continue;
        }

        if (view->search_start <= from && from < view->search_end) {
            tty_setcolor (SELECTED_COLOR);
        }

        if (col >= view->dpy_text_column && col - view->dpy_text_column < width) {
            widget_move (view, top + row, left + (col - view->dpy_text_column));
#ifdef HAVE_CHARSET
            if (utf8_display) {
                if (!view->utf8) {
                    c = convert_from_8bit_to_utf_c ((unsigned char) c, view->converter);
                }
                if (!g_unichar_isprint (c))
                    c = '.';
            } else {
                if (view->utf8) {
                    c = convert_from_utf_to_current_c (c, view->converter);
                } else {
#endif
                    c = convert_to_display_c (c);
#ifdef HAVE_CHARSET
                }
            }
#endif
            tty_print_anychar (c);
        }
        col++;
        tty_setcolor (NORMAL_COLOR);
    }
    view->dpy_end = from;
}

int
mcview__get_nroff_real_len (mcview_t * view, off_t start, off_t length)
{
    return 0;                   /* AB:FIXME */
}

/* --------------------------------------------------------------------------------------------- */
