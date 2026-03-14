/*
   Color setup for NCurses screen library

   Copyright (C) 1994-2026
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

typedef struct
{
    int pair;   // ncurses color pair index
    int attrs;  // attributes
} mc_tty_ncurses_color_pair_and_attrs_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/*
 * Our bookkeeping of the ncurses color pair indices, indexed by the "{fg}.{bg}" string.
 */
static GHashTable *mc_tty_ncurses_color_pairs = NULL;

/*
 * Indexed by mc's color index, points to the mc_tty_ncurses_color_pair_and_attrs_t object
 * representing the ncurses color pair index and the attributes.
 *
 * mc's color index represents unique (fg, bg, attrs) tuples. Allocating an ncurses color pair
 * for each of them might be too wasteful and might cause us to run out of available color pairs
 * too soon (especially in 8-color terminals), if many combinations only differ in attrs.
 * So we allocate a new ncurses color pair only if the (fg, bg) tuple is newly seen.
 * See #5020 for details.
 */
static GArray *mc_tty_ncurses_color_pair_and_attrs = NULL;

static int mc_tty_ncurses_next_color_pair = 0;

static int overlay_colors = 0;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static int
get_ncurses_color_pair (int ifg, int ibg)
{
    char *color_pair_str;
    int *ncurses_color_pair;
    int init_pair_ret;

    color_pair_str = g_strdup_printf ("%d.%d", ifg, ibg);

    ncurses_color_pair =
        (int *) g_hash_table_lookup (mc_tty_ncurses_color_pairs, (gpointer) color_pair_str);

    if (ncurses_color_pair == NULL)
    {
        ncurses_color_pair = g_try_new0 (int, 1);
        *ncurses_color_pair = mc_tty_ncurses_next_color_pair;
#if NCURSES_VERSION_PATCH >= 20170401 && defined(NCURSES_EXT_COLORS) && defined(NCURSES_EXT_FUNCS) \
    && defined(HAVE_NCURSES_WIDECHAR)
        init_pair_ret = init_extended_pair (*ncurses_color_pair, ifg, ibg);
#else
        init_pair_ret = init_pair (*ncurses_color_pair, ifg, ibg);
#endif

        if (init_pair_ret == ERR)
        {
            g_free (ncurses_color_pair);
            g_free (color_pair_str);
            return 0;
        }

        g_hash_table_insert (mc_tty_ncurses_color_pairs, color_pair_str, ncurses_color_pair);
        mc_tty_ncurses_next_color_pair++;
    }
    else
        g_free (color_pair_str);

    return *ncurses_color_pair;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
tty_color_init_lib (gboolean disable, gboolean force)
{
    int default_color_pair_id;

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

    // our tracking of ncurses's color pairs
    mc_tty_ncurses_color_pairs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

    // ncurses color pair 0 always refers to the default colors; add it to our hash
    mc_tty_ncurses_next_color_pair = 0;
    default_color_pair_id = get_ncurses_color_pair (-1, -1);
    g_assert (default_color_pair_id == 0);
    (void) default_color_pair_id;  // unused if g_assert is eliminated

    // mapping from our index to ncurses's index and attributes
    mc_tty_ncurses_color_pair_and_attrs =
        g_array_new (FALSE, FALSE, sizeof (mc_tty_ncurses_color_pair_and_attrs_t));
}

/* --------------------------------------------------------------------------------------------- */

void
tty_color_deinit_lib (void)
{
    g_hash_table_destroy (mc_tty_ncurses_color_pairs);
    mc_tty_ncurses_color_pairs = NULL;

    g_array_free (mc_tty_ncurses_color_pair_and_attrs, TRUE);
    mc_tty_ncurses_color_pair_and_attrs = NULL;

    mc_tty_ncurses_next_color_pair = 0;
}

/* --------------------------------------------------------------------------------------------- */

void
tty_color_try_alloc_lib_pair (tty_color_lib_pair_t *mc_color_pair)
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
    if (ifg >= 0 && (ifg & FLAG_TRUECOLOR) != 0)
    {
        ifg &= ~FLAG_TRUECOLOR;
        if (ifg <= overlay_colors)
            ifg += (1 << 16);
    }

    if (ibg >= 0 && (ibg & FLAG_TRUECOLOR) != 0)
    {
        ibg &= ~FLAG_TRUECOLOR;
        if (ibg <= overlay_colors)
            ibg += (1 << 16);
    }

    const int ncurses_color_pair = get_ncurses_color_pair (ifg, ibg);
    const mc_tty_ncurses_color_pair_and_attrs_t pair_and_attrs = { .pair = ncurses_color_pair,
                                                                   .attrs = attr };

    g_array_insert_val (mc_tty_ncurses_color_pair_and_attrs, mc_color_pair->pair_index,
                        pair_and_attrs);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_setcolor (int color)
{
    mc_tty_ncurses_color_pair_and_attrs_t *pair_and_attrs;

    color = tty_maybe_map_color (color);
    pair_and_attrs = &g_array_index (mc_tty_ncurses_color_pair_and_attrs,
                                     mc_tty_ncurses_color_pair_and_attrs_t, color);
    attr_set (pair_and_attrs->attrs, pair_and_attrs->pair, NULL);
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

void
tty_colorize_area (int y, int x, int rows, int cols, int color)
{
#ifdef ENABLE_SHADOWS
    cchar_t *ctext;
    wchar_t wch[CCHARW_MAX + 1];
    attr_t attrs;
    short color_pair;

    if (!use_colors || !tty_clip (&y, &x, &rows, &cols))
        return;

    color = tty_maybe_map_color (color);
    color = g_array_index (mc_tty_ncurses_color_pair_and_attrs,
                           mc_tty_ncurses_color_pair_and_attrs_t, color)
                .pair;

    ctext = g_malloc (sizeof (cchar_t) * (cols + 1));

    for (int row = 0; row < rows; row++)
    {
        mvin_wchnstr (y + row, x, ctext, cols);

        for (int col = 0; col < cols; col++)
        {
            getcchar (&ctext[col], wch, &attrs, &color_pair, NULL);
            setcchar (&ctext[col], wch, attrs, color, NULL);
        }

        mvadd_wchnstr (y + row, x, ctext, cols);
    }

    g_free (ctext);
#else
    (void) y;
    (void) x;
    (void) rows;
    (void) cols;
    (void) color;
#endif
}

/* --------------------------------------------------------------------------------------------- */
