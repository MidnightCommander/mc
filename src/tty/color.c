/* Color setup
   Copyright (C) 1994, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2007, 2008, 2009 Free Software Foundation, Inc.

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/** \file color.c
 *  \brief Source: color setup
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>		/* size_t */

#include "../../src/global.h"

#include "../../src/tty/tty.h"
#include "../../src/tty/color.h"
#include "../../src/tty/color-internal.h"

#include "../../src/setup.h"		/* setup_color_string, term_color_string */

/* Set if we are actually using colors */
gboolean use_colors = FALSE;

/* Color styles for normal and error dialogs */
int dialog_colors [4];
int alarm_colors [4];

void
get_color (const char *cpp, CTYPE *colp)
{
    const size_t table_len = color_table_len ();
    size_t i;

    for (i = 0; i < table_len; i++)
	if (strcmp (cpp, color_name (i)) == 0) {
	    *colp = color_value (i);
	    break;
	}
}

static void
get_two_colors (char **cpp, struct colorpair *colorpairp)
{
    char *p = *cpp;
    int state;

    state = 0;

    for (; *p; p++) {
	if (*p == ':') {
	    *p = 0;
	    get_color (*cpp, state ? &colorpairp->bg : &colorpairp->fg);
	    *p = ':';
	    *cpp = p + 1;
	    return;
	}

	if (*p == ',') {
	    state = 1;
	    *p = 0;
	    get_color (*cpp, &colorpairp->fg);
	    *p = ',';
	    *cpp = p + 1;
	}
    }

    get_color (*cpp, state ? &colorpairp->bg : &colorpairp->fg);
}

static void
configure_colors_string (const char *the_color_string)
{
    const size_t map_len = color_map_len ();
    char *color_string, *p;
    size_t i;
    gboolean found;

    if (!the_color_string)
	return;

    p = color_string = g_strdup (the_color_string);
    while (color_string && *color_string) {
	while (*color_string == ' ' || *color_string == '\t')
	    color_string++;

	found = FALSE;
	for (i = 0; i < map_len; i++){
	    size_t klen;

	    if (!color_map [i].name)
		continue;
	    klen = strlen (color_map [i].name);

	    if (strncmp (color_string, color_map [i].name, klen) == 0) {
		color_string += klen;
		get_two_colors (&color_string, &color_map [i]);
		found = TRUE;
	    }
	}
	if (!found) {
	    while (*color_string && *color_string != ':')
		color_string++;
	    if (*color_string)
		color_string++;
	}
    }
   g_free (p);
}

void
configure_colors (void)
{
    extern char *command_line_colors;

    configure_colors_string (default_colors);
    configure_colors_string (setup_color_string);
    configure_colors_string (term_color_string);
    configure_colors_string (getenv ("MC_COLOR_TABLE"));
    configure_colors_string (command_line_colors);
}

void
load_dialog_colors (void)
{
    dialog_colors [0] = COLOR_NORMAL;
    dialog_colors [1] = COLOR_FOCUS;
    dialog_colors [2] = COLOR_HOT_NORMAL;
    dialog_colors [3] = COLOR_HOT_FOCUS;

    alarm_colors [0] = ERROR_COLOR;
    alarm_colors [1] = REVERSE_COLOR;
    alarm_colors [2] = ERROR_HOT_NORMAL;
    alarm_colors [3] = ERROR_HOT_FOCUS;
}

/* Functions necessary to implement syntax highlighting  */
struct colors_avail c;
int max_index = 0;

int
alloc_color_pair (CTYPE foreground, CTYPE background)
{
    mc_init_pair (++max_index, foreground, background);
    return max_index;
}

void
tty_colors_done (void)
{
    struct colors_avail *p, *next;

    for (p = c.next; p; p = next) {
	next = p->next;
	g_free (p->fg);
	g_free (p->bg);
	g_free (p);
    }
    c.next = NULL;
}

gboolean
tty_use_colors (void)
{
    return use_colors;
}
