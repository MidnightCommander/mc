/*
   Search functions for diffviewer.

   Copyright (C) 2010-2014
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2010.
   Andrew Borodin <aborodin@vmail.ru>, 2012

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

#include <stdio.h>

#include "lib/global.h"
#include "lib/strutil.h"
#include "lib/tty/key.h"
#include "lib/widget.h"
#include "lib/charsets.h"

#include "src/history.h"
#include "src/selcodepage.h"

#include "internal.h"
#include "charset.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mc_diffviewer_set_codeset (WDiff * dview)
{
    const char *encoding_id = NULL;

    dview->utf8 = TRUE;
    encoding_id =
        get_codepage_id (mc_global.source_codepage >=
                         0 ? mc_global.source_codepage : mc_global.display_codepage);
    if (encoding_id != NULL)
    {
        GIConv conv;

        conv = str_crt_conv_from (encoding_id);
        if (conv != INVALID_CONV)
        {
            if (dview->converter != str_cnv_from_term)
                str_close_conv (dview->converter);
            dview->converter = conv;
        }
        dview->utf8 = (gboolean) str_isutf8 (encoding_id);
    }
}

/* --------------------------------------------------------------------------------------------- */
/* event callback */

gboolean
mc_diffviewer_cmd_select_encoding_show_dialog (const gchar * event_group_name,
                                               const gchar * event_name, gpointer init_data,
                                               gpointer data)
{
    WDiff *dview = (WDiff *) data;

    (void) event_group_name;
    (void) event_name;
    (void) init_data;

    if (do_select_codepage ())
        mc_diffviewer_set_codeset (dview);
    mc_diffviewer_reread (dview);
    tty_touch_screen ();
    repaint_screen ();
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
