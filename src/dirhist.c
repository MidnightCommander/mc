/* Directory History routines
   
   Copyright (C) 1997 The Free Software Foundation
   
   Written by: 1997 Miguel de Icaza
   
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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

FIXME: Maybe maintain a limit of the number of elements in the history
that should be kept?

   */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "profile.h"
#include "setup.h"
#include "dirhist.h"
#include "global.h"
#include "mad.h"

struct dirhist_entry {
    char *directory;
    struct dirhist_entry *next;
};

/* Here we keep a pointer to all of the directories we have */
static struct dirhist_entry *base;

/* The iterator pointer */
static struct dirhist_entry *iter;

/* the field used for loading/saving data from the profile */
static char *dirhist_name = "Directory history";

void
directory_history_load (void)
{
    char entry_name [BUF_TINY];
    char *value;
    int  i;
    
    for (i = 0; i < DIRECTORY_HISTORY_LOAD_COUNT; i++){
	g_snprintf (entry_name, sizeof (entry_name), "%d", i);
	value = get_profile_string (dirhist_name, entry_name, "", profile_name);
	if (!(value || *value))
	    continue;
	directory_history_add (value);
    }
}

void
directory_history_save (void)
{
    char entry_name [BUF_TINY];
    char *dir;
    int  i;
    
    directory_history_init_iterator ();

    for (i = 0; i < DIRECTORY_HISTORY_LOAD_COUNT; i++){
	dir = directory_history_get_next ();
	if (!dir)
	    break;
	g_snprintf (entry_name, sizeof (entry_name), "%d", i);
	WritePrivateProfileString (dirhist_name, entry_name, dir, profile_name);
    }
}

static void
directory_history_delete (struct dirhist_entry *e)
{
    if (!e)
	return;
    directory_history_delete (e->next);
   g_free (e->directory);
   g_free (e);
}

void
directory_history_free (void)
{
    directory_history_delete (base);
}

void
directory_history_init_iterator (void)
{
    iter = base;
}

char *
directory_history_get_next (void)
{
    struct dirhist_entry *p;
    
    if (!iter)
	return NULL;
    p = iter;
    iter = iter->next;
    return p->directory;
}

void
directory_history_add (char *directory)
{
    struct dirhist_entry *p;

    p = g_new (struct dirhist_entry, 1);
    if (!p)
	return;
    p->directory = g_strdup (directory);
    p->next = base;
    base = p;
}

