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

#include <stdlib.h>
#include <sys/types.h>

#include "lib/global.h"
#include "lib/search.h"
#include "lib/strutil.h"

#include "src/wtools.h"
#include "src/history.h"
#include "src/charsets.h"

#include "internal.h"

/*** global variables ****************************************************************************/

mcview_search_options_t mcview_search_options =
{
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

    size_t num_of_types;
    gchar **list_of_types = mc_search_get_types_strings_array (&num_of_types);
    int SEARCH_DLG_HEIGHT = SEARCH_DLG_MIN_HEIGHT + num_of_types - SEARCH_DLG_HEIGHT_SUPPLY;

    QuickWidget quick_widgets[] =
    {
	QUICK_BUTTON (6, 10, SEARCH_DLG_HEIGHT - 3, SEARCH_DLG_HEIGHT, N_("&Cancel"), B_CANCEL, NULL),
	QUICK_BUTTON (2, 10, SEARCH_DLG_HEIGHT - 3, SEARCH_DLG_HEIGHT, N_("&OK"), B_ENTER, NULL),
#ifdef HAVE_CHARSET
	QUICK_CHECKBOX (SEARCH_DLG_WIDTH / 2 + 3, SEARCH_DLG_WIDTH, 8, SEARCH_DLG_HEIGHT,
			N_("All charsets"), &mcview_search_options.all_codepages),
#endif
	QUICK_CHECKBOX (SEARCH_DLG_WIDTH / 2 + 3, SEARCH_DLG_WIDTH, 7, SEARCH_DLG_HEIGHT,
			N_("&Whole words"), &mcview_search_options.whole_words),
	QUICK_CHECKBOX (SEARCH_DLG_WIDTH / 2 + 3, SEARCH_DLG_WIDTH, 6, SEARCH_DLG_HEIGHT,
			N_("&Backwards"), &mcview_search_options.backwards),
	QUICK_CHECKBOX (SEARCH_DLG_WIDTH / 2 + 3, SEARCH_DLG_WIDTH, 5, SEARCH_DLG_HEIGHT,
			N_("case &Sensitive"),  &mcview_search_options.case_sens),
	QUICK_RADIO (3, SEARCH_DLG_WIDTH, 5, SEARCH_DLG_HEIGHT,
			num_of_types, (const char **) list_of_types,
			(int *) &mcview_search_options.type),
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

    if ((qd_result == B_CANCEL) || (exp == NULL) || (exp[0] == '\0')) {
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

    if (view->search_nroff_seq != NULL)
        mcview_nroff_seq_free (&(view->search_nroff_seq));

    if (view->search != NULL)
        mc_search_free (view->search);

    view->search = mc_search_new (view->last_search_string, -1);
    view->search_nroff_seq = mcview_nroff_seq_new (view);
    if (view->search != NULL) {
	view->search->search_type = mcview_search_options.type;
	view->search->is_all_charsets = mcview_search_options.all_codepages;
	view->search->is_case_sentitive = mcview_search_options.case_sens;
	view->search->whole_words = mcview_search_options.whole_words;
	view->search->search_fn = mcview_search_cmd_callback;
	view->search->update_fn = mcview_search_update_cmd_callback;
    }

    return (view->search != NULL);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_dialog_goto (mcview_t *view, off_t *offset)
{
    typedef enum {
        MC_VIEW_GOTO_LINENUM = 0,
        MC_VIEW_GOTO_PERCENT = 1,
        MC_VIEW_GOTO_OFFSET_DEC = 2,
        MC_VIEW_GOTO_OFFSET_HEX = 3
    } mcview_goto_type_t;

    const char *mc_view_goto_str[] =
    {
	N_("&Line number (decimal)"),
	N_("Pe&rcents"),
	N_("&Decimal offset"),
	N_("He&xadecimal offset")
    };

    const int goto_dlg_height = 12;
    int goto_dlg_width = 40;

    static mcview_goto_type_t current_goto_type = MC_VIEW_GOTO_LINENUM;

    size_t i;

    size_t num_of_types = sizeof (mc_view_goto_str) /sizeof (mc_view_goto_str[0]);
    char *exp = NULL;
    int qd_result;
    gboolean res = FALSE;

    QuickWidget quick_widgets[] =
    {
	QUICK_BUTTON (6, 10, goto_dlg_height - 3, goto_dlg_height, N_("&Cancel"), B_CANCEL, NULL),
	QUICK_BUTTON (2, 10, goto_dlg_height - 3, goto_dlg_height, N_("&OK"), B_ENTER, NULL),
	QUICK_RADIO (3, goto_dlg_width, 4, goto_dlg_height,
			num_of_types, (const char **) mc_view_goto_str, (int *) &current_goto_type),
	QUICK_INPUT (3, goto_dlg_width, 2, goto_dlg_height,
			INPUT_LAST_TEXT, goto_dlg_width - 6, 0, MC_HISTORY_VIEW_GOTO, &exp),
	QUICK_END
    };

    QuickDialog Quick_input =
    {
	goto_dlg_width, goto_dlg_height, -1, -1,
	N_("Goto"), "[Input Line Keys]",
	quick_widgets, FALSE
    };

#ifdef ENABLE_NLS
    for (i = 0; i < num_of_types; i++)
	mc_view_goto_str [i] = _(mc_view_goto_str [i]);

    quick_widgets[0].u.button.text = _(quick_widgets[0].u.button.text);
    quick_widgets[1].u.button.text = _(quick_widgets[1].u.button.text);
#endif

    /* calculate widget coordinates */
    {
	int b0_len, b1_len, len;
	const int button_gap = 2;

	/* preliminary dialog width */
	goto_dlg_width = max (goto_dlg_width, str_term_width1 (Quick_input.title) + 4);

	/* length of radiobuttons */
	for (i = 0; i < num_of_types; i++)
	    goto_dlg_width = max (goto_dlg_width, str_term_width1 (mc_view_goto_str [i]) + 10);

	/* length of buttons */
	b0_len = str_term_width1 (quick_widgets[0].u.button.text) + 3;
	b1_len = str_term_width1 (quick_widgets[1].u.button.text) + 5; /* default button */
	len = b0_len + b1_len + button_gap * 2;

	/* dialog width */
	Quick_input.xlen = max (goto_dlg_width, len + 6);

	/* correct widget coordinates */
	for (i = sizeof (quick_widgets)/sizeof (quick_widgets[0]); i > 0; i--)
	    quick_widgets[i - 1].x_divisions = Quick_input.xlen;

	/* input length */
	quick_widgets[3].u.input.len = Quick_input.xlen - 6;

	/* button positions */
        quick_widgets[1].relative_x = Quick_input.xlen/2 - len/2;
        quick_widgets[0].relative_x = quick_widgets[1].relative_x + b1_len + button_gap;
    }

    /* run dialog*/
    qd_result = quick_dialog (&Quick_input);

    *offset = -1;

    /* check input line value */
    if ((qd_result != B_CANCEL) && (exp != NULL) && (exp[0] != '\0')) {
	int base = (current_goto_type == MC_VIEW_GOTO_OFFSET_HEX) ? 16 : 10;
	off_t addr;
	char *error;

	res = TRUE;

	addr = strtoll (exp, &error, base);
	if ((*error == '\0') && (addr >= 0)) {
	    switch (current_goto_type) {
	    case MC_VIEW_GOTO_LINENUM:
		mcview_coord_to_offset (view, offset, addr, 0);
		break;
	    case MC_VIEW_GOTO_PERCENT:
		if (addr > 100)
		    addr = 100;
		*offset = addr * mcview_get_filesize (view) / 100;
		break;
	    case MC_VIEW_GOTO_OFFSET_DEC:
		*offset = addr;
		break;
	    case MC_VIEW_GOTO_OFFSET_HEX:
		*offset = addr;
		break;
	    default:
		break;
	    }
	    *offset = mcview_bol (view, *offset);
	}
    }

    g_free (exp);
    return res;
}
