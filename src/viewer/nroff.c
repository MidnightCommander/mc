/*
   Internal file viewer for the Midnight Commander
   Function for nroff-like view

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
   Ilia Maslakov <il.smind@gmail.com>, 2009

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

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/skin.h"
#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#endif

#include "src/setup.h"          /* option_tab_spacing */

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static gboolean
mcview_nroff_get_char (mcview_nroff_t * nroff, int *ret_val, off_t nroff_index)
{
    int c;
#ifdef HAVE_CHARSET
    if (nroff->view->utf8)
    {
        gboolean utf_result;
        c = mcview_get_utf (nroff->view, nroff_index, &nroff->char_width, &utf_result);
        if (!utf_result)
        {
            /* we need got symbol in any case */
            nroff->char_width = 1;
            if (!mcview_get_byte (nroff->view, nroff_index, &c) || !g_ascii_isprint (c))
                return FALSE;
        }
    }
    else
#endif
    {
        nroff->char_width = 1;
        if (!mcview_get_byte (nroff->view, nroff_index, &c))
            return FALSE;
    }

    *ret_val = c;

    return g_unichar_isprint (c);
}

/* --------------------------------------------------------------------------------------------- */
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
    int c_prev = 0;
    int c_next = 0;
    struct hexedit_change_node *curr = view->change_list;

    mcview_display_clean (view);
    mcview_display_ruler (view);

    /* Find the first displayable changed byte */
    from = view->dpy_start;
    while (curr && (curr->offset < from))
    {
        curr = curr->next;
    }

    tty_setcolor (NORMAL_COLOR);
    for (row = 0, col = 0; row < height;)
    {
#ifdef HAVE_CHARSET
        if (view->utf8)
        {
            gboolean read_res = TRUE;
            c = mcview_get_utf (view, from, &cw, &read_res);
            if (!read_res)
                break;
        }
        else
#endif
        {
            if (!mcview_get_byte (view, from, &c))
                break;
        }
        from++;
        if (cw > 1)
            from += cw - 1;

        if (c == '\b')
        {
            if (from > 1)
            {
#ifdef HAVE_CHARSET
                if (view->utf8)
                {
                    gboolean read_res;
                    c_next = mcview_get_utf (view, from, &cw, &read_res);
                }
                else
#endif
                    mcview_get_byte (view, from, &c_next);
            }
            if (g_unichar_isprint (c_prev) && g_unichar_isprint (c_next)
                && (c_prev == c_next || c_prev == '_' || (c_prev == '+' && c_next == 'o')))
            {
                if (col == 0)
                {
                    if (row == 0)
                    {
                        /* We're inside an nroff character sequence at the
                         * beginning of the screen -- just skip the
                         * backspace and continue with the next character. */
                        continue;
                    }
                    row--;
                    col = width;
                }
                col--;
                if (c_prev == '_'
                    && (c_next != '_' || mcview_count_backspaces (view, from + 1) == 1))
                    tty_setcolor (VIEW_UNDERLINED_COLOR);
                else
                    tty_setcolor (VIEW_BOLD_COLOR);
                continue;
            }
        }

        if ((c == '\n') || (col >= width && view->text_wrap_mode))
        {
            col = 0;
            row++;
            if (c == '\n' || row >= height)
                continue;
        }

        if (c == '\r')
        {
            mcview_get_byte_indexed (view, from, 1, &c);
            if (c == '\r' || c == '\n')
                continue;
            col = 0;
            row++;
            continue;
        }

        if (c == '\t')
        {
            off_t line, column;
            mcview_offset_to_coord (view, &line, &column, from);
            col += (option_tab_spacing - col % option_tab_spacing);
            if (view->text_wrap_mode && col >= width && width != 0)
            {
                row += col / width;
                col %= width;
            }
            continue;
        }

        if (view->search_start <= from && from < view->search_end)
        {
            tty_setcolor (SELECTED_COLOR);
        }

        c_prev = c;

        if ((off_t) col >= view->dpy_text_column
            && (off_t) col - view->dpy_text_column < (off_t) width)
        {
            widget_move (view, top + row, left + ((off_t) col - view->dpy_text_column));
#ifdef HAVE_CHARSET
            if (mc_global.utf8_display)
            {
                if (!view->utf8)
                {
                    c = convert_from_8bit_to_utf_c ((unsigned char) c, view->converter);
                }
                if (!g_unichar_isprint (c))
                    c = '.';
            }
            else if (view->utf8)
                c = convert_from_utf_to_current_c (c, view->converter);
            else
                c = convert_to_display_c (c);
#endif
            tty_print_anychar (c);
        }
        col++;
#ifdef HAVE_CHARSET
        if (view->utf8)
        {
            if (g_unichar_iswide (c))
                col++;
            else if (g_unichar_iszerowidth (c))
                col--;
        }
#endif
        tty_setcolor (NORMAL_COLOR);
    }
    view->dpy_end = from;
}

/* --------------------------------------------------------------------------------------------- */

