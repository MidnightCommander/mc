/* Dialog managing.
   Copyright (C) 1994 Miguel de Icaza.
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <config.h>
#include <stdio.h>
#include "global.h"
#include "dialog.h"
#include "key.h"		/* we_are_background */
#include "main.h"		/* fast_refresh */


/* The refresh stack */
struct Refresh {
    void (*refresh_fn)(void *);
    void *parameter;
    int  flags;
    struct Refresh *next;
};

static struct Refresh *refresh_list;

void
push_refresh (refresh_fn new_refresh, void *parameter, int flags)
{
    struct Refresh *new;

    new = g_new (struct Refresh, 1);
    new->next = (struct Refresh *) refresh_list;
    new->refresh_fn = new_refresh;
    new->parameter = parameter;
    new->flags = flags;
    refresh_list = new;
}

void
pop_refresh (void)
{
    struct Refresh *old;

    if (!refresh_list)
	fprintf (stderr, _("\n\n\nrefresh stack underflow!\n\n\n"));
    else {
	old = refresh_list;
	refresh_list = refresh_list->next;
	g_free (old);
    }
}

static void
do_complete_refresh (struct Refresh *refresh_list)
{
    if (!refresh_list)
	return;

    if (refresh_list->flags != REFRESH_COVERS_ALL)
	do_complete_refresh (refresh_list->next);

    (*(refresh_list->refresh_fn)) (refresh_list->parameter);
}

void
do_refresh (void)
{
    if (we_are_background || !refresh_list)
	return;

    if (fast_refresh)
	(*(refresh_list->refresh_fn)) (refresh_list->parameter);
    else {
	do_complete_refresh (refresh_list);
    }
}
