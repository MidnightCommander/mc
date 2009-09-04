/* Color setup.
    Interface functions.

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

/*** global variables ****************************************************************************/

char *command_line_colors = NULL;

/* Set if we are actually using colors */
gboolean use_colors = FALSE;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static GHashTable *mc_tty_color__hashtable = NULL;
static int mc_tty_color__count = 0;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
mc_tty_color_hash_destroy_key (gpointer data)
{
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_tty_color_hash_destroy_value (gpointer data)
{
    mc_color_pair_t *mc_color_pair = (mc_color_pair_t *) data;
    g_free(mc_color_pair);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
tty_init_colors (gboolean disable, gboolean force)
{
    mc_tty_color_init_lib (disable, force);
    mc_tty_color__count = 0;
    mc_tty_color__hashtable = g_hash_table_new_full (g_str_hash, g_str_equal,
			       mc_tty_color_hash_destroy_key,
			       mc_tty_color_hash_destroy_value);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_colors_done (void)
{
    g_hash_table_destroy (mc_tty_color__hashtable);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
tty_use_colors (void)
{
    return use_colors;
}

/* --------------------------------------------------------------------------------------------- */

int tty_try_alloc_color_pair (const char *fg, const char *bg)
{
    gchar *color_pair;
    mc_color_pair_t *mc_color_pair;
    const char *c_fg, *c_bg;

    c_fg = mc_tty_color_get_valid_name(fg);
    c_bg = mc_tty_color_get_valid_name(bg);

    color_pair = g_strdup_printf("%s.%s",fg,bg);
    if (color_pair == NULL)
	return 0;

    mc_color_pair = (mc_color_pair_t *) g_hash_table_lookup (mc_tty_color__hashtable, (gpointer) color_pair);

    if (mc_color_pair != NULL){
	g_free(color_pair);
	return mc_color_pair->pair_index;
    }

    mc_color_pair = g_new0(mc_color_pair_t,1);
    if (mc_color_pair == NULL)
    {
	g_free(color_pair);
	return 0;
    }

    mc_color_pair->cfg = c_fg;
    mc_color_pair->cbg = c_bg;
    mc_color_pair->ifg = mc_tty_color_get_index_by_name(c_fg);
    mc_color_pair->ibg = mc_tty_color_get_index_by_name(c_bg);
    mc_color_pair->pair_index = mc_tty_color__count++;

    mc_tty_color_try_alloc_pair_lib (mc_color_pair);

    g_hash_table_insert (mc_tty_color__hashtable, (gpointer) color_pair, (gpointer) mc_color_pair);

    return mc_color_pair->pair_index;
}

/* --------------------------------------------------------------------------------------------- */

void tty_setcolor (int color)
{
    mc_tty_color_set_lib (color);
}

/* --------------------------------------------------------------------------------------------- */

void tty_lowlevel_setcolor (int color)
{
    mc_tty_color_lowlevel_set_lib(color);
}

/* --------------------------------------------------------------------------------------------- */

void tty_set_normal_attrs (void)
{
    mc_tty_color_set_normal_attrs_lib();
}

/* --------------------------------------------------------------------------------------------- */
