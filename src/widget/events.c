/*
   Widgets library
   Event functions for object

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

/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
w_event_set (w_object_t * object, const char *event_name, w_event_cb_t cb)
{
    if (object == NULL || event_name == NULL || cb == NULL)
        return;

    if (w_event_isset (object, event_name))
        g_hash_table_replace (object->events, (gpointer) g_strdup (event_name), (gpointer) cb);
    else
        g_hash_table_insert (object->events, (gpointer) g_strdup (event_name), (gpointer) cb);
}

/* --------------------------------------------------------------------------------------------- */

void
w_event_remove (w_object_t * object, const char *event_name)
{
    if (object == NULL || event_name == NULL)
        return;

    (void) g_hash_table_remove (object->events, (gpointer) event_name);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
w_event_isset (w_object_t * object, const char *event_name)
{
    if (object == NULL || event_name == NULL)
        return FALSE;

    return (g_hash_table_lookup (object->events, (gpointer) event_name) != NULL);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
w_event_raise (w_object_t * object, const char *event_name, gpointer data)
{
    w_event_cb_t cb;

    if (object == NULL || event_name == NULL)
        return FALSE;

    cb = (w_event_cb_t) g_hash_table_lookup (object->events, (gpointer) event_name);
    if (cb == NULL)
        return FALSE;

    return (*cb) (object, event_name, data);

}

/* --------------------------------------------------------------------------------------------- */

void
w_event_raise_to_parents (w_container_t * container, const char *event_name, gpointer data)
{
    GPtrArray *containers_list;
    w_container_t *one_of_parents;
    guint index;

    if (w_event_isset ((w_object_t *) container, event_name) &&
        w_event_raise ((w_object_t *) container, event_name, data)
        )
        return;

    /* getting path of current node from widgets tree */
    containers_list = w_container_get_path (container);
    if (containers_list == NULL)
        return;

    /* now get parent widgets in reverse order */
    for (index = containers_list->len; index != 0; index--) {
        one_of_parents = (w_container_t *) g_ptr_array_index (containers_list, index - 1);
        if (w_event_isset ((w_object_t *) one_of_parents, event_name) &&
            w_event_raise ((w_object_t *) one_of_parents, event_name, data))
            break;
    }
    (void) g_ptr_array_free (containers_list, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
w_event_raise_to_children (w_container_t * container, const char *event_name, gpointer data)
{
    w_container_t *child;

    if (w_event_isset ((w_object_t *) container, event_name) &&
        w_event_raise ((w_object_t *) container, event_name, data)
        )
        return TRUE;

    if (container->children == NULL)
        return FALSE;

    for (child = container->children; child->next != NULL; child = child->next) {
        if (!w_event_raise_to_children (child, event_name, data))
            return TRUE;
    }
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
