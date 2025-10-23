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
mc_skin_lines_get_tty_char (mc_skin_t *mc_skin, const char *name, mc_tty_char_t def)
{
    GIConv conv;
    GString *buffer;
    estr_t conv_res;
    char *value_utf8;

    value_utf8 = mc_config_get_string_raw (mc_skin->config, "Lines", name, "");
    const gunichar c = g_utf8_get_char_validated (value_utf8, -1);
    if (c == (gunichar) (-1) || c == (gunichar) (-2))
    {
        g_free (value_utf8);
        return def;
    }

    // Prefer MC_ACS_* values, even if there's a codepoint for this character in the current locale.
    // This gives more flexibility to the ncurses or slang implementation in handling them, and also
    // prevents us from hitting an ncurses bug (mc ticket #4799#issuecomment-3442057420).
    const mc_tty_char_t mc_acs = tty_unicode_to_mc_acs (c);
    if (mc_acs != 0)
    {
        g_free (value_utf8);
        return mc_acs;
    }

    // nothing left to do in UTF-8 locales
    if (mc_global.utf8_display)
    {
        g_free (value_utf8);
        return c;
    }

    // convert to 8-bit charset
    conv = str_crt_conv_from ("UTF-8");
    if (conv == INVALID_CONV)
    {
        g_free (value_utf8);
        return def;
    }

    buffer = g_string_new ("");
    conv_res = str_convert (conv, value_utf8, buffer);
    const mc_tty_char_t retval = (conv_res == ESTR_SUCCESS) ? (unsigned char) buffer->str[0] : def;
    str_close_conv (conv);
    g_string_free (buffer, TRUE);
    g_free (value_utf8);
    return retval;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mc_skin_lines_parse_ini_file (mc_skin_t *mc_skin)
{
    if (mc_global.tty.slow_terminal)
        mc_skin_hardcoded_space_lines (mc_skin);
    else if (mc_global.tty.ugly_line_drawing)
        mc_skin_hardcoded_ugly_lines (mc_skin);

    // single lines
    mc_tty_frm[MC_TTY_FRM_HORIZ] = mc_skin_lines_get_tty_char (mc_skin, "horiz", MC_ACS_HLINE);
    mc_tty_frm[MC_TTY_FRM_VERT] = mc_skin_lines_get_tty_char (mc_skin, "vert", MC_ACS_VLINE);
    mc_tty_frm[MC_TTY_FRM_LEFTTOP] =
        mc_skin_lines_get_tty_char (mc_skin, "lefttop", MC_ACS_ULCORNER);
    mc_tty_frm[MC_TTY_FRM_RIGHTTOP] =
        mc_skin_lines_get_tty_char (mc_skin, "righttop", MC_ACS_URCORNER);
    mc_tty_frm[MC_TTY_FRM_LEFTBOTTOM] =
        mc_skin_lines_get_tty_char (mc_skin, "leftbottom", MC_ACS_LLCORNER);
    mc_tty_frm[MC_TTY_FRM_RIGHTBOTTOM] =
        mc_skin_lines_get_tty_char (mc_skin, "rightbottom", MC_ACS_LRCORNER);
    mc_tty_frm[MC_TTY_FRM_TOPMIDDLE] =
        mc_skin_lines_get_tty_char (mc_skin, "topmiddle", MC_ACS_TTEE);
    mc_tty_frm[MC_TTY_FRM_BOTTOMMIDDLE] =
        mc_skin_lines_get_tty_char (mc_skin, "bottommiddle", MC_ACS_BTEE);
    mc_tty_frm[MC_TTY_FRM_LEFTMIDDLE] =
        mc_skin_lines_get_tty_char (mc_skin, "leftmiddle", MC_ACS_LTEE);
    mc_tty_frm[MC_TTY_FRM_RIGHTMIDDLE] =
        mc_skin_lines_get_tty_char (mc_skin, "rightmiddle", MC_ACS_RTEE);
    mc_tty_frm[MC_TTY_FRM_CROSS] = mc_skin_lines_get_tty_char (mc_skin, "cross", MC_ACS_PLUS);

    // double lines
    mc_tty_frm[MC_TTY_FRM_DHORIZ] =
        mc_skin_lines_get_tty_char (mc_skin, "dhoriz", MC_ACS_DBL_HLINE);
    mc_tty_frm[MC_TTY_FRM_DVERT] = mc_skin_lines_get_tty_char (mc_skin, "dvert", MC_ACS_DBL_VLINE);
    mc_tty_frm[MC_TTY_FRM_DLEFTTOP] =
        mc_skin_lines_get_tty_char (mc_skin, "dlefttop", MC_ACS_DBL_ULCORNER);
    mc_tty_frm[MC_TTY_FRM_DRIGHTTOP] =
        mc_skin_lines_get_tty_char (mc_skin, "drighttop", MC_ACS_DBL_URCORNER);
    mc_tty_frm[MC_TTY_FRM_DLEFTBOTTOM] =
        mc_skin_lines_get_tty_char (mc_skin, "dleftbottom", MC_ACS_DBL_LLCORNER);
    mc_tty_frm[MC_TTY_FRM_DRIGHTBOTTOM] =
        mc_skin_lines_get_tty_char (mc_skin, "drightbottom", MC_ACS_DBL_LRCORNER);
    mc_tty_frm[MC_TTY_FRM_DTOPMIDDLE] =
        mc_skin_lines_get_tty_char (mc_skin, "dtopmiddle", MC_ACS_DBL_TTEE);
    mc_tty_frm[MC_TTY_FRM_DBOTTOMMIDDLE] =
        mc_skin_lines_get_tty_char (mc_skin, "dbottommiddle", MC_ACS_DBL_BTEE);
    mc_tty_frm[MC_TTY_FRM_DLEFTMIDDLE] =
        mc_skin_lines_get_tty_char (mc_skin, "dleftmiddle", MC_ACS_DBL_LTEE);
    mc_tty_frm[MC_TTY_FRM_DRIGHTMIDDLE] =
        mc_skin_lines_get_tty_char (mc_skin, "drightmiddle", MC_ACS_DBL_RTEE);
}

/* --------------------------------------------------------------------------------------------- */
