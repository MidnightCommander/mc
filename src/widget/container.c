/*
   Widgets library
   Containers of objects.

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
w_container_init (w_container_t * container)
{
    w_object_init ((w_object_t *) container);

    container->parent = NULL;
    container->children = NULL;
    container->prev = NULL;
    container->next = NULL;
}

/* --------------------------------------------------------------------------------------------- */

void
w_container_deinit (w_container_t * container)
{
    w_container_t *child;
    w_object_deinit ((w_object_t *) container);

    /* TODO: safe removal container from tree */
    w_container_remove_from_siblings (container);

    /* Is this need? */
    for (child = container->children; child != NULL; child = child->next)
        child->parent = container->parent;
}


/* --------------------------------------------------------------------------------------------- */

GPtrArray *
w_container_get_path (w_container_t * container)
{
    GPtrArray *ret;
    w_container_t *parent;

    if (container->parent == NULL)
        return NULL;

    ret = g_ptr_array_new ();
    for (parent = container->parent; parent != NULL; parent = parent->parent) {
        g_ptr_array_add (ret, (gpointer) parent);
    }
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

void
w_container_append_child (w_container_t * container, w_container_t * container_child)
{
    w_container_t *child;

    /* setting up of child */
    container_child->parent = container;
    w_container_remove_from_siblings (container_child);

    if (container->children == NULL) {
        /* pervonah here if no have any children */
        container->children = container_child;
        return;
    }

    /* search last node */
    child = w_container_get_last_child (container);

    /* append new node into end of list */
    container_child->prev = child;
    child->next = container_child;
}

/* --------------------------------------------------------------------------------------------- */

void
w_container_remove_from_siblings (w_container_t * container)
{
    if (container->prev != NULL) {
        container->prev->next = container->next;
        container->prev = NULL;
    }

    if (container->next != NULL) {
        container->next->prev = container->prev;
        container->next = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */

w_container_t *
w_container_get_last_child (w_container_t * container)
{
    w_container_t *child;

    if (container->children == NULL)
        return NULL;

    for (child = container->children; child->next != NULL; child = child->next);
    return child;
}

/* --------------------------------------------------------------------------------------------- */
