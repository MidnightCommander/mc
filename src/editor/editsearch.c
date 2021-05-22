/*
   Search engine of MCEditor.

   Copyright (C) 2021
   Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2021

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

#include "lib/global.h"
#include "lib/search.h"
#ifdef HAVE_CHARSET
#include "lib/charsets.h"       /* cp_source */
#endif
#include "lib/widget.h"

#include "edit-impl.h"
#include "editwidget.h"

/*** global variables ****************************************************************************/

edit_search_options_t edit_search_options = {
    .type = MC_SEARCH_T_NORMAL,
    .case_sens = FALSE,
    .backwards = FALSE,
    .only_in_selection = FALSE,
    .whole_words = FALSE,
    .all_codepages = FALSE
};

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
edit_search_init (WEdit * edit, const char *str)
{
#ifdef HAVE_CHARSET
    edit->search = mc_search_new (str, cp_source);
#else
    edit->search = mc_search_new (str, NULL);
#endif

    if (edit->search == NULL)
        return FALSE;

    edit->search->search_type = edit_search_options.type;
#ifdef HAVE_CHARSET
    edit->search->is_all_charsets = edit_search_options.all_codepages;
#endif
    edit->search->is_case_sensitive = edit_search_options.case_sens;
    edit->search->whole_words = edit_search_options.whole_words;
    edit->search->search_fn = edit_search_cmd_callback;
    edit->search->update_fn = edit_search_update_callback;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
edit_search_deinit (WEdit * edit)
{
    mc_search_free (edit->search);
    g_free (edit->last_search_string);
}

/* --------------------------------------------------------------------------------------------- */
