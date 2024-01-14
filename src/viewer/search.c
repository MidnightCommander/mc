/*
   Internal file viewer for the Midnight Commander
   Function for search data

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
   Andrew Borodin <aborodin@vmail.ru>, 2009-2022
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
#include "lib/strutil.h"
#ifdef HAVE_CHARSET
#include "lib/charsets.h"       /* cp_source */
#endif
#include "lib/widget.h"

#include "src/setup.h"

#include "internal.h"

/*** global variables ****************************************************************************/

mcview_search_options_t mcview_search_options = {
    .type = MC_SEARCH_T_NORMAL,
    .case_sens = FALSE,
    .backwards = FALSE,
    .whole_words = FALSE,
    .all_codepages = FALSE
};

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

typedef struct
{
    simple_status_msg_t status_msg;     /* base class */

    gboolean first;
    WView *view;
    off_t offset;
} mcview_search_status_msg_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static int search_cb_char_curr_index = -1;
static char search_cb_char_buffer[6];

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static int
mcview_search_status_update_cb (status_msg_t * sm)
{
    simple_status_msg_t *ssm = SIMPLE_STATUS_MSG (sm);
    mcview_search_status_msg_t *vsm = (mcview_search_status_msg_t *) sm;
    Widget *wd = WIDGET (sm->dlg);
    int percent = -1;

    if (verbose)
        percent = mcview_calc_percent (vsm->view, vsm->offset);

    if (percent >= 0)
        label_set_textv (ssm->label, _("Searching %s: %3d%%"), vsm->view->last_search_string,
                         percent);
    else
        label_set_textv (ssm->label, _("Searching %s"), vsm->view->last_search_string);

    if (vsm->first)
    {
        Widget *lw = WIDGET (ssm->label);
        WRect r;

        r = wd->rect;
        r.cols = MAX (r.cols, lw->rect.cols + 6);
        widget_set_size_rect (wd, &r);
        r = lw->rect;
        r.x = wd->rect.x + (wd->rect.cols - r.cols) / 2;
        widget_set_size_rect (lw, &r);
        vsm->first = FALSE;
    }

    return status_msg_common_update (sm);
}

/* --------------------------------------------------------------------------------------------- */

