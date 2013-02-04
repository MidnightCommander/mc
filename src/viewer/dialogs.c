/*
   Internal file viewer for the Midnight Commander
   Function for paint dialogs

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
   Andrew Borodin <aborodin@vmail.ru>, 2009, 2012
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

#include <stdlib.h>
#include <sys/types.h>

#include "lib/global.h"
#include "lib/search.h"
#include "lib/strutil.h"
#include "lib/widget.h"
#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#endif

#include "src/history.h"

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

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_dialog_search (mcview_t * view)
{
    char *exp = NULL;
    int qd_result;
    size_t num_of_types;
    gchar **list_of_types;

    list_of_types = mc_search_get_types_strings_array (&num_of_types);

    {
        quick_widget_t quick_widgets[] = {
            /* *INDENT-OFF* */
            QUICK_LABELED_INPUT (N_("Enter search string:"), input_label_above,
                                 INPUT_LAST_TEXT, MC_HISTORY_SHARED_SEARCH, &exp,
                                 NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
            QUICK_SEPARATOR (TRUE),
            QUICK_START_COLUMNS,
                QUICK_RADIO (num_of_types, (const char **) list_of_types,
                             (int *) &mcview_search_options.type, NULL),
            QUICK_NEXT_COLUMN,
                QUICK_CHECKBOX (N_("Cas&e sensitive"), &mcview_search_options.case_sens, NULL),
                QUICK_CHECKBOX (N_("&Backwards"), &mcview_search_options.backwards, NULL),
                QUICK_CHECKBOX (N_("&Whole words"), &mcview_search_options.whole_words, NULL),
#ifdef HAVE_CHARSET
                QUICK_CHECKBOX (N_("&All charsets"), &mcview_search_options.all_codepages, NULL),
#endif
            QUICK_STOP_COLUMNS,
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END
            /* *INDENT-ON* */
        };

        quick_dialog_t qdlg = {
            -1, -1, 58,
            N_("Search"), "[Input Line Keys]",
            quick_widgets, NULL, NULL
        };

        qd_result = quick_dialog (&qdlg);
    }

    g_strfreev (list_of_types);

    if ((qd_result == B_CANCEL) || (exp == NULL) || (exp[0] == '\0'))
    {
        g_free (exp);
        return FALSE;
    }

#ifdef HAVE_CHARSET
    {
        GString *tmp;

        tmp = str_convert_to_input (exp);
        if (tmp != NULL)
        {
            g_free (exp);
            exp = g_string_free (tmp, FALSE);
        }
    }
#endif

    g_free (view->last_search_string);
    view->last_search_string = exp;
    mcview_nroff_seq_free (&view->search_nroff_seq);
    mc_search_free (view->search);

    view->search = mc_search_new (view->last_search_string, -1);
    view->search_nroff_seq = mcview_nroff_seq_new (view);
    if (view->search != NULL)
    {
        view->search->search_type = mcview_search_options.type;
        view->search->is_all_charsets = mcview_search_options.all_codepages;
        view->search->is_case_sensitive = mcview_search_options.case_sens;
        view->search->whole_words = mcview_search_options.whole_words;
        view->search->search_fn = mcview_search_cmd_callback;
        view->search->update_fn = mcview_search_update_cmd_callback;
    }

    return (view->search != NULL);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_dialog_goto (mcview_t * view, off_t * offset)
{
    typedef enum
    {
        MC_VIEW_GOTO_LINENUM = 0,
        MC_VIEW_GOTO_PERCENT = 1,
        MC_VIEW_GOTO_OFFSET_DEC = 2,
        MC_VIEW_GOTO_OFFSET_HEX = 3
    } mcview_goto_type_t;

    const char *mc_view_goto_str[] = {
        N_("&Line number (decimal)"),
        N_("Pe&rcents"),
        N_("&Decimal offset"),
        N_("He&xadecimal offset")
    };

    static mcview_goto_type_t current_goto_type = MC_VIEW_GOTO_LINENUM;

    size_t num_of_types;
    char *exp = NULL;
    int qd_result;
    gboolean res;

    num_of_types = G_N_ELEMENTS (mc_view_goto_str);

#ifdef ENABLE_NLS
    {
        size_t i;

        for (i = 0; i < num_of_types; i++)
            mc_view_goto_str[i] = _(mc_view_goto_str[i]);
    }
#endif

    {
        quick_widget_t quick_widgets[] = {
            /* *INDENT-OFF* */
            QUICK_INPUT (INPUT_LAST_TEXT, MC_HISTORY_VIEW_GOTO, &exp, NULL,
                         FALSE, FALSE, INPUT_COMPLETE_NONE),
            QUICK_RADIO (num_of_types, (const char **) mc_view_goto_str, (int *) &current_goto_type,
                         NULL),
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END
            /* *INDENT-ON* */
        };

        quick_dialog_t qdlg = {
            -1, -1, 40,
            N_("Goto"), "[Input Line Keys]",
            quick_widgets, NULL, NULL
        };

        /* run dialog */
        qd_result = quick_dialog (&qdlg);
    }

    *offset = -1;

    /* check input line value */
    res = (qd_result != B_CANCEL && exp != NULL && exp[0] != '\0');
    if (res)
    {
        int base = (current_goto_type == MC_VIEW_GOTO_OFFSET_HEX) ? 16 : 10;
        off_t addr;
        char *error;

        addr = (off_t) g_ascii_strtoll (exp, &error, base);
        if ((*error == '\0') && (addr >= 0))
        {
            switch (current_goto_type)
            {
            case MC_VIEW_GOTO_LINENUM:
                mcview_coord_to_offset (view, offset, addr, 0);
                *offset = mcview_bol (view, *offset, 0);
                break;
            case MC_VIEW_GOTO_PERCENT:
                if (addr > 100)
                    addr = 100;
                *offset = addr * mcview_get_filesize (view) / 100;
                if (!view->hex_mode)
                    *offset = mcview_bol (view, *offset, 0);
                break;
            case MC_VIEW_GOTO_OFFSET_DEC:
            case MC_VIEW_GOTO_OFFSET_HEX:
                *offset = addr;
                if (!view->hex_mode)
                    *offset = mcview_bol (view, *offset, 0);
                else
                {
                    addr = mcview_get_filesize (view);
                    if (*offset > addr)
                        *offset = addr;
                }
                break;
            default:
                *offset = 0;
                break;
            }
        }
    }

    g_free (exp);
    return res;
}
