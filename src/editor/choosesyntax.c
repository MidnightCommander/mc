/*
   User interface for syntax selection.

   Copyright (C) 2011-2017
   Free Software Foundation, Inc.

   Copyright (C) 2005, 2006
   Leonard den Ottolander <leonard den ottolander nl>

   Written by:
   Leonard den Ottolander <leonard den ottolander nl>, 2005, 2006

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

/** \file
 *  \brief Source: user %interface for syntax %selection
 *  \author Leonard den Ottolander
 *  \date 2005, 2006
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>             /* strcmp() */

#include "lib/global.h"
#include "lib/widget.h"         /* Listbox */

#include "edit-impl.h"
#include "editwidget.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define MAX_ENTRY_LEN 40
#define LIST_LINES 14
#define N_DFLT_ENTRIES 2

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static int
pstrcmp (const void *p1, const void *p2)
{
    return strcmp (*(char *const *) p1, *(char *const *) p2);
}

/* --------------------------------------------------------------------------------------------- */

static int
exec_edit_syntax_dialog (const GPtrArray * names, const char *current_syntax)
{
    size_t i;
    Listbox *syntaxlist;

    syntaxlist = create_listbox_window (LIST_LINES, MAX_ENTRY_LEN,
                                        _("Choose syntax highlighting"), NULL);
    LISTBOX_APPEND_TEXT (syntaxlist, 'A', _("< Auto >"), NULL, FALSE);
    LISTBOX_APPEND_TEXT (syntaxlist, 'R', _("< Reload Current Syntax >"), NULL, FALSE);

    for (i = 0; i < names->len; i++)
    {
        const char *name;

        name = g_ptr_array_index (names, i);
        LISTBOX_APPEND_TEXT (syntaxlist, 0, name, NULL, FALSE);
        if (current_syntax != NULL && strcmp (name, current_syntax) == 0)
            listbox_select_entry (syntaxlist->list, i + N_DFLT_ENTRIES);
    }

    return run_listbox (syntaxlist);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
edit_syntax_dialog (WEdit * edit)
{
    GPtrArray *names;
    int syntax;

    names = g_ptr_array_new ();

    /* We fill the list of syntax files every time the editor is invoked.
       Instead we could save the list to a file and update it once the syntax
       file gets updated (either by testing or by explicit user command). */
    edit_load_syntax (NULL, names, NULL);
    g_ptr_array_sort (names, pstrcmp);

    syntax = exec_edit_syntax_dialog (names, edit->syntax_type);
    if (syntax >= 0)
    {
        gboolean force_reload = FALSE;
        char *current_syntax;
        gboolean old_auto_syntax;

        current_syntax = g_strdup (edit->syntax_type);
        old_auto_syntax = option_auto_syntax;

        switch (syntax)
        {
        case 0:                /* auto syntax */
            option_auto_syntax = TRUE;
            break;
        case 1:                /* reload current syntax */
            force_reload = TRUE;
            break;
        default:
            option_auto_syntax = FALSE;
            g_free (edit->syntax_type);
            edit->syntax_type = g_strdup (g_ptr_array_index (names, syntax - N_DFLT_ENTRIES));
        }

        /* Load or unload syntax rules if the option has changed */
        if (force_reload || (option_auto_syntax && !old_auto_syntax) || old_auto_syntax ||
            (current_syntax != NULL && edit->syntax_type != NULL &&
             strcmp (current_syntax, edit->syntax_type) != 0))
            edit_load_syntax (edit, NULL, edit->syntax_type);

        g_free (current_syntax);
    }

    g_ptr_array_foreach (names, (GFunc) g_free, NULL);
    g_ptr_array_free (names, TRUE);
}

/* --------------------------------------------------------------------------------------------- */
