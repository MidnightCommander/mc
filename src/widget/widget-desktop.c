/*
   Widgets library
   Desktop widget - this main widget on entire screen.

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
#include "../src/tty/tty.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

static gboolean
w_desktop_cb_on_resize (w_object_t * object, const char *event_name, gpointer data)
{
    /* Some actions here. May be, repaint background */


    /* retranslate event to all childs */
    (void) w_event_raise_to_children ((w_container_t *) object, event_name, data);
}

/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

w_desktop_t *
w_desktop_new (const char *name)
{
    /* required part */
    w_desktop_t *desktop;

    desktop = g_new0 (w_desktop_t, 1);
    if (desktop == NULL)
        return NULL;

    w_widget_init ((w_widget_t *) desktop, name);
    /* Some other init code  here : */
    desktop->widget.top = 0;
    desktop->widget.left = 0;
    desktop->widget.width = COLS;
    desktop->widget.height = LINES;

    w_event_set ((w_object_t *) desktop, "on_resize", w_desktop_cb_on_resize);

    return desktop;
}

/* --------------------------------------------------------------------------------------------- */

void
w_desktop_free (w_desktop_t * desktop)
{
    /* Some other deinit code  here : */


    /* required part */
    w_widget_deinit ((w_widget_t *) desktop);
    g_free (desktop);
}

/* --------------------------------------------------------------------------------------------- */
