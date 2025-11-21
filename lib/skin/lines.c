/*
   Skins engine.
   Work with line draving chars.

   Copyright (C) 2009-2025
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2009.

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

#include <config.h>
#include <stdlib.h>

#include "internal.h"
#include "lib/tty/tty.h"
#include "lib/strutil.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static mc_tty_char_t
unicode_to_mc_acs (gunichar c)
{
    switch (c)
    {
    case 0x2500:  // ─
        return MC_ACS_HLINE;
    case 0x2502:  // │
        return MC_ACS_VLINE;
    case 0x250C:  // ┌
        return MC_ACS_ULCORNER;
    case 0x2510:  // ┐
        return MC_ACS_URCORNER;
    case 0x2514:  // └
        return MC_ACS_LLCORNER;
    case 0x2518:  // ┘
        return MC_ACS_LRCORNER;
    case 0x251C:  // ├
        return MC_ACS_LTEE;
    case 0x2524:  // ┤
        return MC_ACS_RTEE;
    case 0x252C:  // ┬
        return MC_ACS_TTEE;
    case 0x2534:  // ┴
        return MC_ACS_BTEE;
    case 0x253C:  // ┼
        return MC_ACS_PLUS;

    default:
        return 0;
    }
}

/* --------------------------------------------------------------------------------------------- */