static void
mcview_search_update_steps (WView * view)
{
    off_t filesize;

    filesize = mcview_get_filesize (view);

    if (filesize != 0)
        view->update_steps = filesize / 100;
    else                        /* viewing a data stream, not a file */
        view->update_steps = 40000;

    /* Do not update the percent display but every 20 kb */
    if (view->update_steps < 20000)
        view->update_steps = 20000;

    /* Make interrupt more responsive */
    if (view->update_steps > 40000)
        view->update_steps = 40000;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcview_find (mcview_search_status_msg_t * ssm, off_t search_start, off_t search_end, gsize * len)
{
    WView *view = ssm->view;

    view->search_numNeedSkipChar = 0;
    search_cb_char_curr_index = -1;

    if (mcview_search_options.backwards)
    {
        search_end = mcview_get_filesize (view);
        while (search_start >= 0)
        {
            gboolean ok;

            view->search_nroff_seq->index = search_start;
            mcview_nroff_seq_info (view->search_nroff_seq);

            if (search_end > search_start + (off_t) view->search->original.str->len
                && mc_search_is_fixed_search_str (view->search))
                search_end = search_start + view->search->original.str->len;

            ok = mc_search_run (view->search, (void *) ssm, search_start, search_end, len);
            if (ok && view->search->normal_offset == search_start)
            {
                if (view->mode_flags.nroff)
                    view->search->normal_offset++;
                return TRUE;
            }

            /* We abort the search in case of a pattern error, or if the user aborts
               the search. In other words: in all cases except "string not found". */
            if (!ok && view->search->error != MC_SEARCH_E_NOTFOUND)
                return FALSE;

            search_start--;
        }

        mc_search_set_error (view->search, MC_SEARCH_E_NOTFOUND, "%s", _(STR_E_NOTFOUND));
        return FALSE;
    }
    view->search_nroff_seq->index = search_start;
    mcview_nroff_seq_info (view->search_nroff_seq);

    return mc_search_run (view->search, (void *) ssm, search_start, search_end, len);
}

/* --------------------------------------------------------------------------------------------- */

static void
mcview_search_show_result (WView * view, size_t match_len)
{
    int nroff_len;

    nroff_len =
        view->mode_flags.nroff
        ? mcview__get_nroff_real_len (view, view->search->start_buffer,
                                      view->search->normal_offset - view->search->start_buffer) : 0;
    view->search_start = view->search->normal_offset + nroff_len;

    if (!view->mode_flags.hex)
        view->search_start++;

    nroff_len =
        view->mode_flags.nroff ? mcview__get_nroff_real_len (view, view->search_start - 1,
                                                             match_len) : 0;
    view->search_end = view->search_start + match_len + nroff_len;

    mcview_moveto_match (view);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_search_init (WView * view)
{
#ifdef HAVE_CHARSET
    view->search = mc_search_new (view->last_search_string, cp_source);
#else
    view->search = mc_search_new (view->last_search_string, NULL);
#endif

    view->search_nroff_seq = mcview_nroff_seq_new (view);

    if (view->search == NULL)
        return FALSE;

    view->search->search_type = mcview_search_options.type;
#ifdef HAVE_CHARSET
    view->search->is_all_charsets = mcview_search_options.all_codepages;
#endif
    view->search->is_case_sensitive = mcview_search_options.case_sens;
    view->search->whole_words = mcview_search_options.whole_words;
    view->search->search_fn = mcview_search_cmd_callback;
    view->search->update_fn = mcview_search_update_cmd_callback;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_search_deinit (WView * view)
{
    mc_search_free (view->search);
    g_free (view->last_search_string);
    mcview_nroff_seq_free (&view->search_nroff_seq);
}

/* --------------------------------------------------------------------------------------------- */

mc_search_cbret_t
mcview_search_cmd_callback (const void *user_data, gsize char_offset, int *current_char)
{
    WView *view = ((const mcview_search_status_msg_t *) user_data)->view;

    /*    view_read_continue (view, &view->search_onechar_info); *//* AB:FIXME */
    if (!view->mode_flags.nroff)
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
        || search_cb_char_curr_index >= view->search_nroff_seq->char_length)
    {
        if (search_cb_char_curr_index != -1)
            mcview_nroff_seq_next (view->search_nroff_seq);

        search_cb_char_curr_index = 0;
        if (view->search_nroff_seq->char_length > 1)
            g_unichar_to_utf8 (view->search_nroff_seq->current_char, search_cb_char_buffer);
        else
            search_cb_char_buffer[0] = (char) view->search_nroff_seq->current_char;

        if (view->search_nroff_seq->type != NROFF_TYPE_NONE)
        {
            switch (view->search_nroff_seq->type)
            {
            case NROFF_TYPE_BOLD:
                view->search_numNeedSkipChar = 1 + view->search_nroff_seq->char_length; /* real char length and 0x8 */
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

mc_search_cbret_t
mcview_search_update_cmd_callback (const void *user_data, gsize char_offset)
{
    status_msg_t *sm = STATUS_MSG (user_data);
    mcview_search_status_msg_t *vsm = (mcview_search_status_msg_t *) user_data;
    WView *view = vsm->view;
    gboolean do_update = FALSE;
    mc_search_cbret_t result = MC_SEARCH_CB_OK;

    vsm->offset = (off_t) char_offset;

    if (mcview_search_options.backwards)
    {
        if (vsm->offset <= view->update_activate)
        {
            view->update_activate -= view->update_steps;

            do_update = TRUE;
        }
    }
    else
    {
        if (vsm->offset >= view->update_activate)
        {
            view->update_activate += view->update_steps;

            do_update = TRUE;
        }
    }

    if (do_update && sm->update (sm) == B_CANCEL)
        result = MC_SEARCH_CB_ABORT;

    /* may be in future return from this callback will change current position in searching block. */

    return result;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_do_search (WView * view, off_t want_search_start)
{
    mcview_search_status_msg_t vsm;

    off_t search_start = 0;
    off_t orig_search_start = view->search_start;
    gboolean found = FALSE;

    size_t match_len;

    view->search_start = want_search_start;
    /* for avoid infinite search loop we need to increase or decrease start offset of search */

    if (view->search_start != 0)
    {
        if (!view->mode_flags.nroff)
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
                          nroff->char_length + 1);
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

    if (mcview_search_options.backwards && search_start < 0)
        search_start = 0;

    /* Compute the percent steps */
    mcview_search_update_steps (view);

    view->update_activate = search_start;

    vsm.first = TRUE;
    vsm.view = view;
    vsm.offset = search_start;

    status_msg_init (STATUS_MSG (&vsm), _("Search"), 1.0, simple_status_msg_init_cb,
                     mcview_search_status_update_cb, NULL);

    do
    {
        off_t growbufsize;

        if (view->growbuf_in_use)
            growbufsize = mcview_growbuf_filesize (view);
        else
            growbufsize = view->search->original.str->len;

        if (mcview_find (&vsm, search_start, mcview_get_filesize (view), &match_len))
        {
            mcview_search_show_result (view, match_len);
            found = TRUE;
            break;
        }

        /* Search error is here.
         * MC_SEARCH_E_NOTFOUND: continue search
         * others: stop
         */
        if (view->search->error != MC_SEARCH_E_NOTFOUND)
            break;

        search_start = growbufsize - view->search->original.str->len;
    }
    while (search_start > 0 && mcview_may_still_grow (view));

    /* After mcview_may_still_grow (view) == FALSE we have remained last chunk. Search there. */
    if (view->growbuf_in_use && !found && view->search->error == MC_SEARCH_E_NOTFOUND
        && !mcview_search_options.backwards
        && mcview_find (&vsm, search_start, mcview_get_filesize (view), &match_len))
    {
        mcview_search_show_result (view, match_len);
        found = TRUE;
    }

    status_msg_deinit (STATUS_MSG (&vsm));

    if (orig_search_start != 0 && (!found && view->search->error == MC_SEARCH_E_NOTFOUND)
        && !mcview_search_options.backwards)
    {
        view->search_start = orig_search_start;
        mcview_update (view);

        if (query_dialog
            (_("Search done"), _("Continue from beginning?"), D_NORMAL, 2, _("&Yes"),
             _("&No")) != 0)
            found = TRUE;
        else
        {
            /* continue search from beginning */
            view->update_activate = 0;

            vsm.first = TRUE;
            vsm.view = view;
            vsm.offset = 0;

            status_msg_init (STATUS_MSG (&vsm), _("Search"), 1.0, simple_status_msg_init_cb,
                             mcview_search_status_update_cb, NULL);

            /* search from file begin up to initial search start position */
            if (mcview_find (&vsm, 0, orig_search_start, &match_len))
            {
                mcview_search_show_result (view, match_len);
                found = TRUE;
            }

            status_msg_deinit (STATUS_MSG (&vsm));
        }
    }

    if (!found)
    {
        view->search_start = orig_search_start;
        mcview_update (view);

        if (view->search->error == MC_SEARCH_E_NOTFOUND)
            query_dialog (_("Search"), _(STR_E_NOTFOUND), D_NORMAL, 1, _("&Dismiss"));
        else if (view->search->error_str != NULL)
            query_dialog (_("Search"), view->search->error_str, D_NORMAL, 1, _("&Dismiss"));
    }
    view->dirty++;
}

/* --------------------------------------------------------------------------------------------- */
