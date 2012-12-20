/*
   Internal file viewer for the Midnight Commander
   Function for search data

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
#include "lib/widget.h"

#include "src/setup.h"

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static int search_cb_char_curr_index = -1;
static char search_cb_char_buffer[6];

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
mcview_find (mcview_t * view, off_t search_start, gsize * len)
{
    off_t search_end;

    view->search_numNeedSkipChar = 0;
    search_cb_char_curr_index = -1;

    if (mcview_search_options.backwards)
    {
        search_end = mcview_get_filesize (view);
        while (search_start >= 0)
        {
            view->search_nroff_seq->index = search_start;
            mcview_nroff_seq_info (view->search_nroff_seq);

            if (search_end > search_start + (off_t) view->search->original_len
                && mc_search_is_fixed_search_str (view->search))
                search_end = search_start + view->search->original_len;

            if (mc_search_run (view->search, (void *) view, search_start, search_end, len)
                && view->search->normal_offset == search_start)
            {
                if (view->text_nroff_mode)
                    view->search->normal_offset++;
                return TRUE;
            }

            search_start--;
        }
        view->search->error_str = g_strdup (_("Search string not found"));
        return FALSE;
    }
    view->search_nroff_seq->index = search_start;
    mcview_nroff_seq_info (view->search_nroff_seq);

    return mc_search_run (view->search, (void *) view, search_start, mcview_get_filesize (view),
                          len);
}

/* --------------------------------------------------------------------------------------------- */

static void
mcview_search_show_result (mcview_t * view, WDialog ** d, size_t match_len)
{
    int nroff_len;

    nroff_len =
        view->text_nroff_mode
        ? mcview__get_nroff_real_len (view, view->search->start_buffer,
                                      view->search->normal_offset - view->search->start_buffer) : 0;
    view->search_start = view->search->normal_offset + nroff_len;

    if (!view->hex_mode)
        view->search_start++;

    nroff_len =
        view->text_nroff_mode ? mcview__get_nroff_real_len (view, view->search_start - 1,
                                                            match_len) : 0;
    view->search_end = view->search_start + match_len + nroff_len;

    if (view->hex_mode)
    {
        view->hex_cursor = view->search_start;
        view->hexedit_lownibble = FALSE;
        view->dpy_start = view->search_start - view->search_start % view->bytes_per_line;
        view->dpy_end = view->search_end - view->search_end % view->bytes_per_line;
    }

    if (verbose)
    {
        dlg_run_done (*d);
        destroy_dlg (*d);
        *d = create_message (D_NORMAL, _("Search"), _("Seeking to search result"));
        tty_refresh ();
    }
    mcview_moveto_match (view);

}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

mc_search_cbret_t
mcview_search_cmd_callback (const void *user_data, gsize char_offset, int *current_char)
{
    mcview_t *view = (mcview_t *) user_data;

    /*    view_read_continue (view, &view->search_onechar_info); *//* AB:FIXME */
    if (!view->text_nroff_mode)
    {
        mcview_get_byte (view, char_offset, current_char);
        return MC_SEARCH_CB_OK;
    }

    if (view->search_numNeedSkipChar != 0)
    {
        view->search_numNeedSkipChar--;
        return MC_SEARCH_CB_SKIP;
    }

    if (search_cb_char_curr_index == -1
        || search_cb_char_curr_index >= view->search_nroff_seq->char_width)
    {
        if (search_cb_char_curr_index != -1)
            mcview_nroff_seq_next (view->search_nroff_seq);

        search_cb_char_curr_index = 0;
        if (view->search_nroff_seq->char_width > 1)
            g_unichar_to_utf8 (view->search_nroff_seq->current_char, search_cb_char_buffer);
        else
            search_cb_char_buffer[0] = (char) view->search_nroff_seq->current_char;

        if (view->search_nroff_seq->type != NROFF_TYPE_NONE)
        {
            switch (view->search_nroff_seq->type)
            {
            case NROFF_TYPE_BOLD:
                view->search_numNeedSkipChar = 1 + view->search_nroff_seq->char_width;  /* real char width and 0x8 */
                break;
            case NROFF_TYPE_UNDERLINE:
                view->search_numNeedSkipChar = 2;       /* underline symbol and ox8 */
                break;
            default:
                break;
            }
        }
        return MC_SEARCH_CB_INVALID;
    }

    *current_char = search_cb_char_buffer[search_cb_char_curr_index];
    search_cb_char_curr_index++;

    return (*current_char != -1) ? MC_SEARCH_CB_OK : MC_SEARCH_CB_INVALID;
}

