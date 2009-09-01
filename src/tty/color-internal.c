/*  Internal stuff of color setup
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

/** \file color-internal.c
 *  \brief Source: Internal stuff of color setup
 */

#include <config.h>

#include <sys/types.h>				/* size_t */

#include "../../src/tty/color.h"		/* colors and attributes */
#include "../../src/tty/color-internal.h"

struct color_table_s const color_table [] = {
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
    { "default",       0 } /* default color of the terminal */
};

struct colorpair color_map [] = {
    { "normal=",     0, 0 },	/* normal */               /*  1 */
    { "selected=",   0, 0 },	/* selected */
    { "marked=",     0, 0 },	/* marked */
    { "markselect=", 0, 0 },	/* marked/selected */
    { "errors=",     0, 0 },	/* errors */
    { "menu=",       0, 0 },	/* menu entry */
    { "reverse=",    0, 0 },	/* reverse */

    /* Dialog colors */
    { "dnormal=",    0, 0 },	/* Dialog normal */        /*  8 */
    { "dfocus=",     0, 0 },	/* Dialog focused */
    { "dhotnormal=", 0, 0 },	/* Dialog normal/hot */
    { "dhotfocus=",  0, 0 },	/* Dialog focused/hot */

    { "viewunderline=", 0, 0 },	/* _\b? sequence in view, underline in editor */
    { "menusel=",    0, 0 },	/* Menu selected color */  /* 13 */
    { "menuhot=",    0, 0 },	/* Color for menu hotkeys */
    { "menuhotsel=", 0, 0 },	/* Menu hotkeys/selected entry */

    { "helpnormal=", 0, 0 },	/* Help normal */          /* 16 */
    { "helpitalic=", 0, 0 },	/* Italic in help */
    { "helpbold=",   0, 0 },	/* Bold in help */
    { "helplink=",   0, 0 },	/* Not selected hyperlink */
    { "helpslink=",  0, 0 },	/* Selected hyperlink */

    { "gauge=",      0, 0 },	/* Color of the progress bar (percentage) *//* 21 */
    { "input=",      0, 0 },

    { 0,             0, 0 },	/* not usable (DEFAULT_COLOR_INDEX) *//* 23 */
    { 0,             0, 0 },	/* unused */
    { 0,             0, 0 },	/* not usable (A_REVERSE) */
    { 0,             0, 0 },	/* not usable (A_REVERSE_BOLD) */

    /* editor colors start at 27 */
    { "editnormal=",     0, 0 },	/* normal */       /* 27 */
    { "editbold=",       0, 0 },	/* search->found */
    { "editmarked=",     0, 0 },	/* marked/selected */
    { "editwhitespace=", 0, 0 },	/* whitespace */
    { "editlinestate=",  0, 0 },	/* line number bar*/

    /* error dialog colors start at 32 */
    { "errdhotnormal=",  0, 0 },	/* Error dialog normal/hot */ /* 32 */
    { "errdhotfocus=",   0, 0 },	/* Error dialog focused/hot */
};

size_t
color_table_len (void)
{
    return sizeof (color_table)/sizeof(color_table [0]);
}

size_t
color_map_len (void)
{
    return sizeof (color_map)/sizeof(color_map [0]);
}