// "def" is the default in case the skin doesn't define anything. It's not the fallback in case the
// character cannot be converted to the locale, the fallback is handled by the caller.
static gboolean
skin_get_char (mc_skin_t *mc_skin, const char *name, gunichar def, mc_tty_char_t *result)
{
    gunichar c;
    GIConv conv;
    GString *buffer;
    estr_t conv_res;
    char *value_utf8;

    value_utf8 = mc_config_get_string_raw (mc_skin->config, "Lines", name, NULL);
    if (value_utf8 != NULL)
    {
        c = g_utf8_get_char_validated (value_utf8, -1);
        if (c == (gunichar) (-1) || c == (gunichar) (-2))
        {
            g_free (value_utf8);
            return FALSE;
        }
    }
    else
    {
        // the default to be used if not defined in the skin file
        value_utf8 = g_malloc (7);
        const int len = g_unichar_to_utf8 (def, value_utf8);
        value_utf8[len] = '\0';
        c = def;
    }

    // in UTF-8 nothing left to do, no ACS business, just return the codepoint
    if (mc_global.utf8_display)
    {
        g_free (value_utf8);
        *result = c;
        return TRUE;
    }

    // use MC_ACS_* values for single line drawing chars
    const mc_tty_char_t mc_acs = unicode_to_mc_acs (c);
    if (mc_acs != 0)
    {
        g_free (value_utf8);
        *result = mc_acs;
        return TRUE;
    }

    // convert to 8-bit charset, e.g. KOI8-R has all the double line chars

#ifdef HAVE_NCURSES
    // ncurses versions before 6.5-20251115 are buggy in KOI8-R locale, see mc #4799.
    // Claim failure so that the caller will fall back to ACS single lines.
    // ASCII chars (e.g. with `--stickchars`) need to fall through.
    if (ncurses_koi8r_double_line_bug && c >= 128)
    {
        g_free (value_utf8);
        return FALSE;
    }
#endif

    conv = str_crt_conv_from ("UTF-8");
    if (conv == INVALID_CONV)
    {
        g_free (value_utf8);
        return FALSE;
    }

    buffer = g_string_new ("");
    conv_res = str_convert (conv, value_utf8, buffer);
    if (conv_res == ESTR_SUCCESS)
        *result = (unsigned char) buffer->str[0];
    str_close_conv (conv);
    g_string_free (buffer, TRUE);
    g_free (value_utf8);
    return conv_res == ESTR_SUCCESS;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mc_skin_lines_parse_ini_file (mc_skin_t *mc_skin)
{
    gboolean success;

    if (mc_global.tty.slow_terminal)
        mc_skin_hardcoded_space_lines (mc_skin);
    else if (mc_global.tty.ugly_line_drawing)
        mc_skin_hardcoded_ugly_lines (mc_skin);

    // single lines
    success = TRUE /* just for nicer indentation */
        && skin_get_char (mc_skin, "horiz", 0x2500, &mc_tty_frm[MC_TTY_FRM_HORIZ])
        && skin_get_char (mc_skin, "vert", 0x2502, &mc_tty_frm[MC_TTY_FRM_VERT])
        && skin_get_char (mc_skin, "lefttop", 0x250C, &mc_tty_frm[MC_TTY_FRM_LEFTTOP])
        && skin_get_char (mc_skin, "righttop", 0x2510, &mc_tty_frm[MC_TTY_FRM_RIGHTTOP])
        && skin_get_char (mc_skin, "leftbottom", 0x2514, &mc_tty_frm[MC_TTY_FRM_LEFTBOTTOM])
        && skin_get_char (mc_skin, "rightbottom", 0x2518, &mc_tty_frm[MC_TTY_FRM_RIGHTBOTTOM])
        && skin_get_char (mc_skin, "topmiddle", 0x252C, &mc_tty_frm[MC_TTY_FRM_TOPMIDDLE])
        && skin_get_char (mc_skin, "bottommiddle", 0x2534, &mc_tty_frm[MC_TTY_FRM_BOTTOMMIDDLE])
        && skin_get_char (mc_skin, "leftmiddle", 0x251C, &mc_tty_frm[MC_TTY_FRM_LEFTMIDDLE])
        && skin_get_char (mc_skin, "rightmiddle", 0x2524, &mc_tty_frm[MC_TTY_FRM_RIGHTMIDDLE])
        && skin_get_char (mc_skin, "cross", 0x253C, &mc_tty_frm[MC_TTY_FRM_CROSS]);

    // if any of them are unavailable, revert all of them to regular single lines
    if (!success)
    {
        mc_tty_frm[MC_TTY_FRM_HORIZ] = MC_ACS_HLINE;
        mc_tty_frm[MC_TTY_FRM_VERT] = MC_ACS_VLINE;
        mc_tty_frm[MC_TTY_FRM_LEFTTOP] = MC_ACS_ULCORNER;
        mc_tty_frm[MC_TTY_FRM_RIGHTTOP] = MC_ACS_URCORNER;
        mc_tty_frm[MC_TTY_FRM_LEFTBOTTOM] = MC_ACS_LLCORNER;
        mc_tty_frm[MC_TTY_FRM_RIGHTBOTTOM] = MC_ACS_LRCORNER;
        mc_tty_frm[MC_TTY_FRM_TOPMIDDLE] = MC_ACS_TTEE;
        mc_tty_frm[MC_TTY_FRM_BOTTOMMIDDLE] = MC_ACS_BTEE;
        mc_tty_frm[MC_TTY_FRM_LEFTMIDDLE] = MC_ACS_LTEE;
        mc_tty_frm[MC_TTY_FRM_RIGHTMIDDLE] = MC_ACS_RTEE;
        mc_tty_frm[MC_TTY_FRM_CROSS] = MC_ACS_PLUS;
    }

    // double lines
    success = TRUE /* just for nicer indentation */
        && skin_get_char (mc_skin, "dhoriz", 0x2550, &mc_tty_frm[MC_TTY_FRM_DHORIZ])
        && skin_get_char (mc_skin, "dvert", 0x2551, &mc_tty_frm[MC_TTY_FRM_DVERT])
        && skin_get_char (mc_skin, "dlefttop", 0x2554, &mc_tty_frm[MC_TTY_FRM_DLEFTTOP])
        && skin_get_char (mc_skin, "drighttop", 0x2557, &mc_tty_frm[MC_TTY_FRM_DRIGHTTOP])
        && skin_get_char (mc_skin, "dleftbottom", 0x255A, &mc_tty_frm[MC_TTY_FRM_DLEFTBOTTOM])
        && skin_get_char (mc_skin, "drightbottom", 0x255D, &mc_tty_frm[MC_TTY_FRM_DRIGHTBOTTOM])
        && skin_get_char (mc_skin, "dtopmiddle", 0x2564, &mc_tty_frm[MC_TTY_FRM_DTOPMIDDLE])
        && skin_get_char (mc_skin, "dbottommiddle", 0x2567, &mc_tty_frm[MC_TTY_FRM_DBOTTOMMIDDLE])
        && skin_get_char (mc_skin, "dleftmiddle", 0x255F, &mc_tty_frm[MC_TTY_FRM_DLEFTMIDDLE])
        && skin_get_char (mc_skin, "drightmiddle", 0x2562, &mc_tty_frm[MC_TTY_FRM_DRIGHTMIDDLE]);

    // if any of them are unavailable, revert all of them to their single counterpart
    if (!success)
    {
        mc_tty_frm[MC_TTY_FRM_DHORIZ] = mc_tty_frm[MC_TTY_FRM_HORIZ];
        mc_tty_frm[MC_TTY_FRM_DVERT] = mc_tty_frm[MC_TTY_FRM_VERT];
        mc_tty_frm[MC_TTY_FRM_DLEFTTOP] = mc_tty_frm[MC_TTY_FRM_LEFTTOP];
        mc_tty_frm[MC_TTY_FRM_DRIGHTTOP] = mc_tty_frm[MC_TTY_FRM_RIGHTTOP];
        mc_tty_frm[MC_TTY_FRM_DLEFTBOTTOM] = mc_tty_frm[MC_TTY_FRM_LEFTBOTTOM];
        mc_tty_frm[MC_TTY_FRM_DRIGHTBOTTOM] = mc_tty_frm[MC_TTY_FRM_RIGHTBOTTOM];
        mc_tty_frm[MC_TTY_FRM_DTOPMIDDLE] = mc_tty_frm[MC_TTY_FRM_TOPMIDDLE];
        mc_tty_frm[MC_TTY_FRM_DBOTTOMMIDDLE] = mc_tty_frm[MC_TTY_FRM_BOTTOMMIDDLE];
        mc_tty_frm[MC_TTY_FRM_DLEFTMIDDLE] = mc_tty_frm[MC_TTY_FRM_LEFTMIDDLE];
        mc_tty_frm[MC_TTY_FRM_DRIGHTMIDDLE] = mc_tty_frm[MC_TTY_FRM_RIGHTMIDDLE];
    }
}

/* --------------------------------------------------------------------------------------------- */
