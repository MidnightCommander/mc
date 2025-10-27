/*
   Color setup for NCurses screen library

   Copyright (C) 1994-2025
   Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2009
   Slava Zanko <slavazanko@gmail.com>, 2010
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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/** \file color-ncurses.c
 *  \brief Source: NCUrses-specific color setup
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>  // size_t

#include "lib/global.h"

#include "tty.h"
#include "tty-ncurses.h"
#include "color.h"  // variables
#include "color-internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static GHashTable *mc_tty_color_color_pair_attrs = NULL;
static int overlay_colors = 0;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static inline void
mc_tty_color_attr_destroy_cb (gpointer data)
{
    g_free (data);
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_tty_color_save_attr (int color_pair, int color_attr)
{
    int *attr, *key;

    attr = g_try_new0 (int, 1);
    if (attr == NULL)
        return;

    key = g_try_new (int, 1);
    if (key == NULL)
    {
        g_free (attr);
        return;
    }

    *key = color_pair;
    *attr = color_attr;

    g_hash_table_replace (mc_tty_color_color_pair_attrs, (gpointer) key, (gpointer) attr);
}

/* --------------------------------------------------------------------------------------------- */

static int
color_get_attr (int color_pair)
{
    int *fnd = NULL;

    if (mc_tty_color_color_pair_attrs != NULL)
        fnd = (int *) g_hash_table_lookup (mc_tty_color_color_pair_attrs, (gpointer) &color_pair);
    return (fnd != NULL) ? *fnd : 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_tty_color_pair_init_special (tty_color_lib_pair_t *mc_color_pair, int fg1, int bg1, int fg2,
                                int bg2, int attr)
{
    if (has_colors () && !mc_tty_color_disable)
        init_pair (mc_color_pair->pair_index, fg1, bg1);
    else
        init_pair (mc_color_pair->pair_index, fg2, bg2);
    mc_tty_color_save_attr (mc_color_pair->pair_index, attr);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
tty_color_init_lib (gboolean disable, gboolean force)
{
    (void) force;

    if (has_colors () && !disable)
    {
        use_colors = TRUE;
        start_color ();
        use_default_colors ();

        // Extended color mode detection routines must first be called before loading any skin
        tty_use_256colors (NULL);
        tty_use_truecolors (NULL);
    }

    mc_tty_color_color_pair_attrs = g_hash_table_new_full (
        g_int_hash, g_int_equal, mc_tty_color_attr_destroy_cb, mc_tty_color_attr_destroy_cb);
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
tty_color_try_alloc_lib_pair (tty_color_lib_pair_t *mc_color_pair)
{
    if (mc_color_pair->fg <= (int) SPEC_A_REVERSE)
    {
        switch (mc_color_pair->fg)
        {
        case SPEC_A_REVERSE:
            mc_tty_color_pair_init_special (mc_color_pair, COLOR_BLACK, COLOR_WHITE, COLOR_BLACK,
                                            COLOR_WHITE | A_BOLD, A_REVERSE);
            break;
        case SPEC_A_BOLD:
            mc_tty_color_pair_init_special (mc_color_pair, COLOR_WHITE, COLOR_BLACK, COLOR_WHITE,
                                            COLOR_BLACK, A_BOLD);
            break;
        case SPEC_A_BOLD_REVERSE:
            mc_tty_color_pair_init_special (mc_color_pair, COLOR_WHITE, COLOR_WHITE, COLOR_WHITE,
                                            COLOR_WHITE, A_BOLD | A_REVERSE);
            break;
        case SPEC_A_UNDERLINE:
            mc_tty_color_pair_init_special (mc_color_pair, COLOR_WHITE, COLOR_BLACK, COLOR_WHITE,
                                            COLOR_BLACK, A_UNDERLINE);
            break;
        default:
            break;
        }
    }
    else
    {
        int ifg, ibg, attr;

        ifg = mc_color_pair->fg;
        ibg = mc_color_pair->bg;
        attr = mc_color_pair->attr;

        // If we have 8 indexed colors only, change foreground bright colors into bold and
        // background bright colors to basic colors
        if (COLORS <= 8 || (tty_use_truecolors (NULL) && overlay_colors <= 8))
        {
            if (ifg >= 8 && ifg < 16)
            {
                ifg &= 0x07;
                attr |= A_BOLD;
            }

            if (ibg >= 8 && ibg < 16)
            {
                ibg &= 0x07;
            }
        }

        // Shady trick: if we don't have the exact color, because it is overlaid by backwards
        // compatibility indexed values, just borrow one degree of red. The user won't notice :)
        if ((ifg & FLAG_TRUECOLOR) != 0)
        {
            ifg &= ~FLAG_TRUECOLOR;
            if (ifg != 0 && ifg <= overlay_colors)
                ifg += (1 << 16);
        }

        if ((ibg & FLAG_TRUECOLOR) != 0)
        {
            ibg &= ~FLAG_TRUECOLOR;
            if (ibg != 0 && ibg <= overlay_colors)
                ibg += (1 << 16);
        }

#if NCURSES_VERSION_PATCH >= 20170401 && defined(NCURSES_EXT_COLORS) && defined(NCURSES_EXT_FUNCS) \
    && defined(HAVE_NCURSES_WIDECHAR)
        init_extended_pair (mc_color_pair->pair_index, ifg, ibg);
#else
        init_pair (mc_color_pair->pair_index, ifg, ibg);
#endif
        mc_tty_color_save_attr (mc_color_pair->pair_index, attr);
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
    tty_setcolor (color);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_set_normal_attrs (void)
{
    standend ();
}

/* --------------------------------------------------------------------------------------------- */

gboolean
tty_use_256colors (GError **error)
{
    (void) error;

    overlay_colors = tty_tigetnum ("CO", NULL);

    if (COLORS == 256 || (COLORS > 256 && overlay_colors == 256))
        return TRUE;

    if (tty_use_truecolors (NULL))
    {
        need_convert_256color = TRUE;
        return TRUE;
    }

    g_set_error (error, MC_ERROR, -1,
                 _ ("\nIf your terminal supports 256 colors, you need to set your TERM\n"
                    "environment variable to match your terminal, perhaps using\n"
                    "a *-256color or *-direct256 variant. Use the 'toe -a'\n"
                    "command to list all available variants on your system.\n"));
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
tty_use_truecolors (GError **error)
{
    // Low level true color is supported since ncurses 6.0 patch 20170401 preceding release
    // of ncurses 6.1. It needs ABI 6 or higher.
#if !(NCURSES_VERSION_PATCH >= 20170401 && defined(NCURSES_EXT_COLORS)                             \
      && defined(NCURSES_EXT_FUNCS) && defined(HAVE_NCURSES_WIDECHAR))
    g_set_error (error, MC_ERROR, -1,
                 _ ("For true color support, you need version 6.1 or later of the ncurses\n"
                    "library with wide character and ABI 6 or higher support.\n"
                    "Please upgrade your system.\n"));
    return FALSE;
#else
    // We support only bool RGB cap configuration (8:8:8 bits), but the other variants are so rare
    // that we don't need to bother.
    if (!(tty_tigetflag ("RGB", NULL) && COLORS == COLORS_TRUECOLOR))
    {
        g_set_error (
            error, MC_ERROR, -1,
            _ ("\nIf your terminal supports true colors, you need to set your TERM\n"
               "environment variable to a *-direct256, *-direct16, or *-direct variant.\n"
               "Use the 'toe -a' command to list all available variants on your system.\n"));
        return FALSE;
    }

    overlay_colors = tty_tigetnum ("CO", NULL);

    return TRUE;
#endif
}

/* --------------------------------------------------------------------------------------------- */
