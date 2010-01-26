/* User interface for syntax selection.

   Copyright (C) 2005, 2006 Leonard den Ottolander <leonard den ottolander nl>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 *  \brief Source: user %interface for syntax %selection
 *  \author Leonard den Ottolander
 *  \date 2005, 2006
 */

#include <config.h>

#include <stdlib.h>

#include "lib/global.h"
#include "src/wtools.h"

#include "edit-impl.h"

#define MAX_ENTRY_LEN 40
#define LIST_LINES 14
#define N_DFLT_ENTRIES 2

static int
pstrcmp(const void *p1, const void *p2)
{
    return strcmp(*(char**)p1, *(char**)p2);
}

static int
exec_edit_syntax_dialog (const char **names) {
    int i;

    Listbox *syntaxlist = create_listbox_window (LIST_LINES, MAX_ENTRY_LEN,
	_(" Choose syntax highlighting "), NULL);
    LISTBOX_APPEND_TEXT (syntaxlist, 'A', _("< Auto >"), NULL);
    LISTBOX_APPEND_TEXT (syntaxlist, 'R', _("< Reload Current Syntax >"), NULL);

    for (i = 0; names[i]; i++) {
	LISTBOX_APPEND_TEXT (syntaxlist, 0, names[i], NULL);
	if (! option_auto_syntax && option_syntax_type &&
	    (strcmp (names[i], option_syntax_type) == 0))
    	    listbox_select_by_number (syntaxlist->list, i + N_DFLT_ENTRIES);
    }

    return run_listbox (syntaxlist);
}

void
edit_syntax_dialog (void) {
    char *old_syntax_type;
    int old_auto_syntax, syntax;
    char **names;
    int i;
    int force_reload = 0;
    int count = 0;

    names = (char**) g_malloc (sizeof (char*));
    names[0] = NULL;
    /* We fill the list of syntax files every time the editor is invoked.
       Instead we could save the list to a file and update it once the syntax
       file gets updated (either by testing or by explicit user command). */
    edit_load_syntax (NULL, &names, NULL);
    while (names[count++] != NULL);
    qsort(names, count - 1, sizeof(char*), pstrcmp);

    if ((syntax = exec_edit_syntax_dialog ((const char**) names)) < 0) {
	for (i = 0; names[i]; i++) {
	    g_free (names[i]);
	}
	g_free (names);
	return;
    }

    old_auto_syntax = option_auto_syntax;
    old_syntax_type = g_strdup (option_syntax_type);

    switch (syntax) {
	case 0: /* auto syntax */
	    option_auto_syntax = 1;
	    break;
	case 1: /* reload current syntax */
	    force_reload = 1;
	    break;
	default:
	    option_auto_syntax = 0;
	    g_free (option_syntax_type);
	    option_syntax_type = g_strdup (names[syntax - N_DFLT_ENTRIES]);
    }

    /* Load or unload syntax rules if the option has changed */
    if ((option_auto_syntax && !old_auto_syntax) || old_auto_syntax ||
	(old_syntax_type && option_syntax_type &&
	(strcmp (old_syntax_type, option_syntax_type) != 0)) ||
	force_reload)
	edit_load_syntax (wedit, NULL, option_syntax_type);

    for (i = 0; names[i]; i++) {
	g_free (names[i]);
    }
    g_free (names);
    g_free (old_syntax_type);
}
