/*
   Skins engine.
   Set of hardcoded skins

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
#include "../src/global.h"

#include "skin.h"
#include "internal.h"


/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define set_lines(x,y) mc_config_set_string(mc_skin->config, "Lines", x, y)

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mc_skin_hardcoded_blackwhite_colors (mc_skin_t * mc_skin)
{
    mc_config_set_string (mc_skin->config, "core", "_default_", "default;default");
    mc_config_set_string (mc_skin->config, "core", "selected", "A_REVERSE");
    mc_config_set_string (mc_skin->config, "core", "marked", "A_BOLD");
    mc_config_set_string (mc_skin->config, "core", "markselect", "A_BOLD_REVERSE");
    mc_config_set_string (mc_skin->config, "core", "reverse", "A_REVERSE");
    mc_config_set_string (mc_skin->config, "dialog", "_default_", "A_REVERSE");
    mc_config_set_string (mc_skin->config, "dialog", "dfocus", "A_BOLD");
    mc_config_set_string (mc_skin->config, "dialog", "dhotnormal", "A_UNDERLINE");
    mc_config_set_string (mc_skin->config, "dialog", "dhotfocus", "A_UNDERLINE");
    mc_config_set_string (mc_skin->config, "error", "_default_", "A_BOLD");
    mc_config_set_string (mc_skin->config, "menu", "_default_", "default");
    mc_config_set_string (mc_skin->config, "menu", "menuhot", "A_UNDERLINE");
    mc_config_set_string (mc_skin->config, "menu", "menusel", "A_REVERSE");
    mc_config_set_string (mc_skin->config, "menu", "menuhotsel", "A_UNDERLINE");
    mc_config_set_string (mc_skin->config, "help", "_default_", "A_REVERSE");
    mc_config_set_string (mc_skin->config, "help", "helpitalic", "A_REVERSE");
    mc_config_set_string (mc_skin->config, "help", "helpbold", "A_REVERSE");
    mc_config_set_string (mc_skin->config, "help", "helpslink", "A_BOLD");
    mc_config_set_string (mc_skin->config, "viewer", "viewunderline", "A_UNDERLINE");
    mc_config_set_string (mc_skin->config, "editor", "editbold", "A_BOLD");
    mc_config_set_string (mc_skin->config, "editor", "editmarked", "A_REVERSE");
}

/* --------------------------------------------------------------------------------------------- */

void
mc_skin_hardcoded_space_lines (mc_skin_t * mc_skin)
{
    set_lines ("lefttop", " ");
    set_lines ("righttop", " ");
    set_lines ("centertop", " ");
    set_lines ("centerbottom", " ");
    set_lines ("leftbottom", " ");
    set_lines ("rightbottom", " ");
    set_lines ("leftmiddle", " ");
    set_lines ("rightmiddle", " ");
    set_lines ("centermiddle", " ");
    set_lines ("horiz", " ");
    set_lines ("vert", " ");
    set_lines ("thinhoriz", " ");
    set_lines ("thinvert", " ");
}

/* --------------------------------------------------------------------------------------------- */

void
mc_skin_hardcoded_ugly_lines (mc_skin_t * mc_skin)
{
    set_lines ("lefttop", "+");
    set_lines ("righttop", "+");
    set_lines ("centertop", "-");
    set_lines ("centerbottom", "-");
    set_lines ("leftbottom", "+");
    set_lines ("rightbottom", "+");
    set_lines ("leftmiddle", "|");
    set_lines ("rightmiddle", "|");
    set_lines ("centermiddle", "+");
    set_lines ("horiz", "-");
    set_lines ("vert", "|");
    set_lines ("thinhoriz", "-");
    set_lines ("thinvert", "|");
}

/* --------------------------------------------------------------------------------------------- */
