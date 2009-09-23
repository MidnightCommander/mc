/*
   Handle any events in application.
   Raise events.

   Copyright (C) 2009 The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2009.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA.
 */

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
mc_event_raise (const gchar * event_name, gpointer event_data)
{
    GSList *curr;
    mc_event_t *event;
    mc_event_callback_t *cb;

    if (mc_event_hashlist == NULL || event_name == NULL)
        return FALSE;

    event = (mc_event_t *) g_hash_table_lookup (mc_event_hashlist, (gpointer) event_name);
    if (event == NULL)
        return FALSE;

    for (curr = event->callbacks; curr; curr = g_slist_next (curr)) {
        cb = (mc_event_callback_t *) curr->data;
        /* If one of callback will return FALSE, then need to terminate other
           callbacs calls */
        if (!(*cb->callback) (event->name, cb->init_data, event_data))
            return TRUE;
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
