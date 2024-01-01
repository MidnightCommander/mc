/*
   Internal file viewer for the Midnight Commander
   Function for paint dialogs

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

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_dialog_search (WView * view)
{
    char *exp = NULL;
    int qd_result;
    size_t num_of_types = 0;
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

        WRect r = { -1, -1, 0, 58 };

        quick_dialog_t qdlg = {
            r, N_("Search"), "[Input Line Keys]",
            quick_widgets, NULL, NULL
        };

        qd_result = quick_dialog (&qdlg);
    }

    g_strfreev (list_of_types);

    if (qd_result == B_CANCEL || exp[0] == '\0')
    {
        g_free (exp);
        return FALSE;
    }

#ifdef HAVE_CHARSET
    {
        GString *tmp;

        tmp = str_convert_to_input (exp);
        g_free (exp);
        if (tmp != NULL)
            exp = g_string_free (tmp, FALSE);
        else
            exp = g_strdup ("");
    }
#endif

    mcview_search_deinit (view);
    view->last_search_string = exp;

    return mcview_search_init (view);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_dialog_goto (WView * view, off_t * offset)
{
    typedef enum
    {
        MC_VIEW_GOTO_LINENUM = 0,
        MC_VIEW_GOTO_PERCENT = 1,
        MC_VIEW_GOTO_OFFSET_DEC = 2,
        MC_VIEW_GOTO_OFFSET_HEX = 3
    } mcview_goto_type_t;

    const char *mc_view_goto_str[] = {
        N_("&Line number"),
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

        WRect r = { -1, -1, 0, 40 };

        quick_dialog_t qdlg = {
            r, N_("Goto"), "[Input Line Keys]",
            quick_widgets, NULL, NULL
        };

        /* run dialog */
        qd_result = quick_dialog (&qdlg);
    }

    *offset = -1;

    /* check input line value */
    res = (qd_result != B_CANCEL && exp[0] != '\0');
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
                /* Line number entered by user is 1-based. */
                if (addr > 0)
                    addr--;
                mcview_coord_to_offset (view, offset, addr, 0);
                *offset = mcview_bol (view, *offset, 0);
                break;
            case MC_VIEW_GOTO_PERCENT:
                if (addr > 100)
                    addr = 100;
                /* read all data from pipe to get real size */
                if (view->growbuf_in_use)
                    mcview_growbuf_read_all_data (view);
                *offset = addr * mcview_get_filesize (view) / 100;
                if (!view->mode_flags.hex)
                    *offset = mcview_bol (view, *offset, 0);
                break;
            case MC_VIEW_GOTO_OFFSET_DEC:
            case MC_VIEW_GOTO_OFFSET_HEX:
                if (!view->mode_flags.hex)
                {
                    if (view->growbuf_in_use)
                        mcview_growbuf_read_until (view, addr);

                    *offset = mcview_bol (view, addr, 0);
                }
                else
                {
                    /* read all data from pipe to get real size */
                    if (view->growbuf_in_use)
                        mcview_growbuf_read_all_data (view);

                    *offset = addr;
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

/* --------------------------------------------------------------------------------------------- */
