/*
   User interface for charset selection.

   Copyright (C) 2001 Walery Studennikov <despair@sama.ru>

   Copyright (C) 2011-2025
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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
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
 *   -1 (SELECT_CHARSET_NO_TRANSLATE) : "No translation"
 *   >= 0                             : charset number
 */
int
select_charset (const int center_y, const int center_x, const int current_charset)
{
    Listbox *listbox;

    listbox = listbox_window_centered_new (center_y, center_x, codepages->len + 1, ENTRY_LEN + 2,
                                           _ ("Choose codepage"), "[Codepages Translation]");

    LISTBOX_APPEND_TEXT (listbox, '-', _ ("-  < No translation >"), NULL, FALSE);

    for (guint i = 0; i < codepages->len; i++)
    {
        char buffer[BUF_SMALL];
        const char *name = get_codepage_name (i);

        g_snprintf (buffer, sizeof (buffer), "%c  %s", get_hotkey (i), name);
        LISTBOX_APPEND_TEXT (listbox, get_hotkey (i), buffer, NULL, FALSE);
    }

    listbox_set_current (listbox->list, current_charset + 1);

    const int listbox_result = listbox_run (listbox);

    return listbox_result < 0 ? SELECT_CHARSET_CANCEL : listbox_result - 1;
}

/* --------------------------------------------------------------------------------------------- */

/** Set codepage */
gboolean
do_set_codepage (const int codepage)
{
    char *errmsg;
    gboolean ret;

    mc_global.source_codepage = codepage;
    errmsg =
        init_translation_table (codepage == SELECT_CHARSET_NO_TRANSLATE ? mc_global.display_codepage
                                                                        : mc_global.source_codepage,
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
    const int r = select_charset (-1, -1, default_source_codepage);

    if (r == SELECT_CHARSET_CANCEL)
        return FALSE;

    default_source_codepage = r;
    return do_set_codepage (default_source_codepage);
}

/* --------------------------------------------------------------------------------------------- */
