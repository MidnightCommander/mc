/*
   Events diffviewer.

   Copyright (C) 2010-2014
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2010.
   Andrew Borodin <aborodin@vmail.ru>, 2012

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

#include <stdio.h>
#include <errno.h>

#include "lib/global.h"
#include "lib/event.h"
#include "lib/strutil.h"
#include "lib/vfs/vfs.h"        /* mc_opendir, mc_readdir, mc_closedir, */
#include "lib/tty/key.h"
#include "lib/widget.h"

#include "src/filemanager/cmd.h"        /* edit_file_at_line(), view_other_cmd() */
#include "src/history.h"
#include "src/setup.h"

#include "internal.h"
#include "options.h"

#ifdef HAVE_CHARSET
#include "charset.h"
#endif

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

typedef enum
{
    MC_DVIEW_SPLIT_EQUAL = 1,
    MC_DVIEW_SPLIT_FULL,
    MC_DVIEW_SPLIT_MORE,
    MC_DVIEW_SPLIT_LESS,
} mc_diffviewer_split_type_t;

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/* event callback */

static gboolean
mc_diffviewer_cmd_show_symbols (event_info_t * event_info, gpointer data, GError ** error)
{
    WDiff *dview = (WDiff *) data;

    (void) event_info;
    (void) error;

    dview->display_symbols ^= 1;
    dview->new_frame = 1;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* event callback */

static gboolean
mc_diffviewer_cmd_show_numbers (event_info_t * event_info, gpointer data, GError ** error)
{
    WDiff *dview = (WDiff *) data;

    (void) event_info;
    (void) error;

    dview->display_numbers ^= mc_diffviewer_calc_nwidth ((const GArray ** const) dview->a);
    dview->new_frame = 1;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* event callback */

static gboolean
mc_diffviewer_cmd_split_views (event_info_t * event_info, gpointer data, GError ** error)
{
    WDiff *dview = (WDiff *) data;
    mc_diffviewer_split_type_t split_type;

    (void) error;

    if (event_info->init_data == NULL)
        return TRUE;

    split_type = (mc_diffviewer_split_type_t) event_info->init_data;

    if (split_type == MC_DVIEW_SPLIT_FULL)
    {
        dview->full ^= 1;
        dview->new_frame = 1;
    }
    else if (!dview->full)
    {
        switch (split_type)
        {
        case MC_DVIEW_SPLIT_EQUAL:
            dview->bias = 0;
            break;
        case MC_DVIEW_SPLIT_MORE:
            mc_diffviewer_compute_split (dview, 1);
            break;
        case MC_DVIEW_SPLIT_LESS:
            mc_diffviewer_compute_split (dview, -1);
            break;
        default:
            return TRUE;
        }
        dview->new_frame = 1;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* event callback */

static gboolean
mc_diffviewer_cmd_set_tab_size (event_info_t * event_info, gpointer data, GError ** error)
{
    WDiff *dview = (WDiff *) data;

    (void) error;

    if (event_info->init_data != NULL)
        dview->tab_size = (int) (intptr_t) event_info->init_data;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* event callback */

static gboolean
mc_diffviewer_cmd_swap (event_info_t * event_info, gpointer data, GError ** error)
{
    WDiff *dview = (WDiff *) data;

    (void) event_info;
    (void) error;

    dview->ord ^= 1;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* event callback */

static gboolean
mc_diffviewer_cmd_redo (event_info_t * event_info, gpointer data, GError ** error)
{
    WDiff *dview = (WDiff *) data;

    (void) event_info;
    (void) error;

    if (dview->display_numbers)
    {
        int old;

        old = dview->display_numbers;
        dview->display_numbers = mc_diffviewer_calc_nwidth ((const GArray **) dview->a);
        dview->new_frame = (old != dview->display_numbers);
    }
    mc_diffviewer_reread (dview);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* event callback */

static gboolean
mc_diffviewer_cmd_hunk_next (event_info_t * event_info, gpointer data, GError ** error)
{
    WDiff *dview = (WDiff *) data;
    const GArray *a;
    diff_place_t diff_place;

    (void) event_info;
    (void) error;

    diff_place = (diff_place_t) event_info->init_data;
    a = dview->a[diff_place];

    while ((size_t) dview->skip_rows < a->len
           && ((DIFFLN *) & g_array_index (a, DIFFLN, dview->skip_rows))->ch != EQU_CH)
        dview->skip_rows++;
    while ((size_t) dview->skip_rows < a->len
           && ((DIFFLN *) & g_array_index (a, DIFFLN, dview->skip_rows))->ch == EQU_CH)
        dview->skip_rows++;

    dview->search.last_accessed_num_line = dview->skip_rows;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* event callback */

static gboolean
mc_diffviewer_cmd_hunk_prev (event_info_t * event_info, gpointer data, GError ** error)
{
    WDiff *dview = (WDiff *) data;
    const GArray *a;
    diff_place_t diff_place;

    (void) error;

    diff_place = (diff_place_t) event_info->init_data;
    a = dview->a[diff_place];

    while (dview->skip_rows > 0
           && ((DIFFLN *) & g_array_index (a, DIFFLN, dview->skip_rows))->ch != EQU_CH)
        dview->skip_rows--;
    while (dview->skip_rows > 0
           && ((DIFFLN *) & g_array_index (a, DIFFLN, dview->skip_rows))->ch == EQU_CH)
        dview->skip_rows--;
    while (dview->skip_rows > 0
           && ((DIFFLN *) & g_array_index (a, DIFFLN, dview->skip_rows))->ch != EQU_CH)
        dview->skip_rows--;
    if (dview->skip_rows > 0 && (size_t) dview->skip_rows < a->len)
        dview->skip_rows++;

    dview->search.last_accessed_num_line = dview->skip_rows;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* event callback */

static gboolean
mc_diffviewer_cmd_goto_line (event_info_t * event_info, gpointer data, GError ** error)
{
    WDiff *dview = (WDiff *) data;
    diff_place_t ord = (diff_place_t) event_info->init_data;
    /* *INDENT-OFF* */
    static const char *title[2] = {
        N_("Goto line (left)"),
        N_("Goto line (right)")
    };
    /* *INDENT-ON* */
    static char prev[256];
    /* XXX some statics here, to be remembered between runs */

    int newline;
    char *input;

    (void) error;

    input =
        input_dialog (_(title[ord]), _("Enter line:"), MC_HISTORY_YDIFF_GOTO_LINE, prev,
                      INPUT_COMPLETE_NONE);
    if (input != NULL)
    {
        const char *s = input;

        if (mc_diffviewer_scan_deci (&s, &newline) == 0 && *s == '\0')
        {
            size_t i = 0;

            if (newline > 0)
            {
                for (; i < dview->a[ord]->len; i++)
                {
                    const DIFFLN *p;

                    p = &g_array_index (dview->a[ord], DIFFLN, i);
                    if (p->line == newline)
                        break;
                }
            }
            dview->skip_rows = dview->search.last_accessed_num_line = (ssize_t) i;
            g_snprintf (prev, sizeof (prev), "%d", newline);
        }
        g_free (input);
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* event callback */

static gboolean
mc_diffviewer_cmd_edit (event_info_t * event_info, gpointer data, GError ** error)
{
    WDiff *dview = (WDiff *) data;
    diff_place_t diff_place = (diff_place_t) event_info->init_data;

    WDialog *h;
    gboolean h_modal;
    int linenum, lineofs;

    switch (diff_place)
    {
    case DIFF_CURRENT:
        diff_place = dview->ord;
        break;
    case DIFF_OTHER:
        diff_place = dview->ord ^ 1;
        break;
    case DIFF_LEFT:
    case DIFF_RIGHT:
        break;
    default:
        return FALSE;
    }

    if (dview->dsrc == DATA_SRC_TMP)
    {
        error_dialog (_("Edit"), _("Edit is disabled"));
        return TRUE;
    }

    h = WIDGET (dview)->owner;
    h_modal = h->modal;

    mc_diffviewer_get_line_numbers (dview->a[diff_place], dview->skip_rows, &linenum, &lineofs);
    h->modal = TRUE;            /* not allow edit file in several editors */
    {
        vfs_path_t *tmp_vpath;

        tmp_vpath = vfs_path_from_str (dview->file[diff_place]);
        edit_file_at_line (tmp_vpath, use_internal_edit != 0, linenum);
        vfs_path_free (tmp_vpath);
    }
    h->modal = h_modal;
    mc_event_raise (MCEVENT_GROUP_DIFFVIEWER, "redo", dview, NULL, error);
    mc_diffviewer_update (dview);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* event callback */

static gboolean
mc_diffviewer_cmd_goto_top (event_info_t * event_info, gpointer data, GError ** error)
{
    WDiff *dview = (WDiff *) data;

    (void) event_info;
    (void) error;

    dview->skip_rows = dview->search.last_accessed_num_line = 0;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* event callback */

static gboolean
mc_diffviewer_cmd_goto_bottom (event_info_t * event_info, gpointer data, GError ** error)
{
    WDiff *dview = (WDiff *) data;

    (void) event_info;
    (void) error;

    dview->skip_rows = dview->search.last_accessed_num_line = dview->a[DIFF_LEFT]->len - 1;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* event callback */

static gboolean
mc_diffviewer_cmd_goto_up (event_info_t * event_info, gpointer data, GError ** error)
{
    WDiff *dview = (WDiff *) data;

    (void) event_info;
    (void) error;

    if (dview->skip_rows > 0)
    {
        dview->skip_rows--;
        dview->search.last_accessed_num_line = dview->skip_rows;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* event callback */

static gboolean
mc_diffviewer_cmd_goto_down (event_info_t * event_info, gpointer data, GError ** error)
{
    WDiff *dview = (WDiff *) data;

    (void) event_info;
    (void) error;

    dview->skip_rows++;
    dview->search.last_accessed_num_line = dview->skip_rows;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* event callback */

static gboolean
mc_diffviewer_cmd_goto_page_up (event_info_t * event_info, gpointer data, GError ** error)
{
    WDiff *dview = (WDiff *) data;

    (void) event_info;
    (void) error;

    if (dview->height > 2)
    {
        dview->skip_rows -= dview->height - 2;
        dview->search.last_accessed_num_line = dview->skip_rows;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* event callback */

static gboolean
mc_diffviewer_cmd_goto_page_down (event_info_t * event_info, gpointer data, GError ** error)
{
    WDiff *dview = (WDiff *) data;

    (void) event_info;
    (void) error;

    if (dview->height > 2)
    {
        dview->skip_rows += dview->height - 2;
        dview->search.last_accessed_num_line = dview->skip_rows;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* event callback */

static gboolean
mc_diffviewer_cmd_goto_left (event_info_t * event_info, gpointer data, GError ** error)
{
    WDiff *dview = (WDiff *) data;

    (void) error;

    dview->skip_cols -= (int) (intptr_t) event_info->init_data;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* event callback */

static gboolean
mc_diffviewer_cmd_goto_right (event_info_t * event_info, gpointer data, GError ** error)
{
    WDiff *dview = (WDiff *) data;

    (void) error;

    dview->skip_cols += (int) (intptr_t) event_info->init_data;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* event callback */

static gboolean
mc_diffviewer_cmd_goto_start_of_line (event_info_t * event_info, gpointer data, GError ** error)
{
    WDiff *dview = (WDiff *) data;

    (void) event_info;
    (void) error;

    dview->skip_cols = 0;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* event callback */

static gboolean
mc_diffviewer_cmd_quit (event_info_t * event_info, gpointer data, GError ** error)
{
    WDiff *dview = (WDiff *) data;

    (void) event_info;
    (void) error;

    dview->view_quit = 1;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* event callback */

static gboolean
mc_diffviewer_cmd_save_changes (event_info_t * event_info, gpointer data, GError ** error)
{
    WDiff *dview = (WDiff *) data;
    gboolean res;

    (void) event_info;
    (void) error;

    if (dview->merged[DIFF_LEFT])
    {
        res = mc_util_unlink_backup_if_possible (dview->file[DIFF_LEFT], "~~~");
        dview->merged[DIFF_LEFT] = !res;
    }
    if (dview->merged[DIFF_RIGHT])
    {
        res = mc_util_unlink_backup_if_possible (dview->file[DIFF_RIGHT], "~~~");
        dview->merged[DIFF_RIGHT] = !res;
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mc_diffviewer_init_events (GError ** error)
{
    /* *INDENT-OFF* */
    event_init_group_t core_group_events[] =
    {
        {"run", mc_diffviewer_cmd_run, NULL},
        {"show_symbols", mc_diffviewer_cmd_show_symbols, NULL},
        {"show_numbers", mc_diffviewer_cmd_show_numbers, NULL},
        {"split_full",  mc_diffviewer_cmd_split_views, (void *) MC_DVIEW_SPLIT_FULL},
        {"split_equal", mc_diffviewer_cmd_split_views, (void *) MC_DVIEW_SPLIT_EQUAL},
        {"split_more",  mc_diffviewer_cmd_split_views, (void *) MC_DVIEW_SPLIT_MORE},
        {"split_less",  mc_diffviewer_cmd_split_views, (void *) MC_DVIEW_SPLIT_LESS},
        {"tab_size_2", mc_diffviewer_cmd_set_tab_size, (void *) 2},
        {"tab_size_3", mc_diffviewer_cmd_set_tab_size, (void *) 3},
        {"tab_size_4", mc_diffviewer_cmd_set_tab_size, (void *) 4},
        {"tab_size_8", mc_diffviewer_cmd_set_tab_size, (void *) 8},
        {"swap", mc_diffviewer_cmd_swap, NULL},
        {"redo", mc_diffviewer_cmd_redo, NULL},
        {"hunk_next", mc_diffviewer_cmd_hunk_next, (void *) DIFF_LEFT},
        {"hunk_prev", mc_diffviewer_cmd_hunk_prev, (void *) DIFF_LEFT},
        {"goto_line", mc_diffviewer_cmd_goto_line, (void *) DIFF_RIGHT},

        {"edit_current", mc_diffviewer_cmd_edit, (void *) DIFF_CURRENT},
        {"edit_other", mc_diffviewer_cmd_edit, (void *) DIFF_OTHER},
        {"edit_left", mc_diffviewer_cmd_edit, (void *) DIFF_LEFT},
        {"edit_right", mc_diffviewer_cmd_edit, (void *) DIFF_RIGHT},

        {"merge_from_left_to_right", mc_diffviewer_cmd_merge, (void *) DIFF_LEFT},
        {"merge_from_right_to_left", mc_diffviewer_cmd_merge, (void *) DIFF_RIGHT},
        {"merge_from_current_to_other", mc_diffviewer_cmd_merge, (void *) DIFF_CURRENT},
        {"merge_from_other_to_current", mc_diffviewer_cmd_merge, (void *) DIFF_OTHER},
        {"search", mc_diffviewer_cmd_search, NULL},
        {"continue_search", mc_diffviewer_cmd_continue_search, NULL},
        {"goto_top", mc_diffviewer_cmd_goto_top, NULL},
        {"goto_bottom", mc_diffviewer_cmd_goto_bottom, NULL},
        {"goto_up", mc_diffviewer_cmd_goto_up, NULL},
        {"goto_down", mc_diffviewer_cmd_goto_down, NULL},
        {"goto_page_up", mc_diffviewer_cmd_goto_page_up, NULL},
        {"goto_page_down", mc_diffviewer_cmd_goto_page_down, NULL},
        {"goto_left", mc_diffviewer_cmd_goto_left, (void *) 1},
        {"goto_left_quick", mc_diffviewer_cmd_goto_left, (void *) 8},
        {"goto_right", mc_diffviewer_cmd_goto_right, (void *) 1},
        {"goto_right_quick", mc_diffviewer_cmd_goto_right, (void *) 8},
        {"goto_start_of_line", mc_diffviewer_cmd_goto_start_of_line, (void *) 8},
        {"quit", mc_diffviewer_cmd_quit, (void *) 8},
        {"save_changes", mc_diffviewer_cmd_save_changes, (void *) 8},

        {"options_show_dialog", mc_diffviewer_cmd_options_show_dialog, NULL},
        {"options_save", mc_diffviewer_cmd_options_save, NULL},
        {"options_load", mc_diffviewer_cmd_options_load, NULL},
#ifdef HAVE_CHARSET
        {"select_encoding_show_dialog", mc_diffviewer_cmd_select_encoding_show_dialog, NULL},
#endif /* HAVE_CHARSET */

        {NULL, NULL, NULL}
    };
    /* *INDENT-ON* */

    /* *INDENT-OFF* */
    event_init_t standard_events[] =
    {
        {MCEVENT_GROUP_DIFFVIEWER, core_group_events},
        {NULL, NULL}
    };
    /* *INDENT-ON* */

    mc_event_mass_add (standard_events, error);
}

/* --------------------------------------------------------------------------------------------- */
