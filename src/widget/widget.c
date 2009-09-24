/*
   Widgets library
   Common widgets.

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

#include <string.h>

#include "main.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
w_widget_init (w_widget_t * widget, const char *name)
{
    w_container_init ((w_container_t *) widget);

    widget->top = 0;
    widget->left = 0;
    widget->width = 0;
    widget->height = 0;
    widget->name = (name != NULL) ? g_strdup (name) : NULL;
}

/* --------------------------------------------------------------------------------------------- */

void
w_widget_deinit (w_widget_t * widget)
{
    g_free (widget->name);
    w_container_deinit ((w_container_t *) widget);
}

/* --------------------------------------------------------------------------------------------- */

w_widget_t *
w_widget_find_by_name (w_widget_t * widget, const char *name)
{
    w_container_t *container;
    w_widget_t *ret;

    if (widget == NULL || name == NULL)
        return NULL;

    if (strcmp (widget->name, name) == 0)
        return widget;

    container = (w_container_t *) widget;
    if (container->children == NULL)
        return NULL;

    for (container = container->children; container != NULL; container = container->next) {
        ret = w_widget_find_by_name ((w_widget_t *) container, name);
        if (ret != NULL)
            return ret;
    }
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */
