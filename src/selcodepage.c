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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>

#ifdef HAVE_CHARSET
#include <stdlib.h>
#include <stdio.h>

#include "global.h"
#include "dialog.h"
#include "widget.h"
#include "wtools.h"
#include "charsets.h"
#include "selcodepage.h"
#include "main.h"

#define ENTRY_LEN 35

/* Numbers of (file I/O) and (input/display) codepages. -1 if not selected */
int source_codepage = -1;
int display_codepage = -1;

static unsigned char
get_hotkey (int n)
{
    return (n <= 9) ? '0' + n : 'a' + n - 10;
}

int
select_charset (int current_charset, int seldisplay)
{
    int i, menu_lines = n_codepages + 1;
    char buffer[255];

    /* Create listbox */
    Listbox *listbox = create_listbox_window (ENTRY_LEN + 2, menu_lines,
					      _(" Choose input codepage "),
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

    return (seldisplay) ? ((i >= n_codepages) ? -1 : i)
	: (i - 1);
}

/* Helper functions for codepages support */


int
do_select_codepage (void)
{
    char *errmsg;

    if (display_codepage > 0) {
	source_codepage = select_charset (source_codepage, 0);
	errmsg =
	    init_translation_table (source_codepage, display_codepage);
	if (errmsg) {
	    message (1, MSG_ERROR, "%s", errmsg);
	    return -1;
	}
    } else {
	message (1, _("Warning"),
		 _("To use this feature select your codepage in\n"
		   "Setup / Display Bits dialog!\n"
		   "Do not forget to save options."));
	return -1;
    }
    return 0;
}

#endif				/* HAVE_CHARSET */
