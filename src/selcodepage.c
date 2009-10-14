/* User interface for charset selection.

   Copyright (C) 2001 Walery Studennikov <despair@sama.ru>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file selcodepage.c
 *  \brief Source: user %interface for charset %selection
 */

#include <config.h>

#ifdef HAVE_CHARSET

#include <stdio.h>
#include <stdlib.h>

#include "global.h"
#include "dialog.h"
#include "widget.h"
#include "wtools.h"
#include "charsets.h"
#include "selcodepage.h"
#include "main.h"

#define ENTRY_LEN 30

/* Numbers of (file I/O) and (input/display) codepages. -1 if not selected */
int source_codepage = -1;
int display_codepage = -1;

static unsigned char
get_hotkey (int n)
{
    return (n <= 9) ? '0' + n : 'a' + n - 10;
}

/* Return value:
 *   -2 (SELECT_CHARSET_CANCEL)       : Cancel
 *   -1 (SELECT_CHARSET_OTHER_8BIT)   : "Other 8 bit"    if seldisplay == TRUE
 *   -1 (SELECT_CHARSET_NO_TRANSLATE) : "No translation" if seldisplay == FALSE
 *   >= 0                             : charset number
 */
int
select_charset (int center_y, int center_x, int current_charset, gboolean seldisplay)
{
    int i;
    char buffer[255];

    /* Create listbox */
    Listbox *listbox = create_listbox_window_centered (center_y, center_x,
					      n_codepages + 1, ENTRY_LEN + 2,
					      _("Choose codepage"),
					      "[Codepages Translation]");

    if (!seldisplay)
	LISTBOX_APPEND_TEXT (listbox, '-', _("-  < No translation >"),
			     NULL);

    /* insert all the items found */
    for (i = 0; i < n_codepages; i++) {
	char *name = codepages[i].name;
	g_snprintf (buffer, sizeof (buffer), "%c  %s", get_hotkey (i),
		    name);
	LISTBOX_APPEND_TEXT (listbox, get_hotkey (i), buffer, NULL);
    }
    if (seldisplay) {
	g_snprintf (buffer, sizeof (buffer), "%c  %s",
		    get_hotkey (n_codepages), _("Other 8 bit"));
	LISTBOX_APPEND_TEXT (listbox, get_hotkey (n_codepages), buffer,
			     NULL);
    }

    /* Select the default entry */
    i = (seldisplay)
	? ((current_charset < 0) ? n_codepages : current_charset)
	: (current_charset + 1);

    listbox_select_by_number (listbox->list, i);

    i = run_listbox (listbox);

    if (i < 0) {
	/* Cancel dialog */
	return SELECT_CHARSET_CANCEL;
    } else {
	/* some charset has been selected */
	if (seldisplay) {
	    /* charset list is finished with "Other 8 bit" item */
	    return (i >= n_codepages) ? SELECT_CHARSET_OTHER_8BIT : i;
	} else {
	    /* charset list is began with "-  < No translation >" item */
	    return (i - 1);
	}
    }
}

gboolean
do_select_codepage (void)
{
    const char *errmsg = NULL;
    int r;

    r = select_charset (-1, -1, source_codepage, FALSE);
    if (r == SELECT_CHARSET_CANCEL)
	return FALSE;

    source_codepage = r;

    errmsg = init_translation_table (r == SELECT_CHARSET_NO_TRANSLATE ?
					display_codepage : source_codepage,
					display_codepage);
    if (errmsg != NULL)
        message (D_ERROR, MSG_ERROR, "%s", errmsg);

    return (errmsg == NULL);
}

#endif				/* HAVE_CHARSET */
