/*
   Internal file viewer for the Midnight Commander
   Functions for searching in nroff-like view

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

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static gboolean
mcview_nroff_get_char (mcview_nroff_t * nroff, int *ret_val, off_t nroff_index)
{
    int c = 0;

#ifdef HAVE_CHARSET
    if (nroff->view->utf8)
    {
        if (!mcview_get_utf (nroff->view, nroff_index, &c, &nroff->char_length))
        {
            /* we need got symbol in any case */
            nroff->char_length = 1;
            if (!mcview_get_byte (nroff->view, nroff_index, &c) || !g_ascii_isprint (c))
                return FALSE;
        }
    }
    else
#endif
    {
        nroff->char_length = 1;
        if (!mcview_get_byte (nroff->view, nroff_index, &c))
            return FALSE;
    }

    *ret_val = c;

    return g_unichar_isprint (c);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

int
mcview__get_nroff_real_len (WView * view, off_t start, off_t length)
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
            ret += 1 + nroff->char_length;      /* real char length and 0x8 */
            break;
        case NROFF_TYPE_UNDERLINE:
            ret += 2;           /* underline symbol and ox8 */
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
mcview_nroff_seq_new_num (WView * view, off_t lc_index)
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
mcview_nroff_seq_new (WView * view)
{
    return mcview_nroff_seq_new_num (view, (off_t) 0);

}

/* --------------------------------------------------------------------------------------------- */

void
mcview_nroff_seq_free (mcview_nroff_t ** nroff)
{
    if (nroff == NULL || *nroff == NULL)
        return;
    MC_PTR_FREE (*nroff);
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

    if (!mcview_get_byte (nroff->view, nroff->index + nroff->char_length, &next) || next != '\b')
        return nroff->type;

    if (!mcview_nroff_get_char (nroff, &next2, nroff->index + 1 + nroff->char_length))
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
        nroff->index += 1 + nroff->char_length;
        break;
    case NROFF_TYPE_UNDERLINE:
        nroff->index += 2;
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
