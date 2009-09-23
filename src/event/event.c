/*
   Handle any events in application.
   Interface functions: init/deinit; start/stop

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

GHashTable *mc_event_hashlist = NULL;


/*** file scope functions ************************************************************************/

/* --------------------------------------------------------------------------------------------- */

static void
mc_event_hash_destroy_key (gpointer data)
{
    g_free (data);
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_event_hash_destroy_value (gpointer data)
{
    GSList *curr;
    mc_event_t *event = (mc_event_t *) data;
    mc_event_callback_t *cb;

    for (curr = event->callbacks; curr; curr = g_slist_next (curr)) {
        cb = (mc_event_callback_t *) curr->data;
        g_free (cb);
    }
    g_slist_free (event->callbacks);
    g_free (event);
}

/* --------------------------------------------------------------------------------------------- */


/*** public functions ****************************************************************************/

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_event_init (void)
{
    mc_event_hashlist =
        g_hash_table_new_full (g_str_hash, g_str_equal,
                               mc_event_hash_destroy_key, mc_event_hash_destroy_value);
    if (mc_event_hashlist == NULL)
        return FALSE;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
mc_event_deinit (void)
{
    if (mc_event_hashlist == NULL)
        return;

    g_hash_table_destroy (mc_event_hashlist);
    mc_event_hashlist = NULL;
}

/* --------------------------------------------------------------------------------------------- */
