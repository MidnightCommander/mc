/*
   User interface for charset selection.

   Copyright (C) 2001 Walery Studennikov <despair@sama.ru>

   Copyright (C) 2011-2024
   Free Software Foundation, Inc.

   Written by:
   Walery Studennikov <despair@sama.ru>, 2001

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

/** \file selcodepage.c
 *  \brief Source: user %interface for charset %selection
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>

#include "lib/global.h"
#include "lib/widget.h"
#include "lib/charsets.h"

#include "setup.h"

#include "selcodepage.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define ENTRY_LEN 30

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static unsigned char
get_hotkey (int n)
{
    return (n <= 9) ? '0' + n : 'a' + n - 10;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* Return value:
 *   -2 (SELECT_CHARSET_CANCEL)       : Cancel
 *   -1 (SELECT_CHARSET_OTHER_8BIT)   : "Other 8 bit"    if seldisplay == TRUE
 *   -1 (SELECT_CHARSET_NO_TRANSLATE) : "No translation" if seldisplay == FALSE
 *   >= 0                             : charset number
 */
int
select_charset (int center_y, int center_x, int current_charset, gboolean seldisplay)
{
    Listbox *listbox;
    size_t i;
    int listbox_result;
    char buffer[255];

    /* Create listbox */
    listbox =
        listbox_window_centered_new (center_y, center_x, codepages->len + 1, ENTRY_LEN + 2,
                                     _("Choose codepage"), "[Codepages Translation]");

    if (!seldisplay)
        LISTBOX_APPEND_TEXT (listbox, '-', _("-  < No translation >"), NULL, FALSE);

    /* insert all the items found */
    for (i = 0; i < codepages->len; i++)
    {
        const char *name;

        name = ((codepage_desc *) g_ptr_array_index (codepages, i))->name;
        g_snprintf (buffer, sizeof (buffer), "%c  %s", get_hotkey (i), name);
        LISTBOX_APPEND_TEXT (listbox, get_hotkey (i), buffer, NULL, FALSE);
    }

    if (seldisplay)
    {
        unsigned char hotkey;

        hotkey = get_hotkey (codepages->len);
        g_snprintf (buffer, sizeof (buffer), "%c  %s", hotkey, _("Other 8 bit"));
        LISTBOX_APPEND_TEXT (listbox, hotkey, buffer, NULL, FALSE);
    }

    /* Select the default entry */
    i = seldisplay
        ? ((current_charset < 0) ? codepages->len : (size_t) current_charset)
        : ((size_t) current_charset + 1);

    listbox_set_current (listbox->list, i);

    listbox_result = listbox_run (listbox);

    if (listbox_result < 0)
        /* Cancel dialog */
        return SELECT_CHARSET_CANCEL;

    /* some charset has been selected */
    if (seldisplay)
        /* charset list is finished with "Other 8 bit" item */
        return (listbox_result >=
                (int) codepages->len) ? SELECT_CHARSET_OTHER_8BIT : listbox_result;

    /* charset list is began with "-  < No translation >" item */
    return (listbox_result - 1);
}

/* --------------------------------------------------------------------------------------------- */

/** Set codepage */
gboolean
do_set_codepage (int codepage)
{
    char *errmsg;
    gboolean ret;

    mc_global.source_codepage = codepage;
    errmsg = init_translation_table (codepage == SELECT_CHARSET_NO_TRANSLATE ?
                                     mc_global.display_codepage : mc_global.source_codepage,
                                     mc_global.display_codepage);
    ret = errmsg == NULL;

    if (!ret)
    {
        message (D_ERROR, MSG_ERROR, "%s", errmsg);
        g_free (errmsg);
    }

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

/** Show menu selecting codepage */
gboolean
do_select_codepage (void)
{
    int r;

    r = select_charset (-1, -1, default_source_codepage, FALSE);
    if (r == SELECT_CHARSET_CANCEL)
        return FALSE;

    default_source_codepage = r;
    return do_set_codepage (default_source_codepage);
}

/* --------------------------------------------------------------------------------------------- */
