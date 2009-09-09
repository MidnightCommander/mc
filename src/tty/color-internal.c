/*  Internal stuff of color setup
   Copyright (C) 1994, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2007, 2008, 2009 Free Software Foundation, Inc.
   
   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2009.
   Slava Zanko <slavazanko@gmail.com>, 2009.
   
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

/** \file color-internal.c
 *  \brief Source: Internal stuff of color setup
 */

#include <config.h>

#include <sys/types.h>				/* size_t */

#include "../../src/tty/color.h"		/* colors and attributes */
#include "../../src/tty/color-internal.h"

/*** global variables ****************************************************************************/

gboolean mc_tty_color_disable;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

typedef struct mc_tty_color_table_struct {
    const char *name;
    int  value;
} mc_tty_color_table_t;

/*** file scope variables ************************************************************************/

mc_tty_color_table_t const color_table [] = {
    { "black",         COLOR_BLACK   },
    { "gray",          COLOR_BLACK   | A_BOLD },
    { "red",           COLOR_RED     },
    { "brightred",     COLOR_RED     | A_BOLD },
    { "green",         COLOR_GREEN   },
    { "brightgreen",   COLOR_GREEN   | A_BOLD },
    { "brown",         COLOR_YELLOW  },
    { "yellow",        COLOR_YELLOW  | A_BOLD },
    { "blue",          COLOR_BLUE    },
    { "brightblue",    COLOR_BLUE    | A_BOLD },
    { "magenta",       COLOR_MAGENTA },
    { "brightmagenta", COLOR_MAGENTA | A_BOLD },
    { "cyan",          COLOR_CYAN    },
    { "brightcyan",    COLOR_CYAN    | A_BOLD },
    { "lightgray",     COLOR_WHITE },
    { "white",         COLOR_WHITE   | A_BOLD },
    { "default",       0 }, /* default color of the terminal */
    /* special colors */
    { "A_REVERSE",      SPEC_A_REVERSE },
    { "A_BOLD",         SPEC_A_BOLD},
    { "A_BOLD_REVERSE", SPEC_A_BOLD_REVERSE },
    { "A_UNDERLINE",    SPEC_A_UNDERLINE },
    /* End of list */
    { NULL, 0}
};

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

const char *
mc_tty_color_get_valid_name(const char *color_name)
{
    size_t i;

    if (color_name == NULL)
	return NULL;

    for (i=0; color_table [i].name != NULL; i++)
	if (strcmp (color_name, color_table [i].name) == 0) {
	    return color_table [i].name;
	    break;
	}
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

int
mc_tty_color_get_index_by_name(const char *color_name)
{
    size_t i;

    if (color_name == NULL)
	return -1;

    for (i=0; color_table [i].name != NULL; i++)
	if (strcmp (color_name, color_table [i].name) == 0) {
	    return color_table [i].value;
	    break;
	}
    return -1;
}

/* --------------------------------------------------------------------------------------------- */
