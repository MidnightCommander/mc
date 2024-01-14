/*
   Color setup for S_Lang screen library

   Copyright (C) 1994-2024
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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file color-slang.c
 *  \brief Source: S-Lang-specific color setup
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>          /* size_t */

#include "lib/global.h"
#include "lib/util.h"           /* whitespace() */

#include "tty-slang.h"
#include "color.h"              /* variables */
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

    if (force || (getenv ("COLORTERM") != NULL))
        SLtt_Use_Ansi_Colors = 1;

    if (!mc_tty_color_disable)
    {
        const char *terminal = getenv ("TERM");
        const size_t len = strlen (terminal);
        char *cts = mc_global.tty.color_terminal_string;

        /* check mc_global.tty.color_terminal_string */
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

static void
mc_tty_color_pair_init_special (tty_color_lib_pair_t * mc_color_pair,
                                const char *fg1, const char *bg1,
                                const char *fg2, const char *bg2, SLtt_Char_Type mask)
{
    if (SLtt_Use_Ansi_Colors != 0)
    {
        if (!mc_tty_color_disable)
        {
            SLtt_set_color (mc_color_pair->pair_index, (char *) "", (char *) fg1, (char *) bg1);
        }
        else
        {
            SLtt_set_color (mc_color_pair->pair_index, (char *) "", (char *) fg2, (char *) bg2);
        }
    }
    else
    {
        SLtt_set_mono (mc_color_pair->pair_index, NULL, mask);
    }
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
    }
}

/* --------------------------------------------------------------------------------------------- */

void
tty_color_deinit_lib (void)
{
}

/* --------------------------------------------------------------------------------------------- */

void
tty_color_try_alloc_lib_pair (tty_color_lib_pair_t * mc_color_pair)
{
    if (mc_color_pair->fg <= (int) SPEC_A_REVERSE)
    {
        switch (mc_color_pair->fg)
        {
        case SPEC_A_REVERSE:
            mc_tty_color_pair_init_special (mc_color_pair,
                                            "black", "white", "black", "lightgray", SLTT_REV_MASK);
            break;
        case SPEC_A_BOLD:
            mc_tty_color_pair_init_special (mc_color_pair,
                                            "white", "black", "white", "black", SLTT_BOLD_MASK);
            break;
        case SPEC_A_BOLD_REVERSE:
            mc_tty_color_pair_init_special (mc_color_pair,
                                            "white", "white",
                                            "white", "white", SLTT_BOLD_MASK | SLTT_REV_MASK);
            break;
        case SPEC_A_UNDERLINE:
            mc_tty_color_pair_init_special (mc_color_pair,
                                            "white", "black", "white", "black", SLTT_ULINE_MASK);
            break;
        default:
            break;
        }
    }
    else
    {
        const char *fg, *bg;

        fg = tty_color_get_name_by_index (mc_color_pair->fg);
        bg = tty_color_get_name_by_index (mc_color_pair->bg);
        SLtt_set_color (mc_color_pair->pair_index, (char *) "", (char *) fg, (char *) bg);
        SLtt_add_color_attribute (mc_color_pair->pair_index, mc_color_pair->attr);
    }
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
    SLsmg_set_color (color & 0x7F);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_set_normal_attrs (void)
{
    SLsmg_normal_video ();
}

/* --------------------------------------------------------------------------------------------- */

gboolean
tty_use_256colors (GError ** error)
{
    gboolean ret;

    ret = (SLtt_Use_Ansi_Colors && SLtt_tgetnum ((char *) "Co") == 256);

    if (!ret)
        g_set_error (error, MC_ERROR, -1,
                     _("Your terminal doesn't even seem to support 256 colors."));

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
tty_use_truecolors (GError ** error)
{
    char *colorterm;

    /* True color is supported since slang-2.3.1 on 64-bit machines,
       and expected to be supported from slang-3 on 32-bit machines:
       http://lists.jedsoft.org/lists/slang-users/2016/0000014.html.
       Check for sizeof (long) being 8, exactly as slang does. */
    if (SLang_Version < 20301 || (sizeof (long) != 8 && SLang_Version < 30000))
    {
        g_set_error (error, MC_ERROR, -1, _("True color not supported in this slang version."));
        return FALSE;
    }

    /* Duplicate slang's check so that we can pop up an error message
       rather than silently use wrong colors. */
    colorterm = getenv ("COLORTERM");
    if (colorterm == NULL
        || (strcmp (colorterm, "truecolor") != 0 && strcmp (colorterm, "24bit") != 0))
    {
        g_set_error (error, MC_ERROR, -1,
                     _("Set COLORTERM=truecolor if your terminal really supports true colors."));
        return FALSE;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
