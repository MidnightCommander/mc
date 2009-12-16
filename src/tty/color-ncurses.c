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
#include <sys/types.h>          /* size_t */

#include "../../src/global.h"

#include "../../src/tty/tty-ncurses.h"
#include "../../src/tty/color.h"        /* variables */
#include "../../src/tty/color-internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static GHashTable *mc_tty_color_color_pair_attrs = NULL;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static inline void
mc_tty_color_attr_destroy_cb (gpointer data)
{
    g_free (data);
}

/* --------------------------------------------------------------------------------------------- */

static int
mc_tty_color_save_attr_lib (int color_pair, int color_attr)
{
    int *attr, *key;
    attr = g_try_new0 (int, 1);
    if (attr == NULL)
        return color_attr;

    key = g_try_new (int, 1);
    if (key == NULL) {
        g_free (attr);
        return color_attr;
    }

    *key = color_pair;

    if (color_attr != -1)
        *attr = color_attr & (A_BOLD | A_REVERSE | A_UNDERLINE);
    g_hash_table_replace (mc_tty_color_color_pair_attrs, (gpointer) key, (gpointer) attr);
    return color_attr & (~(*attr));
}

/* --------------------------------------------------------------------------------------------- */

static int
color_get_attr (int color_pair)
{
    int *fnd = NULL;

    if (mc_tty_color_color_pair_attrs != NULL)
	fnd = (int *) g_hash_table_lookup (mc_tty_color_color_pair_attrs, (gpointer) & color_pair);
    return (fnd != NULL) ? *fnd : 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_tty_color_pair_init_special (tty_color_pair_t * mc_color_pair,
                                int fg1, int bg1, int fg2, int bg2, int mask)
{
    if (has_colors ()) {
        if (!mc_tty_color_disable) {
            init_pair (mc_color_pair->pair_index,
                       mc_tty_color_save_attr_lib (mc_color_pair->pair_index, fg1 | mask), bg1);
        } else {
            init_pair (mc_color_pair->pair_index,
                       mc_tty_color_save_attr_lib (mc_color_pair->pair_index, fg2 | mask), bg2);
        }
    }
#if 0
    else {
        SLtt_set_mono (mc_color_pair->pair_index, NULL, mask);
    }
#endif
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
tty_color_init_lib (gboolean disable, gboolean force)
{
    (void) force;

    if (has_colors () && !disable) {
        use_colors = TRUE;
        start_color ();
        use_default_colors ();
    }

    mc_tty_color_color_pair_attrs = g_hash_table_new_full
        (g_int_hash, g_int_equal, mc_tty_color_attr_destroy_cb, mc_tty_color_attr_destroy_cb);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_color_deinit_lib (void)
{
    g_hash_table_destroy (mc_tty_color_color_pair_attrs);
    mc_tty_color_color_pair_attrs = NULL;
}

/* --------------------------------------------------------------------------------------------- */

void
tty_color_try_alloc_pair_lib (tty_color_pair_t * mc_color_pair)
{
    if (mc_color_pair->ifg <= (int) SPEC_A_REVERSE) {
        switch (mc_color_pair->ifg) {
        case SPEC_A_REVERSE:
            mc_tty_color_pair_init_special (mc_color_pair,
                                            COLOR_BLACK, COLOR_WHITE,
                                            COLOR_BLACK, COLOR_WHITE | A_BOLD, A_REVERSE);
            break;
        case SPEC_A_BOLD:
            mc_tty_color_pair_init_special (mc_color_pair,
                                            COLOR_WHITE, COLOR_BLACK,
                                            COLOR_WHITE, COLOR_BLACK, A_BOLD);
            break;
        case SPEC_A_BOLD_REVERSE:

            mc_tty_color_pair_init_special (mc_color_pair,
                                            COLOR_WHITE, COLOR_WHITE,
                                            COLOR_WHITE, COLOR_WHITE, A_BOLD | A_REVERSE);
            break;
        case SPEC_A_UNDERLINE:
            mc_tty_color_pair_init_special (mc_color_pair,
                                            COLOR_WHITE, COLOR_BLACK,
                                            COLOR_WHITE, COLOR_BLACK, A_UNDERLINE);
            break;
        }
    } else {
        init_pair (mc_color_pair->pair_index,
                   mc_tty_color_save_attr_lib (mc_color_pair->pair_index,
                                               mc_color_pair->ifg) & COLOR_WHITE,
                   mc_color_pair->ibg & COLOR_WHITE);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
tty_setcolor (int color)
{
    attrset (COLOR_PAIR (color) | color_get_attr (color));
}

/* --------------------------------------------------------------------------------------------- */

void
tty_lowlevel_setcolor (int color)
{
    attrset (COLOR_PAIR (color) | color_get_attr (color));
}

/* --------------------------------------------------------------------------------------------- */

void
tty_set_normal_attrs (void)
{
    standend ();
}

/* --------------------------------------------------------------------------------------------- */
