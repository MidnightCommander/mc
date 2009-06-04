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

extern char *command_line_colors;

/* Set if we are actually using colors */
gboolean use_colors = FALSE;

struct colors_avail c;
int max_index = 0;

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
configure_colors_string (const char *the_color_string)
{
    const size_t map_len = color_map_len ();

    size_t i;
    char **color_strings, **p;

    if (!the_color_string)
	return;

    color_strings = g_strsplit (the_color_string, ":", -1);

    p = color_strings;

    while ((p != NULL) && (*p != NULL)) {
	char **cfb; /* color, fore, back*/
	/* cfb[0] - entry name
	 * cfb[1] - fore color
	 * cfb[20 - back color
	 */
	char *e;

	cfb = g_strsplit_set (*p, "=,", 3);
	/* append '=' to the entry name */
	e = g_strdup_printf ("%s=", cfb[0]);
	g_free (cfb[0]);
	cfb[0] = e;

	for (i = 0; i < map_len; i++)
	    if (color_map [i].name != NULL) {
		size_t klen = strlen (color_map [i].name);

		if (strncmp (cfb[0], color_map [i].name, klen) == 0) {
		    if ((cfb[1] != NULL) && (*cfb[1] != '\0'))
			get_color (cfb[1], &color_map [i].fg);
		    if ((cfb[2] != NULL) && (*cfb[2] != '\0'))
			get_color (cfb[2], &color_map [i].bg);
		    break;
		}
	    }

	g_strfreev (cfb);
	p++;
    }

   g_strfreev (color_strings);
}

void
configure_colors (void)
{
    configure_colors_string (default_colors);
    configure_colors_string (setup_color_string);
    configure_colors_string (term_color_string);
    configure_colors_string (getenv ("MC_COLOR_TABLE"));
    configure_colors_string (command_line_colors);
}

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

    for (p = c.next; p != NULL; p = next) {
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
