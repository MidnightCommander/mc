/* Event callbacks initialization

   Copyright (C)  2011 Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2011.

   This file is part of the Midnight Commander.

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

#include <config.h>

#include "lib/global.h"

#include "lib/event.h"

#include "events_init.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/


/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
events_init (GError ** error)
{
    /* *INDENT-OFF* */
    event_init_t standard_events[] =
    {
        {NULL, NULL, NULL, NULL}
    };
    /* *INDENT-ON* */

    if (!mc_event_init (error))
        return FALSE;

    return mc_event_mass_add (standard_events, error);
}

/* --------------------------------------------------------------------------------------------- */
