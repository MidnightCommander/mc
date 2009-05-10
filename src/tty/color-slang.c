/* Color setup for S_Lang screen library
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

/** \file color-slang.c
 *  \brief Source: S-Lang-specific color setup
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>		/* size_t */

#include "../../src/global.h"

#include "../../src/tty/tty-slang.h"
#include "../../src/tty/color.h"		/* variables */
#include "../../src/tty/color-internal.h"

void
init_colors (void)
{
    /* FIXME: if S-Lang is used, has_colors() must be called regardless
       of whether we are interested in its result */
    if (has_colors () && !disable_colors)
	use_colors = 1;

    if (use_colors) {
	const size_t map_len = color_map_len ();
	size_t i;

	start_color ();
	configure_colors ();

	if (use_colors) {
	    /*
	     * We are relying on undocumented feature of
	     * S-Lang to make COLOR_PAIR(DEFAULT_COLOR_INDEX)
	     * the default fg/bg of the terminal.
	     * Hopefully, future versions of S-Lang will
	     * document this feature.
	     */
	    SLtt_set_color (DEFAULT_COLOR_INDEX, NULL, (char *) "default", (char *) "default");
	}

	for (i = 0; i < map_len; i++)
	    if (color_map [i].name != NULL)
		mc_init_pair (i + 1, color_map_fg(i), color_map_bg(i));
    }

    load_dialog_colors ();
}

/* Functions necessary to implement syntax highlighting  */
void
mc_init_pair (int index, CTYPE foreground, CTYPE background)
{
    if (!background)
	background = "default";

    if (!foreground)
	foreground = "default";

    SLtt_set_color (index, (char *) "", (char *) foreground, (char *) background);
    if (index > max_index)
	max_index = index;
}

int
try_alloc_color_pair (const char *fg, const char *bg)
{
    struct colors_avail *p = &c;

    c.index = EDITOR_NORMAL_COLOR_INDEX;
    for (;;) {
	if (((fg && p->fg) ? (strcmp (fg, p->fg) == 0) : (fg == p->fg)) != 0
	    && ((bg && p->bg) ? (strcmp (bg, p->bg) == 0) : (bg == p->bg)) != 0)
	    return p->index;
	if (p->next == NULL)
	    break;
	p = p->next;
    }
    p->next = g_new (struct colors_avail, 1);
    p = p->next;
    p->next = NULL;
    p->fg = fg ? g_strdup (fg) : NULL;
    p->bg = bg ? g_strdup (bg) : NULL;
    if (fg == NULL
        /* Index in color_map array = COLOR_INDEX - 1 */
	fg = color_map[EDITOR_NORMAL_COLOR_INDEX - 1].fg;
    if (bg == NULL
	bg = color_map[EDITOR_NORMAL_COLOR_INDEX - 1].bg;
    p->index = alloc_color_pair (fg, bg);
    return p->index;
}
