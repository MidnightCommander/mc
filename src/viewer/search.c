/*
   Internal file viewer for the Midnight Commander
   Function for search data

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
#include "../src/setup.h"
#include "../src/wtools.h"
#include "../src/tty/tty.h"
#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/


/* --------------------------------------------------------------------------------------------- */
static void
mcview_search_update_steps (mcview_t * view)
{
    off_t filesize = mcview_get_filesize (view);
    if (filesize != 0)
        view->update_steps = 40000;
    else                        /* viewing a data stream, not a file */
        view->update_steps = filesize / 100;

    /* Do not update the percent display but every 20 ks */
    if (view->update_steps < 20000)
        view->update_steps = 20000;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcview_find (mcview_t * view, gsize search_start, gsize * len)
{
    gsize search_end;

    view->search_numNeedSkipChar = 0;

    if (view->search_backwards) {
        search_end = mcview_get_filesize (view);
        while ((int) search_start >= 0) {
            view->search_nroff_seq->index = search_start;
            mcview_nroff_seq_info (view->search_nroff_seq);

            if (search_end > search_start + view->search->original_len
                && mc_search_is_fixed_search_str (view->search))
                search_end = search_start + view->search->original_len;

            if (mc_search_run (view->search, (void *) view, search_start, search_end, len)
                && view->search->normal_offset == search_start)
                return TRUE;

            search_start--;
        }
        view->search->error_str = g_strdup (_(" Search string not found "));
        return FALSE;
    }
    view->search_nroff_seq->index = search_start;
    mcview_nroff_seq_info (view->search_nroff_seq);

    return mc_search_run (view->search, (void *) view, search_start, mcview_get_filesize (view),
                          len);
}

/* --------------------------------------------------------------------------------------------- */

/*** public functions ****************************************************************************/

/* --------------------------------------------------------------------------------------------- */

int
mcview_search_cmd_callback (const void *user_data, gsize char_offset)
{
    int byte;
    mcview_t *view = (mcview_t *) user_data;

    /*    view_read_continue (view, &view->search_onechar_info); *//* AB:FIXME */
    if (!view->text_nroff_mode) {
        if (! mcview_get_byte (view, char_offset, &byte))
            return MC_SEARCH_CB_ABORT;

        return byte;
    }

    if (view->search_numNeedSkipChar) {
        view->search_numNeedSkipChar--;
        return MC_SEARCH_CB_SKIP;
    }

    byte = view->search_nroff_seq->current_char;

    if (byte == -1)
        return MC_SEARCH_CB_ABORT;

    mcview_nroff_seq_next (view->search_nroff_seq);

    if (view->search_nroff_seq->type != NROFF_TYPE_NONE)
        view->search_numNeedSkipChar = 2;

    return byte;
}

/* --------------------------------------------------------------------------------------------- */

int
mcview_search_update_cmd_callback (const void *user_data, gsize char_offset)
{
    mcview_t *view = (mcview_t *) user_data;

    if (char_offset >= view->update_activate) {
        view->update_activate += view->update_steps;
        if (verbose) {
            mcview_percent (view, char_offset);
            tty_refresh ();
        }
        if (tty_got_interrupt ())
            return MC_SEARCH_CB_ABORT;
    }
    /* may be in future return from this callback will change current position
     * in searching block. Now this just constant return value.
     */
    return 1;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_do_search (mcview_t * view)
{
    off_t search_start;
    gboolean isFound = FALSE;

    Dlg_head *d = NULL;

    size_t match_len;

    if (verbose) {
        d = create_message (D_NORMAL, _("Search"), _("Searching %s"), view->last_search_string);
        tty_refresh ();
    }

    /*for avoid infinite search loop we need to increase or decrease start offset of search */

    if (view->search_start) {
        search_start = (view->search_backwards) ? -2 : 2;
        search_start = view->search_start + search_start +
            mcview__get_nroff_real_len (view, view->search_start, 2) * search_start;
    } else {
        search_start = view->search_start;
    }

    if (view->search_backwards && (int) search_start < 0)
        search_start = 0;

    /* Compute the percent steps */
    mcview_search_update_steps (view);
    view->update_activate = 0;

    tty_enable_interrupt_key ();

    do {
        if (mcview_find (view, search_start, &match_len)) {
            view->search_start = view->search->normal_offset +
                mcview__get_nroff_real_len (view,
                                            view->search->start_buffer,
                                            view->search->normal_offset -
                                            view->search->start_buffer);

            if (!view->hex_mode)
                view->search_start++;

            view->search_end = view->search_start + match_len +
                mcview__get_nroff_real_len (view, view->search_start - 1, match_len);

            if (view->hex_mode) {
                view->hex_cursor = view->search_start;
                view->hexedit_lownibble = FALSE;
                view->dpy_start = view->search_start - view->search_start % view->bytes_per_line;
                view->dpy_end = view->search_end - view->search_end % view->bytes_per_line;
            }

            if (verbose) {
                dlg_run_done (d);
                destroy_dlg (d);
                d = create_message (D_NORMAL, _("Search"), _("Seeking to search result"));
                tty_refresh ();
            }

            mcview_moveto_match (view);
            isFound = TRUE;
            break;
        }
    } while (mcview_may_still_grow (view));

    if (!isFound) {
        if (view->search->error_str)
            message (D_NORMAL, _("Search"), "%s", view->search->error_str);
    }

    view->dirty++;
    mcview_update (view);


    tty_disable_interrupt_key ();
    if (verbose) {
        dlg_run_done (d);
        destroy_dlg (d);
    }

}

/* --------------------------------------------------------------------------------------------- */
