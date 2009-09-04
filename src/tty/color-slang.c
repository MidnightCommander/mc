/* Color setup for S_Lang screen library
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

#include "../../src/setup.h"	/* color_terminal_string */

static int
has_colors (gboolean disable, gboolean force)
{
    if (force || (getenv ("COLORTERM") != NULL))
	SLtt_Use_Ansi_Colors = 1;

#if 0
    /* We want to allow overwriding */
    if (!disable) {
	const char *terminal = getenv ("TERM");
	const size_t len = strlen (terminal);

	char *cts = color_terminal_string;
	char *s;
	size_t i;

	/* check color_terminal_string */
	while (*cts != '\0') {
	    while (*cts == ' ' || *cts == '\t')
		cts++;
	    s = cts;
	    i = 0;

	    while (*cts != '\0' && *cts != ',') {
		cts++;
		i++;
	    }

	    if ((i != 0) && (i == len) && (strncmp (s, terminal, i) == 0))
		SLtt_Use_Ansi_Colors = 1;

	    if (*cts == ',')
	        cts++;
	}
    }
    /* Setup emulated colors */
    if (SLtt_Use_Ansi_Colors != 0) {
        if (!disable) {
	    mc_init_pair (A_REVERSE, "black", "white");
	    mc_init_pair (A_BOLD, "white", "black");
	} else {
	    mc_init_pair (A_REVERSE, "black", "lightgray");
	    mc_init_pair (A_BOLD, "white", "black");
	    mc_init_pair (A_BOLD_REVERSE, "white", "lightgray");
	}
    } else {
	SLtt_set_mono (A_BOLD,    NULL, SLTT_BOLD_MASK);
	SLtt_set_mono (A_REVERSE, NULL, SLTT_REV_MASK);
	SLtt_set_mono (A_BOLD | A_REVERSE, NULL, SLTT_BOLD_MASK | SLTT_REV_MASK);
    }
#endif
    return SLtt_Use_Ansi_Colors;
}

void
mc_tty_color_init_lib (gboolean disable, gboolean force)
{
    /* FIXME: if S-Lang is used, has_colors() must be called regardless
       of whether we are interested in its result */
    if (has_colors (disable, force) && !disable) {
	use_colors = TRUE;
    }
}

void
mc_tty_color_try_alloc_pair_lib (mc_color_pair_t *mc_color_pair)
{
    SLtt_set_color (mc_color_pair->pair_index, (char *) "", (char *) mc_color_pair->cfg, (char *) mc_color_pair->cbg);
}

void
mc_tty_color_set_lib (int color)
{
    if (!SLtt_Use_Ansi_Colors)
	SLsmg_set_color (color);
    else if ((color & A_BOLD) != 0) {
	if (color == A_BOLD)
	    SLsmg_set_color (A_BOLD);
	else
	    SLsmg_set_color ((color & (~A_BOLD)) + 8);
    } else
	SLsmg_set_color (color);
}

/* Set colorpair by index, don't interpret S-Lang "emulated attributes" */
void
mc_tty_color_lowlevel_set_lib (int color)
{
    SLsmg_set_color (color & 0x7F);
}

void
mc_tty_color_set_normal_attrs_lib (void)
{
    SLsmg_normal_video ();
}
