/*
   Color setup for S_Lang screen library

   Copyright (C) 1994-2025
   Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2009
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

/** \file color-slang.c
 *  \brief Source: S-Lang-specific color setup
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>  // size_t

#include "lib/global.h"
#include "lib/util.h"  // whitespace()

#include "tty.h"
#include "tty-slang.h"
#include "color.h"  // variables
#include "color-internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static int
has_colors (gboolean disable, gboolean force)
{
    mc_tty_color_disable = disable;

    // S-Lang enables color if the setaf/setab/setf/setb terminfo capabilities are set or
    // the COLORTERM environment variable is set

    if (force)
        SLtt_Use_Ansi_Colors = 1;

    if (!mc_tty_color_disable)
    {
        const char *terminal = getenv ("TERM");
        const size_t len = strlen (terminal);
        char *cts = mc_global.tty.color_terminal_string;

        // check mc_global.tty.color_terminal_string
        while (*cts != '\0')
        {
            char *s;
            size_t i = 0;

            while (whitespace (*cts))
                cts++;
            s = cts;

            while (*cts != '\0' && *cts != ',')
            {
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

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
tty_color_init_lib (gboolean disable, gboolean force)
{
    /* FIXME: if S-Lang is used, has_colors() must be called regardless
       of whether we are interested in its result */
    if (has_colors (disable, force) && !disable)
    {
        use_colors = TRUE;

        // Extended color mode detection routines must first be called before loading any skin
        tty_use_256colors (NULL);
        tty_use_truecolors (NULL);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
tty_color_deinit_lib (void)
{
}

/* --------------------------------------------------------------------------------------------- */

void
tty_color_try_alloc_lib_pair (tty_color_lib_pair_t *mc_color_pair)
{
    /*
     * According to the S-Lang Library C Programmer's Guide (v2.3.0)
     * (https://www.jedsoft.org/slang/doc/pdf/cslang.pdf), §7.4.4:
     *
     * "[for SLtt_set_color] When the SLtt_Use_Ansi_Colors variable is zero, all objects with
     * numbers greater than one will be displayed in inverse video." Footnote: "This behavior can be
     * modifed by using the SLtt_set_mono function call."
     */
    if (SLtt_Use_Ansi_Colors)
    {
        const char *fg, *bg;

        fg = tty_color_get_name_by_index (mc_color_pair->fg);
        bg = tty_color_get_name_by_index (mc_color_pair->bg);
        SLtt_set_color (mc_color_pair->pair_index, (char *) "", (char *) fg, (char *) bg);
        SLtt_add_color_attribute (mc_color_pair->pair_index, mc_color_pair->attr);
    }
    else
        SLtt_set_mono (mc_color_pair->pair_index, NULL, mc_color_pair->attr);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_setcolor (int color)
{
    SLsmg_set_color (color);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Set colorpair by index, don't interpret S-Lang "emulated attributes"
 */

void
tty_lowlevel_setcolor (int color)
{
    SLsmg_set_color (color);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_set_normal_attrs (void)
{
    SLsmg_normal_video ();
}

/* --------------------------------------------------------------------------------------------- */

gboolean
tty_use_256colors (GError **error)
{
    int colors, overlay_colors;

    colors = tty_tigetnum ("colors", "Co");
    overlay_colors = tty_tigetnum ("CO", NULL);

    if (SLtt_Use_Ansi_Colors && (colors == 256 || (colors > 256 && overlay_colors == 256)))
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
    char *colorterm;

    /* True color is supported since slang-2.3.1 on 64-bit machines,
       and expected to be supported from slang-3 on 32-bit machines:
       https://lists.jedsoft.org/lists/slang-users/2016/0000014.html
       Check for sizeof (long) being 8, exactly as slang does. */
    if (SLang_Version < 20301 || (sizeof (long) != 8 && SLang_Version < 30000))
    {
        g_set_error (error, MC_ERROR, -1, _ ("True color not supported in this slang version."));
        return FALSE;
    }

    /* Duplicate slang's check so that we can pop up an error message
       rather than silently use wrong colors. */
    colorterm = getenv ("COLORTERM");
    if (!((tty_tigetflag ("RGB", NULL) && tty_tigetnum ("colors", "Co") == COLORS_TRUECOLOR)
          || (colorterm != NULL
              && (strcmp (colorterm, "truecolor") == 0 || strcmp (colorterm, "24bit") == 0))))
    {
        g_set_error (error, MC_ERROR, -1,
                     _ ("\nIf your terminal supports true colors, you need to set your TERM\n"
                        "environment variable to a *-direct256, *-direct16, or *-direct variant.\n"
                        "Use the 'toe -a' command to list all available variants on your system.\n"
                        "Alternatively, you can set COLORTERM=truecolor.\n"));
        return FALSE;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