/* --------------------------------------------------------------------------------------------- */

int
mcview_search_update_cmd_callback (const void *user_data, gsize char_offset)
{
    mcview_t *view = (mcview_t *) user_data;

    if ((off_t) char_offset >= view->update_activate)
    {
        view->update_activate += view->update_steps;
        if (verbose)
        {
            mcview_percent (view, char_offset);
            tty_refresh ();
        }
        if (tty_got_interrupt ())
            return MC_SEARCH_CB_ABORT;
    }
    /* may be in future return from this callback will change current position
     * in searching block. Now this just constant return value.
     */
    return MC_SEARCH_CB_OK;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_do_search (mcview_t * view)
{
    off_t search_start = 0;
    gboolean isFound = FALSE;
    gboolean need_search_again = TRUE;

    WDialog *d = NULL;

    size_t match_len;

    if (verbose)
    {
        d = create_message (D_NORMAL, _("Search"), _("Searching %s"), view->last_search_string);
        tty_refresh ();
    }

    /*for avoid infinite search loop we need to increase or decrease start offset of search */

    if (view->search_start != 0)
    {
        if (!view->text_nroff_mode)
            search_start = view->search_start + (mcview_search_options.backwards ? -2 : 0);
        else
        {
            if (mcview_search_options.backwards)
            {
                mcview_nroff_t *nroff;
                nroff = mcview_nroff_seq_new_num (view, view->search_start);
                if (mcview_nroff_seq_prev (nroff) != -1)
                    search_start =
                        -(mcview__get_nroff_real_len (view, nroff->index - 1, 2) +
                          nroff->char_width + 1);
                else
                    search_start = -2;

                mcview_nroff_seq_free (&nroff);
            }
            else
            {
                search_start = mcview__get_nroff_real_len (view, view->search_start + 1, 2);
            }
            search_start += view->search_start;
        }
    }

    if (mcview_search_options.backwards && (int) search_start < 0)
        search_start = 0;

    /* Compute the percent steps */
    mcview_search_update_steps (view);
    view->update_activate = 0;

    tty_enable_interrupt_key ();

    do
    {
        off_t growbufsize;

        if (view->growbuf_in_use)
            growbufsize = mcview_growbuf_filesize (view);
        else
            growbufsize = view->search->original_len;

        if (mcview_find (view, search_start, &match_len))
        {
            mcview_search_show_result (view, &d, match_len);
            need_search_again = FALSE;
            isFound = TRUE;
            break;
        }

        if (view->search->error_str == NULL)
            break;

        search_start = growbufsize - view->search->original_len;
        if (search_start <= 0)
        {
            search_start = 0;
            break;
        }
    }
    while (mcview_may_still_grow (view));

    if (view->search_start != 0 && !isFound && need_search_again
        && !mcview_search_options.backwards)
    {
        int result;

        mcview_update (view);

        result =
            query_dialog (_("Search done"), _("Continue from beginning?"), D_NORMAL, 2, _("&Yes"),
                          _("&No"));

        if (result != 0)
            isFound = TRUE;
        else
            search_start = 0;
    }

    if (!isFound && view->search->error_str != NULL && mcview_find (view, search_start, &match_len))
    {
        mcview_search_show_result (view, &d, match_len);
        isFound = TRUE;
    }

    tty_disable_interrupt_key ();

    if (verbose)
    {
        dlg_run_done (d);
        destroy_dlg (d);
    }

    if (!isFound && view->search->error_str != NULL)
        message (D_NORMAL, _("Search"), "%s", view->search->error_str);

    view->dirty++;
    mcview_update (view);
}

/* --------------------------------------------------------------------------------------------- */
