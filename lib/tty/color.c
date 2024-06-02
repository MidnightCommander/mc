/*
   Color setup.
   Interface functions.

   Copyright (C) 1994-2024
   Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2009
   Slava Zanko <slavazanko@gmail.com>, 2009
   Egmont Koblinger <egmont@gmail.com>, 2010

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file color.c
 *  \brief Source: color setup
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>          /* size_t */

#include "lib/global.h"

#include "tty.h"
#include "color.h"

#include "color-internal.h"

/*** global variables ****************************************************************************/

/* *INDENT-OFF* */
static tty_color_pair_t tty_color_defaults =
{
    .fg = NULL,
    .bg = NULL,
    .attrs = NULL,
    .pair_index = 0
};
/* *INDENT-ON* */

/* Set if we are actually using colors */
gboolean use_colors = FALSE;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static GHashTable *mc_tty_color__hashtable = NULL;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
mc_color__deinit (tty_color_pair_t *color)
{
    g_free (color->fg);
    g_free (color->bg);
    g_free (color->attrs);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
tty_color_free_temp_cb (gpointer key, gpointer value, gpointer user_data)
{
    (void) key;
    (void) user_data;

    return ((tty_color_lib_pair_t *) value)->is_temp;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
tty_color_get_next_cpn_cb (gpointer key, gpointer value, gpointer user_data)
{
    tty_color_lib_pair_t *mc_color_pair = (tty_color_lib_pair_t *) value;
    size_t cp = GPOINTER_TO_SIZE (user_data);

    (void) key;

    return (cp == mc_color_pair->pair_index);
}

/* --------------------------------------------------------------------------------------------- */

static size_t
tty_color_get_next__color_pair_number (void)
{
    size_t cp_count, cp;

    cp_count = g_hash_table_size (mc_tty_color__hashtable);
    for (cp = 0; cp < cp_count; cp++)
        if (g_hash_table_find (mc_tty_color__hashtable, tty_color_get_next_cpn_cb,
                               GSIZE_TO_POINTER (cp)) == NULL)
            break;

    return cp;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
tty_init_colors (gboolean disable, gboolean force)
{
    tty_color_init_lib (disable, force);
    mc_tty_color__hashtable = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_colors_done (void)
{
    tty_color_deinit_lib ();
    mc_color__deinit (&tty_color_defaults);
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
tty_try_alloc_color_pair (const tty_color_pair_t *color, gboolean is_temp)
{
    gboolean is_base;
    gchar *color_pair;
    tty_color_lib_pair_t *mc_color_pair;
    int ifg, ibg, attr;

    is_base = (color->fg == NULL || strcmp (color->fg, "base") == 0);
    ifg = tty_color_get_index_by_name (is_base ? tty_color_defaults.fg : color->fg);
    is_base = (color->bg == NULL || strcmp (color->bg, "base") == 0);
    ibg = tty_color_get_index_by_name (is_base ? tty_color_defaults.bg : color->bg);
    is_base = (color->attrs == NULL || strcmp (color->attrs, "base") == 0);
    attr = tty_attr_get_bits (is_base ? tty_color_defaults.attrs : color->attrs);

    color_pair = g_strdup_printf ("%d.%d.%d", ifg, ibg, attr);
    if (color_pair == NULL)
        return 0;

    mc_color_pair =
        (tty_color_lib_pair_t *) g_hash_table_lookup (mc_tty_color__hashtable,
                                                      (gpointer) color_pair);

    if (mc_color_pair != NULL)
    {
        g_free (color_pair);
        return mc_color_pair->pair_index;
    }

    mc_color_pair = g_try_new0 (tty_color_lib_pair_t, 1);
    if (mc_color_pair == NULL)
    {
        g_free (color_pair);
        return 0;
    }

    mc_color_pair->is_temp = is_temp;
    mc_color_pair->fg = ifg;
    mc_color_pair->bg = ibg;
    mc_color_pair->attr = attr;
    mc_color_pair->pair_index = tty_color_get_next__color_pair_number ();

    tty_color_try_alloc_lib_pair (mc_color_pair);

    g_hash_table_insert (mc_tty_color__hashtable, (gpointer) color_pair, (gpointer) mc_color_pair);

    return mc_color_pair->pair_index;
}

/* --------------------------------------------------------------------------------------------- */

void
tty_color_free_temp (void)
{
    g_hash_table_foreach_remove (mc_tty_color__hashtable, tty_color_free_temp_cb, NULL);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_color_free_all (void)
{
    g_hash_table_remove_all (mc_tty_color__hashtable);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_color_set_defaults (const tty_color_pair_t *color)
{
    mc_color__deinit (&tty_color_defaults);

    tty_color_defaults.fg = g_strdup (color->fg);
    tty_color_defaults.bg = g_strdup (color->bg);
    tty_color_defaults.attrs = g_strdup (color->attrs);
    tty_color_defaults.pair_index = 0;
}

/* --------------------------------------------------------------------------------------------- */
