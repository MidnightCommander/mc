/*
   Search functions for diffviewer.

   Copyright (C) 2010, 2011
   The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2010.

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

#include "lib/global.h"
#include "lib/strutil.h"
#include "lib/tty/key.h"
#include "lib/widget.h"
#include "lib/charsets.h"

#include "src/history.h"

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define SEARCH_DLG_WIDTH  58
#define SEARCH_DLG_HEIGHT 13

/*** file scope type declarations ****************************************************************/

typedef struct mcdiffview_search_options_struct
{
    mc_search_type_t type;
    gboolean case_sens;
    gboolean backwards;
    gboolean whole_words;
    gboolean all_codepages;
} mcdiffview_search_options_t;

/*** file scope variables ************************************************************************/

static mcdiffview_search_options_t mcdiffview_search_options = {
    .type = MC_SEARCH_T_NORMAL,
    .case_sens = FALSE,
    .backwards = FALSE,
    .whole_words = FALSE,
    .all_codepages = FALSE,
};

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

#define DLG_BTN1_text N_("&Cancel")
#define DLG_BTN2_text N_("&OK")

static void
mcdiffview_dialog_fix_buttons_positions (QuickDialog * dlg)
{
    size_t str_cancel_len, str_ok_len;
    size_t first_start_position;

    str_cancel_len = str_term_width1 (_(DLG_BTN1_text)) + 4;
    str_ok_len = str_term_width1 (_(DLG_BTN2_text)) + 6;

    first_start_position = (SEARCH_DLG_WIDTH - str_cancel_len - str_ok_len - 1) / 2;
    dlg->widgets[1].relative_x = first_start_position;
    dlg->widgets[0].relative_x = first_start_position + str_ok_len + 1;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcdiffview_dialog_search (WDiff * dview)
{
    char *exp = NULL;

    int qd_result;

    size_t num_of_types;
    gchar **list_of_types = mc_search_get_types_strings_array (&num_of_types);

    QuickWidget search_widgets[] = {
        /* 0 */
        QUICK_BUTTON (3, SEARCH_DLG_WIDTH, SEARCH_DLG_HEIGHT - 3, SEARCH_DLG_HEIGHT, DLG_BTN1_text,
                      B_CANCEL, NULL),
        /* 1 */
        QUICK_BUTTON (3, SEARCH_DLG_WIDTH, SEARCH_DLG_HEIGHT - 3, SEARCH_DLG_HEIGHT, DLG_BTN2_text,
                      B_ENTER, NULL),
        /* 2 */
#ifdef HAVE_CHARSET
        QUICK_CHECKBOX (33, SEARCH_DLG_WIDTH, 8, SEARCH_DLG_HEIGHT, N_("&All charsets"),
                        &mcdiffview_search_options.all_codepages),
#endif

        QUICK_CHECKBOX (33, SEARCH_DLG_WIDTH, 7, SEARCH_DLG_HEIGHT, N_("&Whole words"),
                        &mcdiffview_search_options.whole_words),
        /* 3 */
        QUICK_CHECKBOX (33, SEARCH_DLG_WIDTH, 6, SEARCH_DLG_HEIGHT, N_("&Backwards"),
                        &mcdiffview_search_options.backwards),
        /* 4 */
        QUICK_CHECKBOX (33, SEARCH_DLG_WIDTH, 5, SEARCH_DLG_HEIGHT, N_("Cas&e sensitive"),
                        &mcdiffview_search_options.case_sens),
        /* 5 */
        QUICK_RADIO (3, SEARCH_DLG_WIDTH, 5, SEARCH_DLG_HEIGHT,
                     num_of_types, (const char **) list_of_types,
                     (int *) &mcdiffview_search_options.type),
        /* 6 */
        QUICK_INPUT (3, SEARCH_DLG_WIDTH, 3, SEARCH_DLG_HEIGHT,
                     INPUT_LAST_TEXT, SEARCH_DLG_WIDTH - 6, 0,
                     MC_HISTORY_SHARED_SEARCH,
                     &exp),
        /* 7 */
        QUICK_LABEL (3, SEARCH_DLG_WIDTH, 2, SEARCH_DLG_HEIGHT, N_("Enter search string:")),
        QUICK_END
    };

    QuickDialog search_input = {
        SEARCH_DLG_WIDTH, SEARCH_DLG_HEIGHT, -1, -1,
        N_("Search"), "[Input Line Keys]",
        search_widgets, NULL, FALSE
    };

    mcdiffview_dialog_fix_buttons_positions (&search_input);

    qd_result = quick_dialog (&search_input);
    g_strfreev (list_of_types);


    if ((qd_result == B_CANCEL) || (exp == NULL) || (exp[0] == '\0'))
    {
        g_free (exp);
        return FALSE;
    }

#ifdef HAVE_CHARSET
    {
        GString *tmp = str_convert_to_input (exp);

        if (tmp)
        {
            g_free (exp);
            exp = g_string_free (tmp, FALSE);
        }
    }
#endif

    g_free (dview->search.last_string);
    dview->search.last_string = exp;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcdiffview_do_search_backward (WDiff * dview)
{
    ssize_t ind;
    DIFFLN *p;

    if (dview->search.last_accessed_num_line < 0)
    {
        dview->search.last_accessed_num_line = -1;
        return FALSE;
    }

    if ((size_t) dview->search.last_accessed_num_line >= dview->a[dview->ord]->len)
        dview->search.last_accessed_num_line = (ssize_t) dview->a[dview->ord]->len;

    for (ind = --dview->search.last_accessed_num_line; ind >= 0; ind--)
    {
        p = (DIFFLN *) & g_array_index (dview->a[dview->ord], DIFFLN, (size_t) ind);
        if (p->u.len == 0)
            continue;

        if (mc_search_run (dview->search.handle, p->p, 0, p->u.len, NULL))
        {
            dview->skip_rows = dview->search.last_found_line =
                dview->search.last_accessed_num_line = ind;
            return TRUE;
        }
    }
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */


static gboolean
mcdiffview_do_search_forward (WDiff * dview)
{
    size_t ind;
    DIFFLN *p;

    if (dview->search.last_accessed_num_line < 0)
        dview->search.last_accessed_num_line = -1;
    else if ((size_t) dview->search.last_accessed_num_line >= dview->a[dview->ord]->len)
    {
        dview->search.last_accessed_num_line = (ssize_t) dview->a[dview->ord]->len;
        return FALSE;
    }

    for (ind = (size_t)++ dview->search.last_accessed_num_line; ind < dview->a[dview->ord]->len;
         ind++)
    {
        p = (DIFFLN *) & g_array_index (dview->a[dview->ord], DIFFLN, ind);
        if (p->u.len == 0)
            continue;

        if (mc_search_run (dview->search.handle, p->p, 0, p->u.len, NULL))
        {
            dview->skip_rows = dview->search.last_found_line =
                dview->search.last_accessed_num_line = (ssize_t) ind;
            return TRUE;
        }
    }
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static void
mcdiffview_do_search (WDiff * dview)
{
    gboolean present_result = FALSE;

    tty_enable_interrupt_key ();

    if (mcdiffview_search_options.backwards)
    {
        present_result = mcdiffview_do_search_backward (dview);
    }
    else
    {
        present_result = mcdiffview_do_search_forward (dview);
    }

    tty_disable_interrupt_key ();

    if (!present_result)
    {
        dview->search.last_found_line = -1;
        error_dialog (_("Search"), _("Search string not found"));
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
dview_search_cmd (WDiff * dview)
{
    if (dview->dsrc != DATA_SRC_MEM)
    {
        error_dialog (_("Search"), _("Search is disabled"));
        return;
    }

    if (!mcdiffview_dialog_search (dview))
        return;

    mc_search_free (dview->search.handle);
    dview->search.handle = mc_search_new (dview->search.last_string, -1);

    if (dview->search.handle == NULL)
        return;

    dview->search.handle->search_type = mcdiffview_search_options.type;
    dview->search.handle->is_all_charsets = mcdiffview_search_options.all_codepages;
    dview->search.handle->is_case_sensitive = mcdiffview_search_options.case_sens;
    dview->search.handle->whole_words = mcdiffview_search_options.whole_words;

    mcdiffview_do_search (dview);
}

/* --------------------------------------------------------------------------------------------- */

void
dview_continue_search_cmd (WDiff * dview)
{
    if (dview->dsrc != DATA_SRC_MEM)
        error_dialog (_("Search"), _("Search is disabled"));
    else if (dview->search.handle == NULL)
        dview_search_cmd (dview);
    else
        mcdiffview_do_search (dview);
}

/* --------------------------------------------------------------------------------------------- */
