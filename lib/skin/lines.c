/*
   Skins engine.
   Work with line draving chars.

   Copyright (C) 2009-2017
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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <stdlib.h>

#include "internal.h"
#include "lib/tty/tty.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

static int
mc_skin_lines_load_frm (mc_skin_t * mc_skin, const char *name)
{
    int ret;
    char *frm_val = NULL;
    frm_val = mc_config_get_string_raw (mc_skin->config, "Lines", name, " ");
    ret = mc_tty_normalize_lines_char (frm_val);

    g_free (frm_val);

#if 0
    switch (ret)
    {
    case 0x80:
        ret = ACS_HLINE;
        break;
    case 0x81:
        ret = ACS_VLINE;
        break;
    case 0x82:
        ret = ACS_ULCORNER;
        break;
    case 0x83:
        ret = ACS_URCORNER;
        break;
    case 0x84:
        ret = ACS_LLCORNER;
        break;
    case 0x85:
        ret = ACS_LRCORNER;
        break;
    case 0x86:
        ret = ACS_LTEE;
        break;
    case 0x87:
        ret = ACS_RTEE;
        break;
    case 0x8a:
        ret = ACS_PLUS;
        break;
    default:
        break;
    }
#endif

    return ret;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mc_skin_lines_parse_ini_file (mc_skin_t * mc_skin)
{
    if (mc_global.tty.slow_terminal)
        mc_skin_hardcoded_space_lines (mc_skin);
    else if (mc_global.tty.ugly_line_drawing)
        mc_skin_hardcoded_ugly_lines (mc_skin);

    /* single lines */
    mc_tty_frm[MC_TTY_FRM_VERT] = mc_skin_lines_load_frm (mc_skin, "vert");
    mc_tty_frm[MC_TTY_FRM_HORIZ] = mc_skin_lines_load_frm (mc_skin, "horiz");
    mc_tty_frm[MC_TTY_FRM_LEFTTOP] = mc_skin_lines_load_frm (mc_skin, "lefttop");
    mc_tty_frm[MC_TTY_FRM_RIGHTTOP] = mc_skin_lines_load_frm (mc_skin, "righttop");
    mc_tty_frm[MC_TTY_FRM_LEFTBOTTOM] = mc_skin_lines_load_frm (mc_skin, "leftbottom");
    mc_tty_frm[MC_TTY_FRM_RIGHTBOTTOM] = mc_skin_lines_load_frm (mc_skin, "rightbottom");
    mc_tty_frm[MC_TTY_FRM_TOPMIDDLE] = mc_skin_lines_load_frm (mc_skin, "topmiddle");
    mc_tty_frm[MC_TTY_FRM_BOTTOMMIDDLE] = mc_skin_lines_load_frm (mc_skin, "bottommiddle");
    mc_tty_frm[MC_TTY_FRM_LEFTMIDDLE] = mc_skin_lines_load_frm (mc_skin, "leftmiddle");
    mc_tty_frm[MC_TTY_FRM_RIGHTMIDDLE] = mc_skin_lines_load_frm (mc_skin, "rightmiddle");
    mc_tty_frm[MC_TTY_FRM_CROSS] = mc_skin_lines_load_frm (mc_skin, "cross");

    /* double lines */
    mc_tty_frm[MC_TTY_FRM_DVERT] = mc_skin_lines_load_frm (mc_skin, "dvert");
    mc_tty_frm[MC_TTY_FRM_DHORIZ] = mc_skin_lines_load_frm (mc_skin, "dhoriz");
    mc_tty_frm[MC_TTY_FRM_DLEFTTOP] = mc_skin_lines_load_frm (mc_skin, "dlefttop");
    mc_tty_frm[MC_TTY_FRM_DRIGHTTOP] = mc_skin_lines_load_frm (mc_skin, "drighttop");
    mc_tty_frm[MC_TTY_FRM_DLEFTBOTTOM] = mc_skin_lines_load_frm (mc_skin, "dleftbottom");
    mc_tty_frm[MC_TTY_FRM_DRIGHTBOTTOM] = mc_skin_lines_load_frm (mc_skin, "drightbottom");
    mc_tty_frm[MC_TTY_FRM_DTOPMIDDLE] = mc_skin_lines_load_frm (mc_skin, "dtopmiddle");
    mc_tty_frm[MC_TTY_FRM_DBOTTOMMIDDLE] = mc_skin_lines_load_frm (mc_skin, "dbottommiddle");
    mc_tty_frm[MC_TTY_FRM_DLEFTMIDDLE] = mc_skin_lines_load_frm (mc_skin, "dleftmiddle");
    mc_tty_frm[MC_TTY_FRM_DRIGHTMIDDLE] = mc_skin_lines_load_frm (mc_skin, "drightmiddle");
}

/* --------------------------------------------------------------------------------------------- */
