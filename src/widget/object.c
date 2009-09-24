/*
   Widgets library
   Low-level objects.

   Copyright (C) 1994, 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003,
   2004, 2005, 2006, 2007, 2009 Free Software Foundation, Inc.

   Written by:
	       2009 Slava Zanko <slavazanko@google.com>

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

#include "main.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
w_object_event_hash_destroy_key (gpointer data)
{
    g_free (data);
}

/* --------------------------------------------------------------------------------------------- */

static void
w_object_event_hash_destroy_value (gpointer data)
{
}


/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
w_object_init (w_object_t * object)
{
    object->events = g_hash_table_new_full (g_str_hash, g_str_equal,
                                            w_object_event_hash_destroy_key,
                                            w_object_event_hash_destroy_value);

}

/* --------------------------------------------------------------------------------------------- */

void
w_object_deinit (w_object_t * object)
{
    g_hash_table_destroy (object->events);
}

/* --------------------------------------------------------------------------------------------- */
