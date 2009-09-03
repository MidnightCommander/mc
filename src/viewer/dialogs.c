/*
   Internal file viewer for the Midnight Commander
   Function for paint dialogs

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
#include "../src/wtools.h"
#include "internal.h"
#include "../src/history.h"
#include "../src/charsets.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

/*** public functions ****************************************************************************/

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_dialog_search (mcview_t * view)
{
    int SEARCH_DLG_MIN_HEIGHT = 12;
    int SEARCH_DLG_HEIGHT_SUPPLY = 3;
    int SEARCH_DLG_WIDTH = 58;

    char *exp = NULL;
    int qd_result;

    gchar **list_of_types = mc_search_get_types_strings_array ();
    int SEARCH_DLG_HEIGHT =
	SEARCH_DLG_MIN_HEIGHT + g_strv_length (list_of_types) - SEARCH_DLG_HEIGHT_SUPPLY;

    QuickWidget quick_widgets[] = {
	QUICK_BUTTON (6, 10, SEARCH_DLG_HEIGHT - 3, SEARCH_DLG_HEIGHT, N_("&Cancel"), B_CANCEL, NULL),
	QUICK_BUTTON (2, 10, SEARCH_DLG_HEIGHT - 3, SEARCH_DLG_HEIGHT, N_("&OK"), B_ENTER, NULL),
#ifdef HAVE_CHARSET
	QUICK_CHECKBOX (SEARCH_DLG_WIDTH / 2 + 3, SEARCH_DLG_WIDTH, 8, SEARCH_DLG_HEIGHT,
			N_("All charsets"), &view->search_all_codepages),
#endif
	QUICK_CHECKBOX (SEARCH_DLG_WIDTH / 2 + 3, SEARCH_DLG_WIDTH, 7, SEARCH_DLG_HEIGHT,
			N_("&Whole words"), &view->whole_words),
	QUICK_CHECKBOX (SEARCH_DLG_WIDTH / 2 + 3, SEARCH_DLG_WIDTH, 6, SEARCH_DLG_HEIGHT,
			N_("&Backwards"), &view->search_backwards),
	QUICK_CHECKBOX (SEARCH_DLG_WIDTH / 2 + 3, SEARCH_DLG_WIDTH, 5, SEARCH_DLG_HEIGHT,
			N_("case &Sensitive"),  &view->search_case),
	QUICK_RADIO (3, SEARCH_DLG_WIDTH, 5, SEARCH_DLG_HEIGHT,
			g_strv_length (list_of_types), (const char **) list_of_types, &view->search_type),
	QUICK_INPUT (3, SEARCH_DLG_WIDTH, 3, SEARCH_DLG_HEIGHT,
			INPUT_LAST_TEXT, SEARCH_DLG_WIDTH - 6, 0, MC_HISTORY_SHARED_SEARCH, &exp),
	QUICK_LABEL (2, SEARCH_DLG_WIDTH, 2, SEARCH_DLG_HEIGHT, N_(" Enter search string:")),
	QUICK_END
    };

    QuickDialog Quick_input =
    {
	SEARCH_DLG_WIDTH, SEARCH_DLG_HEIGHT, -1, -1,
	N_("Search"), "[Input Line Keys]",
	quick_widgets, FALSE
    };

    qd_result = quick_dialog (&Quick_input);
    g_strfreev (list_of_types);

    if ((qd_result == B_CANCEL) ||(exp == NULL) || (exp[0] == '\0')) {
        g_free (exp);
        return FALSE;
    }

#ifdef HAVE_CHARSET
    {
	GString *tmp = str_convert_to_input (exp);

	if (tmp) {
	    g_free (exp);
	    exp = g_string_free (tmp, FALSE);
	}
    }
#endif

    g_free (view->last_search_string);
    view->last_search_string = exp;
    exp = NULL;

    if (view->search_nroff_seq)
        mcview_nroff_seq_free (&(view->search_nroff_seq));

    if (view->search)
        mc_search_free (view->search);

    view->search = mc_search_new (view->last_search_string, -1);
    view->search_nroff_seq = mcview_nroff_seq_new (view);
    if (!view->search) {
        g_free (exp);
        return FALSE;
    }

    view->search->search_type = view->search_type;
    view->search->is_all_charsets = view->search_all_codepages;
    view->search->is_case_sentitive = view->search_case;
    view->search->search_fn = mcview_search_cmd_callback;
    view->search->update_fn = mcview_search_update_cmd_callback;
    view->search->whole_words = view->whole_words;

    g_free (exp);
    return TRUE;
}
