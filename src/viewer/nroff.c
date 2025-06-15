/*
   Internal file viewer for the Midnight Commander
   Functions for searching in nroff-like view

   Copyright (C) 1994-2026
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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <config.h>

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/skin.h"

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
mcview_nroff_get_char (mcview_nroff_t *nroff, const off_t nroff_index)
{
    charset_conv_t *conv = &nroff->view->conv;

    convert_char_to_display (conv, nroff->view, nroff_index);

    if (conv->utf8 && conv->ch == -1)
    {
        // we need to get a symbol in any case
        conv->utf8 = FALSE;
        convert_char_to_display (conv, nroff->view, nroff_index);
        conv->utf8 = TRUE;
    }

    nroff->char_length = conv->len;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

int
mcview__get_nroff_real_len (WView *view, off_t start, off_t length)
{
    mcview_nroff_t *nroff;
    int ret = 0;
    off_t i = 0;

    if (!view->mode_flags.nroff)
        return 0;

    nroff = mcview_nroff_seq_new_num (view, start);
    if (nroff == NULL)
        return 0;
    while (i < length)
    {
        switch (nroff->type)
        {
        case NROFF_TYPE_BOLD:
            ret += nroff->char_length + 1;  // letter + '\b'
            break;
        case NROFF_TYPE_UNDERLINE:
            ret += 2;  // '_' + '\b'
            break;
        case NROFF_TYPE_BOLD_UNDERLINE:
            ret += 2 + nroff->char_length + 1;  // '_' + '\b' + letter + '\b'
            break;
        default:
            break;
        }
        i += nroff->char_length;
        mcview_nroff_seq_next (nroff);
    }

    mcview_nroff_seq_free (&nroff);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

mcview_nroff_t *
mcview_nroff_seq_new_num (WView *view, off_t lc_index)
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
mcview_nroff_seq_new (WView *view)
{
    return mcview_nroff_seq_new_num (view, (off_t) 0);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_nroff_seq_free (mcview_nroff_t **nroff)
{
    if (nroff == NULL || *nroff == NULL)
        return;
    MC_PTR_FREE (*nroff);
}

/* --------------------------------------------------------------------------------------------- */

nroff_type_t
mcview_nroff_seq_info (mcview_nroff_t *nroff)
{
    gboolean bold_and_underline;
    charset_conv_t *conv = &nroff->view->conv;

    if (nroff == NULL)
        return NROFF_TYPE_NONE;

    nroff->type = NROFF_TYPE_NONE;

    mcview_nroff_get_char (nroff, nroff->index);

    if (conv->ch == -1 || !conv->printable)
        return nroff->type;

    nroff->current_char = conv->ch;

    const int next = mcview_get_byte (nroff->view, nroff->index + nroff->char_length);

    if (next == -1 || next != '\b')
        return nroff->type;

    mcview_nroff_get_char (nroff, nroff->index + 1 + nroff->char_length);

    if (conv->ch == -1 || !conv->printable)
        return nroff->type;

    const int next2 = conv->ch;

    bold_and_underline = nroff->current_char == '_';
    if (bold_and_underline)
    {
        const int next3 = mcview_get_byte (nroff->view, nroff->index + 2 + nroff->char_length);

        bold_and_underline = next3 != -1 && next3 == '\b';
    }
    if (bold_and_underline)
    {
        mcview_nroff_get_char (nroff, nroff->index + 2 + nroff->char_length + 1);
        bold_and_underline = conv->ch != -1 && conv->printable && next2 == conv->ch;
    }
    if (bold_and_underline)
    {
        nroff->current_char = next2;
        nroff->type = NROFF_TYPE_BOLD_UNDERLINE;
    }
    else if (nroff->current_char == '_' && next2 == '_')
        nroff->type =
            (nroff->prev_type == NROFF_TYPE_BOLD) ? NROFF_TYPE_BOLD : NROFF_TYPE_UNDERLINE;
    else if (nroff->current_char == next2)
        nroff->type = NROFF_TYPE_BOLD;
    else if (nroff->current_char == '_')
    {
        nroff->current_char = next2;
        nroff->type = NROFF_TYPE_UNDERLINE;
    }
    return nroff->type;
}

/* --------------------------------------------------------------------------------------------- */

int
mcview_nroff_seq_next (mcview_nroff_t *nroff)
{
    if (nroff == NULL)
        return -1;

    nroff->prev_type = nroff->type;

    switch (nroff->type)
    {
    case NROFF_TYPE_BOLD:
        nroff->index += nroff->char_length + 1;  // letter + '\b'
        break;
    case NROFF_TYPE_UNDERLINE:
        nroff->index += 2;  // '_' + '\b'
        break;
    case NROFF_TYPE_BOLD_UNDERLINE:
        nroff->index += 2 + nroff->char_length + 1;  // '_' + '\b' + letter + '\b'
        break;
    default:
        break;
    }

    nroff->index += nroff->char_length;

    mcview_nroff_seq_info (nroff);
    return nroff->current_char;
}

/* --------------------------------------------------------------------------------------------- */

int
mcview_nroff_seq_prev (mcview_nroff_t *nroff)
{
    int prev;
    off_t prev_index, prev_index2;

    if (nroff == NULL)
        return -1;

    nroff->prev_type = NROFF_TYPE_NONE;

    if (nroff->index == 0)
        return -1;

    for (prev_index = nroff->index - 1; prev_index != 0; prev_index--)
    {
        charset_conv_t *conv = &nroff->view->conv;

        mcview_nroff_get_char (nroff, prev_index);
        nroff->current_char = conv->ch;

        if (conv->ch != -1 && conv->printable)
            break;
    }

    if (prev_index == 0)
    {
        nroff->index--;
        mcview_nroff_seq_info (nroff);
        return nroff->current_char;
    }

    prev_index--;

    prev = mcview_get_byte (nroff->view, prev_index);
    if (prev == -1 || prev != '\b')
    {
        nroff->index = prev_index;
        mcview_nroff_seq_info (nroff);
        return nroff->current_char;
    }

    for (prev_index2 = prev_index - 1; prev_index2 != 0; prev_index2--)
    {
        charset_conv_t *conv = &nroff->view->conv;

        mcview_nroff_get_char (nroff, prev_index);
        prev = conv->ch;

        if (conv->ch != -1 && conv->printable)
            break;
    }

    nroff->index = prev_index2 == 0 ? prev_index : prev_index2;
    mcview_nroff_seq_info (nroff);
    return nroff->current_char;
}

/* --------------------------------------------------------------------------------------------- */
