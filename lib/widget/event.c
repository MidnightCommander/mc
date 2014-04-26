/*
   Editor's events definitions.

   Copyright (C) 2012-2014
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2014

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
#include "lib/event.h"

#include "event.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mc_widget_init_events (GError ** error)
{
    /* *INDENT-OFF* */
    event_init_group_t dialog_group_events[] =
    {
        {"show_dialog_list", mc_widget_dialog_show_dialog_list, NULL},

        {NULL, NULL, NULL}
    };
    /* *INDENT-ON* */

    /* *INDENT-OFF* */
    event_init_group_t input_group_events[] =
    {
        {"show_history", mc_widget_input_show_history, NULL},

        {NULL, NULL, NULL}
    };
    /* *INDENT-ON* */


    /* *INDENT-OFF* */
    event_init_t standard_events[] =
    {
        {MCEVENT_GROUP_WIDGET_DIALOG, dialog_group_events},
        {MCEVENT_GROUP_WIDGET_INPUT, input_group_events},
        {NULL, NULL}
    };
    /* *INDENT-ON* */

    mc_event_mass_add (standard_events, error);

}

/* --------------------------------------------------------------------------------------------- */
