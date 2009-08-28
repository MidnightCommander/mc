/*
   Handle any events in application.
   Interface functions: callbacks handle

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

#include "event.h"
#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

/*** public functions ****************************************************************************/

/* --------------------------------------------------------------------------------------------- */

gboolean
mcevent_add_cb(const gchar *event_name ,  mcevent_callback_func event_callback, gpointer some_callback_data)
{
    mcevent_t *event;

    if (mcevent_hashlist == NULL || event_name == NULL || event_callback == NULL)
	return FALSE;

    event = g_new0(mcevent_t, 1);
    if (event == NULL)
	return FALSE;

    event->name = g_strdup(event_name);
    event->cb = event_callback;
    event->data = some_callback_data;

    if (mcevent_get_handler(event_name) != NULL)
	mcevent_del_cb(event_name);

    g_hash_table_insert (mcevent_hashlist, (gpointer) event_name, (gpointer) event);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcevent_del_cb(const gchar *event_name)
{
    if (mcevent_hashlist == NULL || event_name == NULL)
	return FALSE;

    return g_hash_table_remove (mcevent_hashlist, (gpointer) event_name);
}

/* --------------------------------------------------------------------------------------------- */
