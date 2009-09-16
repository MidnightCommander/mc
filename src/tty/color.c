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
#include <sys/types.h>          /* size_t */

#include "../../src/global.h"

#include "../../src/tty/tty.h"
#include "../../src/tty/color.h"

#include "../../src/tty/color-internal.h"

/*** global variables ****************************************************************************/

char *command_line_colors = NULL;

static char *tty_color_defaults__fg = NULL;
static char *tty_color_defaults__bg = NULL;

/* Set if we are actually using colors */
gboolean use_colors = FALSE;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static GHashTable *mc_tty_color__hashtable = NULL;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static inline void
color_hash_destroy_key (gpointer data)
{
    g_free (data);
}

/* --------------------------------------------------------------------------------------------- */

static void
color_hash_destroy_value (gpointer data)
{
    tty_color_pair_t *mc_color_pair = (tty_color_pair_t *) data;
    g_free (mc_color_pair);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
tty_color_free_condition_cb (gpointer key, gpointer value, gpointer user_data)
{
    gboolean is_temp_color;
    tty_color_pair_t *mc_color_pair;
    (void) key;

    is_temp_color = (gboolean) user_data;
    mc_color_pair = (tty_color_pair_t *) value;
    return (mc_color_pair->is_temp == is_temp_color);
}

/* --------------------------------------------------------------------------------------------- */

static void
tty_color_free_all (gboolean is_temp_color)
{
    g_hash_table_foreach_remove (mc_tty_color__hashtable, tty_color_free_condition_cb,
                                 (gpointer) is_temp_color);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
tty_color_get_next_cpn_cb (gpointer key, gpointer value, gpointer user_data)
{
    int cp;
    tty_color_pair_t *mc_color_pair;
    (void) key;

    cp = (int) user_data;
    mc_color_pair = (tty_color_pair_t *) value;

    if (cp == mc_color_pair->pair_index)
        return TRUE;

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static int
tty_color_get_next__color_pair_number ()
{
    int cp_count = g_hash_table_size (mc_tty_color__hashtable);
    int cp = 0;

    for (cp = 0; cp < cp_count; cp++) {
        if (g_hash_table_find (mc_tty_color__hashtable, tty_color_get_next_cpn_cb, (gpointer) cp) ==
            NULL)
            return cp;
    }
    return cp;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
tty_init_colors (gboolean disable, gboolean force)
{
    tty_color_init_lib (disable, force);
    mc_tty_color__hashtable = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                     color_hash_destroy_key,
                                                     color_hash_destroy_value);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_colors_done (void)
{
    tty_color_deinit_lib ();
    g_free (tty_color_defaults__fg);
    g_free (tty_color_defaults__bg);

    g_hash_table_destroy (mc_tty_color__hashtable);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
tty_use_colors (void)
{
    return use_colors;
}

/* --------------------------------------------------------------------------------------------- */

int
tty_try_alloc_color_pair2 (const char *fg, const char *bg, gboolean is_temp_color)
{
    gchar *color_pair;
    tty_color_pair_t *mc_color_pair;
    const char *c_fg, *c_bg;

    if (fg == NULL)
        fg = tty_color_defaults__fg;

    if (bg == NULL) {
        bg = tty_color_defaults__bg;
    }
    c_fg = tty_color_get_valid_name (fg);
    c_bg = tty_color_get_valid_name (bg);

    color_pair = g_strdup_printf ("%s.%s", c_fg, c_bg);
    if (color_pair == NULL)
        return 0;

    mc_color_pair =
        (tty_color_pair_t *) g_hash_table_lookup (mc_tty_color__hashtable, (gpointer) color_pair);

    if (mc_color_pair != NULL) {
        g_free (color_pair);
        return mc_color_pair->pair_index;
    }

    mc_color_pair = g_new0 (tty_color_pair_t, 1);
    if (mc_color_pair == NULL) {
        g_free (color_pair);
        return 0;
    }

    mc_color_pair->is_temp = is_temp_color;
    mc_color_pair->cfg = c_fg;
    mc_color_pair->cbg = c_bg;
    mc_color_pair->ifg = tty_color_get_index_by_name (c_fg);
    mc_color_pair->ibg = tty_color_get_index_by_name (c_bg);
    mc_color_pair->pair_index = tty_color_get_next__color_pair_number ();

    tty_color_try_alloc_pair_lib (mc_color_pair);

    g_hash_table_insert (mc_tty_color__hashtable, (gpointer) color_pair, (gpointer) mc_color_pair);

    return mc_color_pair->pair_index;
}

/* --------------------------------------------------------------------------------------------- */

int
tty_try_alloc_color_pair (const char *fg, const char *bg)
{
    return tty_try_alloc_color_pair2 (fg, bg, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_color_free_all_tmp (void)
{
    tty_color_free_all (TRUE);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_color_free_all_non_tmp (void)
{
    tty_color_free_all (FALSE);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_color_set_defaults (const char *fgcolor, const char *bgcolor)
{
    g_free (tty_color_defaults__fg);
    g_free (tty_color_defaults__fg);

    tty_color_defaults__fg = (fgcolor != NULL) ? g_strdup (fgcolor) : NULL;
    tty_color_defaults__bg = (bgcolor != NULL) ? g_strdup (bgcolor) : NULL;
}

/* --------------------------------------------------------------------------------------------- */
