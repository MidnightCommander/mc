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
    mc_tty_color_disable = disable;

    if (force || (getenv ("COLORTERM") != NULL))
	SLtt_Use_Ansi_Colors = 1;

    if (!mc_tty_color_disable)
    {
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
    return SLtt_Use_Ansi_Colors;
}

static void
mc_tty_color_pair_init_special (tty_color_pair_t *mc_color_pair,
				const char *fg1, const char *bg1,
				const char *fg2, const char *bg2,
				SLtt_Char_Type mask)
{
    if (SLtt_Use_Ansi_Colors != 0)
    {
	if (!mc_tty_color_disable)
	{
	    SLtt_set_color (mc_color_pair->pair_index, (char *) "", (char *) fg1, (char *) bg1);
	}
	else
	{
	    SLtt_set_color (mc_color_pair->pair_index, (char *) "", (char *) fg2, (char *) bg2);
	}
    }
    else
    {
	SLtt_set_mono (mc_color_pair->pair_index, NULL, mask);
    }
}

void
tty_color_init_lib (gboolean disable, gboolean force)
{
    /* FIXME: if S-Lang is used, has_colors() must be called regardless
       of whether we are interested in its result */
    if (has_colors (disable, force) && !disable) {
	use_colors = TRUE;
    }
}

void
tty_color_deinit_lib (void)
{
}

void
tty_color_try_alloc_pair_lib (tty_color_pair_t *mc_color_pair)
{
    const char *fg, *bg;
    if (mc_color_pair->ifg <= (int) SPEC_A_REVERSE)
    {
	switch(mc_color_pair->ifg)
	{
	case SPEC_A_REVERSE:
	    mc_tty_color_pair_init_special(
		    mc_color_pair,
		    "black", "white",
		    "black", "lightgray",
		    SLTT_REV_MASK
	    );
	break;
	case SPEC_A_BOLD:
	    mc_tty_color_pair_init_special(
		    mc_color_pair,
		    "white", "black",
		    "white", "black",
		    SLTT_BOLD_MASK
	    );
	break;
	case SPEC_A_BOLD_REVERSE:

	    mc_tty_color_pair_init_special(
		    mc_color_pair,
		    "white", "white",
		    "white", "white",
		    SLTT_BOLD_MASK | SLTT_REV_MASK
	    );
	break;
	case SPEC_A_UNDERLINE:
	    mc_tty_color_pair_init_special(
		    mc_color_pair,
		    "white", "black",
		    "white", "black",
		    SLTT_ULINE_MASK
	    );
	break;
	}
    }
    else
    {
	fg = (mc_color_pair->cfg) ? mc_color_pair->cfg : "default";
	bg = (mc_color_pair->cbg) ? mc_color_pair->cbg : "default";
	SLtt_set_color (mc_color_pair->pair_index, (char *) "", (char *) fg, (char *) bg);
    }
}

void
tty_setcolor (int color)
{
    if (!SLtt_Use_Ansi_Colors)
	SLsmg_set_color (color);
    else if ((color & A_BOLD) != 0) {
	if (color == A_BOLD)
	    SLsmg_set_color (A_BOLD);
	else
	    SLsmg_set_color ((color & (~A_BOLD)) + 8);
    } else if (color == A_REVERSE)
	SLsmg_set_color (A_REVERSE);
    else
	SLsmg_set_color (color);
}

/* Set colorpair by index, don't interpret S-Lang "emulated attributes" */
void
tty_lowlevel_setcolor (int color)
{
    SLsmg_set_color (color & 0x7F);
}

void
tty_set_normal_attrs (void)
{
    SLsmg_normal_video ();
}
