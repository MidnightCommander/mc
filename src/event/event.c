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

#include "event.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

GHashTable *mcevent_hashlist = NULL;
GMainLoop *mcevent_mainLoop = NULL;
GMainContext *mcevent_mainContext = NULL;


/*** file scope functions ************************************************************************/

/* --------------------------------------------------------------------------------------------- */

static void
mcevent_hash_destroy_key (gpointer data)
{
}

/* --------------------------------------------------------------------------------------------- */

static void
mcevent_hash_destroy_value (gpointer data)
{
    mcevent_t *event = (mcevent_t *) data;
    g_free(event->name);
    g_free (event);
}

/* --------------------------------------------------------------------------------------------- */


/*** public functions ****************************************************************************/

/* --------------------------------------------------------------------------------------------- */

gboolean mcevent_init(void)
{
    mcevent_hashlist =
	g_hash_table_new_full (g_str_hash, g_str_equal,
			       mcevent_hash_destroy_key,
			       mcevent_hash_destroy_value);
    if (mcevent_hashlist == NULL)
	return FALSE;

    mcevent_mainLoop = g_main_loop_new (NULL, FALSE);
    if (mcevent_mainLoop == NULL)
    {
	g_hash_table_destroy (mcevent_hashlist);
	return FALSE;
    }
    mcevent_mainContext =  g_main_context_default ();
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void mcevent_deinit(void)
{
    if (mcevent_hashlist == NULL)
	return;

    g_hash_table_destroy (mcevent_hashlist);
    g_main_loop_unref (mcevent_mainLoop);
    mcevent_hashlist = NULL;
    mcevent_mainLoop = NULL;
    mcevent_mainContext = NULL;
}

/* --------------------------------------------------------------------------------------------- */

void mcevent_run(void)
{
/* start GMainLoop */
    g_main_loop_run (mcevent_mainLoop);
}

/* --------------------------------------------------------------------------------------------- */

void mcevent_stop(void)
{
/* stop GMainLoop */
    g_main_loop_quit (mcevent_mainLoop);
}

/* --------------------------------------------------------------------------------------------- */

GList *
mcevent_get_list (void)
{
    if (mcevent_hashlist == NULL)
	return NULL;

/*    return g_hash_table_get_keys (mcevent_hashlist);*/
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

mcevent_t *
mcevent_get_handler(const gchar *event_name)
{
    if (event_name == NULL || mcevent_hashlist == NULL)
	return NULL;

    return (mcevent_t *) g_hash_table_lookup (mcevent_hashlist, (gpointer) event_name);
}

/* --------------------------------------------------------------------------------------------- */

gboolean mcevent_raise(const gchar *event_name, gpointer event_data)
{
     mcevent_t *event = mcevent_get_handler(event_name);

     if (event == NULL)
        return FALSE;

    return (*event->cb) (event, event_data);
}

/* --------------------------------------------------------------------------------------------- */

