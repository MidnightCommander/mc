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
#include "../src/viewer/internal.h"

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
    enum {
        SEARCH_DLG_MIN_HEIGHT = 10,
        SEARCH_DLG_HEIGHT_SUPPLY = 3,
        SEARCH_DLG_WIDTH = 58
    };

    char *defval = g_strdup (view->last_search_string != NULL ? view->last_search_string : "");
    char *exp = NULL;
#ifdef HAVE_CHARSET
    GString *tmp;
#endif

    int ttype_of_search = (int) view->search_type;
    int tall_codepages = (int) view->search_all_codepages;
    int tsearch_case = (int) view->search_case;
    int tsearch_backwards = (int) view->search_backwards;
    int qd_result;

    gchar **list_of_types = mc_search_get_types_strings_array ();
    int SEARCH_DLG_HEIGHT =
        SEARCH_DLG_MIN_HEIGHT + g_strv_length (list_of_types) - SEARCH_DLG_HEIGHT_SUPPLY;

    QuickWidget quick_widgets[] = {

        {quick_button, 6, 10, SEARCH_DLG_HEIGHT - 3, SEARCH_DLG_HEIGHT, N_("&Cancel"), 0,
         B_CANCEL, 0, 0, NULL, NULL, NULL},

        {quick_button, 2, 10, SEARCH_DLG_HEIGHT - 3, SEARCH_DLG_HEIGHT, N_("&OK"), 0, B_ENTER,
         0, 0, NULL, NULL, NULL},

#ifdef HAVE_CHARSET
        {quick_checkbox, SEARCH_DLG_WIDTH / 2 + 3, SEARCH_DLG_WIDTH, 6, SEARCH_DLG_HEIGHT,
         N_("All charsets"), 0, 0,
         &tall_codepages, 0, NULL, NULL, NULL},
#endif

        {quick_checkbox, SEARCH_DLG_WIDTH / 2 + 3, SEARCH_DLG_WIDTH, 5, SEARCH_DLG_HEIGHT,
         N_("&Backwards"), 0, 0, &tsearch_backwards, 0, NULL, NULL, NULL},

        {quick_checkbox, SEARCH_DLG_WIDTH / 2 + 3, SEARCH_DLG_WIDTH, 4, SEARCH_DLG_HEIGHT,
         N_("case &Sensitive"), 0, 0,
         &tsearch_case, 0, NULL, NULL, NULL},

        {quick_radio, 3, SEARCH_DLG_WIDTH, 4, SEARCH_DLG_HEIGHT, 0, g_strv_length (list_of_types),
         ttype_of_search,
         (void *) &ttype_of_search, const_cast (char **, list_of_types), NULL, NULL, NULL},


        {quick_input, 3, SEARCH_DLG_WIDTH, 3, SEARCH_DLG_HEIGHT, defval, 52, 0,
         0, &exp, N_("Search"), NULL, NULL},

        {quick_label, 2, SEARCH_DLG_WIDTH, 2, SEARCH_DLG_HEIGHT,
         N_(" Enter search string:"), 0, 0, 0, 0, 0, NULL, NULL},

        NULL_QuickWidget
    };

    QuickDialog Quick_input = {
        SEARCH_DLG_WIDTH, SEARCH_DLG_HEIGHT, -1, 0, N_("Search"),
        "[Input Line Keys]", quick_widgets, 0
    };

    convert_to_display (defval);

    qd_result = quick_dialog (&Quick_input);
    g_strfreev (list_of_types);

    if (qd_result == B_CANCEL) {
        g_free (exp);
        g_free (defval);
        return FALSE;
    }

    view->search_backwards = tsearch_backwards;
    view->search_type = (mc_search_type_t) ttype_of_search;

    view->search_all_codepages = (gboolean) tall_codepages;
    view->search_case = (gboolean) tsearch_case;

    if (exp == NULL || exp[0] == '\0') {
        g_free (exp);
        g_free (defval);
        return FALSE;
    }
#ifdef HAVE_CHARSET
    tmp = str_convert_to_input (exp);

    if (tmp) {
        g_free (exp);
        exp = g_string_free (tmp, FALSE);
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
        g_free (defval);
        return FALSE;
    }

    view->search->search_type = view->search_type;
    view->search->is_all_charsets = view->search_all_codepages;
    view->search->is_case_sentitive = view->search_case;
    view->search->search_fn = mcview_search_cmd_callback;
    view->search->update_fn = mcview_search_update_cmd_callback;

    g_free (exp);
    g_free (defval);
    return TRUE;
}
