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
mc_tty_color_init_lib (gboolean disable, gboolean force)
{
    (void) force;

    if (has_colors () && !disable) {

	use_colors = TRUE;

	start_color ();
	use_default_colors ();

    }
}

void
mc_tty_color_try_alloc_pair_lib (mc_color_pair_t *mc_color_pair)
{
    init_pair (mc_color_pair->pair_index,
	    mc_color_pair->ifg,
	    mc_color_pair->ibg == 0 ? -1 : mc_color_pair->ibg);
}

void
mc_tty_color_set_lib (int color)
{
    attrset (color);
}

void
mc_tty_color_lowlevel_set_lib (int color)
{
    attrset (MY_COLOR_PAIR (color));
}

void
mc_tty_color_set_normal_attrs_lib (void)
{
    standend ();
}