int
mcview__get_nroff_real_len (mcview_t * view, off_t start, off_t length)
{
    mcview_nroff_t *nroff;
    int ret = 0;
    off_t i = 0;

    if (!view->text_nroff_mode)
        return 0;

    nroff = mcview_nroff_seq_new_num (view, start);
    if (nroff == NULL)
        return 0;
    while (i < length)
    {
        switch (nroff->type)
        {
        case NROFF_TYPE_BOLD:
            ret += 1 + nroff->char_width;       /* real char width and 0x8 */
            break;
        case NROFF_TYPE_UNDERLINE:
            ret += 2;           /* underline symbol and ox8 */
            break;
        default:
            break;
        }
        i += nroff->char_width;
        mcview_nroff_seq_next (nroff);
    }

    mcview_nroff_seq_free (&nroff);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

mcview_nroff_t *
mcview_nroff_seq_new_num (mcview_t * view, off_t lc_index)
{
    mcview_nroff_t *nroff;

    nroff = g_try_malloc0 (sizeof (mcview_nroff_t));
    if (nroff != NULL)
    {
        nroff->index = lc_index;
        nroff->view = view;
        mcview_nroff_seq_info (nroff);
    }
    return nroff;
}

/* --------------------------------------------------------------------------------------------- */

mcview_nroff_t *
mcview_nroff_seq_new (mcview_t * view)
{
    return mcview_nroff_seq_new_num (view, (off_t) 0);

}

/* --------------------------------------------------------------------------------------------- */

void
mcview_nroff_seq_free (mcview_nroff_t ** nroff)
{
    if (nroff == NULL || *nroff == NULL)
        return;
    g_free (*nroff);
    *nroff = NULL;
}

/* --------------------------------------------------------------------------------------------- */

nroff_type_t
mcview_nroff_seq_info (mcview_nroff_t * nroff)
{
    int next, next2;

    if (nroff == NULL)
        return NROFF_TYPE_NONE;
    nroff->type = NROFF_TYPE_NONE;

    if (!mcview_nroff_get_char (nroff, &nroff->current_char, nroff->index))
        return nroff->type;

    if (!mcview_get_byte (nroff->view, nroff->index + nroff->char_width, &next) || next != '\b')
        return nroff->type;

    if (!mcview_nroff_get_char (nroff, &next2, nroff->index + 1 + nroff->char_width))
        return nroff->type;

    if (nroff->current_char == '_' && next2 == '_')
    {
        nroff->type = (nroff->prev_type == NROFF_TYPE_BOLD)
            ? NROFF_TYPE_BOLD : NROFF_TYPE_UNDERLINE;

    }
    else if (nroff->current_char == next2)
    {
        nroff->type = NROFF_TYPE_BOLD;
    }
    else if (nroff->current_char == '_')
    {
        nroff->current_char = next2;
        nroff->type = NROFF_TYPE_UNDERLINE;
    }
    else if (nroff->current_char == '+' && next2 == 'o')
    {
        /* ??? */
    }
    return nroff->type;
}

/* --------------------------------------------------------------------------------------------- */

int
mcview_nroff_seq_next (mcview_nroff_t * nroff)
{
    if (nroff == NULL)
        return -1;

    nroff->prev_type = nroff->type;

    switch (nroff->type)
    {
    case NROFF_TYPE_BOLD:
        nroff->index += 1 + nroff->char_width;
        break;
    case NROFF_TYPE_UNDERLINE:
        nroff->index += 2;
        break;
    default:
        break;
    }

    nroff->index += nroff->char_width;

    mcview_nroff_seq_info (nroff);
    return nroff->current_char;
}

/* --------------------------------------------------------------------------------------------- */

int
mcview_nroff_seq_prev (mcview_nroff_t * nroff)
{
    int prev;
    off_t prev_index, prev_index2;

    if (nroff == NULL)
        return -1;

    nroff->prev_type = NROFF_TYPE_NONE;

    if (nroff->index == 0)
        return -1;

    prev_index = nroff->index - 1;

    while (prev_index != 0)
    {
        if (mcview_nroff_get_char (nroff, &nroff->current_char, prev_index))
            break;
        prev_index--;
    }
    if (prev_index == 0)
    {
        nroff->index--;
        mcview_nroff_seq_info (nroff);
        return nroff->current_char;
    }

    prev_index--;

    if (!mcview_get_byte (nroff->view, prev_index, &prev) || prev != '\b')
    {
        nroff->index = prev_index;
        mcview_nroff_seq_info (nroff);
        return nroff->current_char;
    }
    prev_index2 = prev_index - 1;

    while (prev_index2 != 0)
    {
        if (mcview_nroff_get_char (nroff, &prev, prev_index))
            break;
        prev_index2--;
    }

    nroff->index = (prev_index2 == 0) ? prev_index : prev_index2;
    mcview_nroff_seq_info (nroff);
    return nroff->current_char;
}

/* --------------------------------------------------------------------------------------------- */
