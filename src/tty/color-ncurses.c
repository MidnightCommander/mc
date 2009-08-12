/* Color setup for NCurses screen library
   Copyright (C) 1994, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2007, 2008, 2009 Free Software Foundation, Inc.
   
   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2009.
   
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

/** \file color-ncurses.c
 *  \brief Source: NCUrses-specific color setup
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>		/* size_t*/

#include "../../src/global.h"

#include "../../src/tty/tty-ncurses.h"
#include "../../src/tty/color.h"		/* variables */
#include "../../src/tty/color-internal.h"

int attr_pairs [MAX_PAIRS];

void
tty_init_colors (gboolean disable, gboolean force)
{
    (void) force;

    if (has_colors () && !disable) {
	const size_t map_len = color_map_len ();
        size_t i;

	use_colors = TRUE;

	start_color ();
	use_default_colors ();

	configure_colors ();

	if (map_len > MAX_PAIRS) {
	    /* This message should only be seen by the developers */
	    fprintf (stderr,
		     "Too many defined colors, resize MAX_PAIRS on color.c");
	    exit (1);
	}

	/* Use default terminal colors */
	mc_init_pair (DEFAULT_COLOR_INDEX, -1, -1);

	for (i = 0; i < map_len; i++)
	    if (color_map [i].name != NULL) {
		mc_init_pair (i + 1, color_map_fg (i), color_map_bg (i));
		/*
		 * ncurses doesn't remember bold attribute in the color pairs,
		 * so we should keep track of it in a separate array.
		 */
		attr_pairs [i + 1] = color_map [i].fg & A_BOLD;
	    }
    }
}

/* Functions necessary to implement syntax highlighting  */
void
mc_init_pair (int index, CTYPE foreground, CTYPE background)
{
    init_pair (index, foreground, background == 0 ? -1 : background);
    if (index > max_index)
	max_index = index;
}

int
tty_try_alloc_color_pair (const char *fg, const char *bg)
{
    int fg_index, bg_index;
    int bold_attr;
    struct colors_avail *p = &c;

    c.index = EDITOR_NORMAL_COLOR_INDEX;
    for (;;) {
	if (((fg && p->fg) ? !strcmp (fg, p->fg) : fg == p->fg) != 0
	    && ((bg && p->bg) ? !strcmp (bg, p->bg) : bg == p->bg) != 0)
	    return p->index;
	if (!p->next)
	    break;
	p = p->next;
    }
    p->next = g_new (struct colors_avail, 1);
    p = p->next;
    p->next = 0;
    p->fg = fg ? g_strdup (fg) : 0;
    p->bg = bg ? g_strdup (bg) : 0;
    if (!fg)
        /* Index in color_map array = COLOR_INDEX - 1 */
	fg_index = color_map[EDITOR_NORMAL_COLOR_INDEX - 1].fg;
    else
	get_color (fg, &fg_index);

    if (!bg)
	bg_index = color_map[EDITOR_NORMAL_COLOR_INDEX - 1].bg;
    else
	get_color (bg, &bg_index);

    bold_attr = fg_index & A_BOLD;
    fg_index = fg_index & COLOR_WHITE;
    bg_index = bg_index & COLOR_WHITE;

    p->index = alloc_color_pair (fg_index, bg_index);
    attr_pairs [p->index] = bold_attr;
    return p->index;
}

void
tty_setcolor (int color)
{
    attrset (color);
}

void
tty_lowlevel_setcolor (int color)
{
    attrset (MY_COLOR_PAIR (color));
}

void
tty_set_normal_attrs (void)
{
    standend ();
}
