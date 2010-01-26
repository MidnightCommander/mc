/*
   Skins engine.
   Work with line draving chars.

   Copyright (C) 2009 The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2009.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA.
 */

#include <config.h>
#include <stdlib.h>

#include "internal.h"
#include "lib/tty/tty.h"

#include "src/args.h"



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

    switch (ret) {
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

    return ret;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mc_skin_lines_parse_ini_file (mc_skin_t * mc_skin)
{
    if (mc_args__slow_terminal) {
        mc_skin_hardcoded_space_lines (mc_skin);
    } else if (mc_args__ugly_line_drawing) {
        mc_skin_hardcoded_ugly_lines (mc_skin);
    }

    mc_tty_ugly_frm[MC_TTY_FRM_horiz] = mc_skin_lines_load_frm (mc_skin, "horiz");
    mc_tty_ugly_frm[MC_TTY_FRM_vert] = mc_skin_lines_load_frm (mc_skin, "vert");
    mc_tty_ugly_frm[MC_TTY_FRM_lefttop] = mc_skin_lines_load_frm (mc_skin, "lefttop");
    mc_tty_ugly_frm[MC_TTY_FRM_righttop] = mc_skin_lines_load_frm (mc_skin, "righttop");
    mc_tty_ugly_frm[MC_TTY_FRM_leftbottom] = mc_skin_lines_load_frm (mc_skin, "leftbottom");
    mc_tty_ugly_frm[MC_TTY_FRM_rightbottom] = mc_skin_lines_load_frm (mc_skin, "rightbottom");
    mc_tty_ugly_frm[MC_TTY_FRM_thinvert] = mc_skin_lines_load_frm (mc_skin, "thinvert");
    mc_tty_ugly_frm[MC_TTY_FRM_thinhoriz] = mc_skin_lines_load_frm (mc_skin, "thinhoriz");
    mc_tty_ugly_frm[MC_TTY_FRM_rightmiddle] = mc_skin_lines_load_frm (mc_skin, "rightmiddle");
    mc_tty_ugly_frm[MC_TTY_FRM_centertop] = mc_skin_lines_load_frm (mc_skin, "centertop");
    mc_tty_ugly_frm[MC_TTY_FRM_centerbottom] = mc_skin_lines_load_frm (mc_skin, "centerbottom");
    mc_tty_ugly_frm[MC_TTY_FRM_centermiddle] = mc_skin_lines_load_frm (mc_skin, "centermiddle");
    mc_tty_ugly_frm[MC_TTY_FRM_leftmiddle] = mc_skin_lines_load_frm (mc_skin, "leftmiddle");

}

/* --------------------------------------------------------------------------------------------- */
