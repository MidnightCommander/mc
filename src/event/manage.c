/*
   Handle any events in application.
   Manage events: add, delete, destroy, search

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

static mc_event_callback_t *
mc_event_is_callback_in_array (GSList * callbacks, mc_event_callback_func_t event_callback)
{
    mc_event_callback_t *cb;
    GSList *curr;

    for (curr = callbacks; curr; curr = g_slist_next (curr)) {
        cb = (mc_event_callback_t *) curr->data;

        if (cb->callback == event_callback)
            return cb;
    }
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
mc_event_add (const gchar * event_name, mc_event_callback_func_t event_callback,
              gpointer event_init_data)
{
    mc_event_t *event;
    mc_event_callback_t *cb;

    if (mc_event_hashlist == NULL || event_name == NULL || event_callback == NULL)
        return FALSE;

    event = (mc_event_t *) g_hash_table_lookup (mc_event_hashlist, (gpointer) event_name);
    if (event == NULL) {
        event = g_new0 (mc_event_t, 1);
        event->name = g_strdup (event_name);
        event->callbacks = NULL;
        g_hash_table_insert (mc_event_hashlist, (gpointer) g_strdup (event_name), (gpointer) event);
    }
    cb = mc_event_is_callback_in_array (event->callbacks, event_callback);
    if (cb != NULL) {
        cb->init_data = event_init_data;
        return TRUE;
    }
    cb = g_new0 (mc_event_callback_t, 1);
    cb->init_data = event_init_data;
    cb->callback = event_callback;
    event->callbacks = g_slist_prepend (event->callbacks, (gpointer) cb);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_event_del (const gchar * event_name, mc_event_callback_func_t event_callback)
{
    mc_event_t *event;
    mc_event_callback_t *cb;

    if (mc_event_hashlist == NULL || event_name == NULL || event_callback == NULL)
        return FALSE;

    event = (mc_event_t *) g_hash_table_lookup (mc_event_hashlist, (gpointer) event_name);
    if (event == NULL)
        return FALSE;

    cb = mc_event_is_callback_in_array (event->callbacks, event_callback);
    if (cb == NULL)
        return FALSE;
    event->callbacks = g_slist_remove (event->callbacks, (gpointer) event_callback);
    if (event->callbacks == NULL)
        return g_hash_table_remove (mc_event_hashlist, (gpointer) event_name);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_event_destroy (const gchar * event_name)
{

    if (mc_event_hashlist == NULL || event_name == NULL)
        return FALSE;

    return g_hash_table_remove (mc_event_hashlist, (gpointer) event_name);
}

/* --------------------------------------------------------------------------------------------- */
