/*
   Skins engine.
   Set of hardcoded skins

   Copyright (C) 2009-2025
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2009
   Andrew Borodin <aborodin@vmail.ru>, 2012

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

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define set_lines(x, y) mc_config_set_string (mc_skin->config, "lines", x, y)

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mc_skin_hardcoded_blackwhite_colors (mc_skin_t *mc_skin)
{
    mc_config_set_string (mc_skin->config, "core", "_default_", "default;default");
    mc_config_set_string (mc_skin->config, "core", "selected", ";;reverse");
    mc_config_set_string (mc_skin->config, "core", "marked", ";;bold");
    mc_config_set_string (mc_skin->config, "core", "markselect", ";;reverse+bold");
    mc_config_set_string (mc_skin->config, "core", "reverse", ";;reverse");
    mc_config_set_string (mc_skin->config, "dialog", "_default_", ";;reverse");
    mc_config_set_string (mc_skin->config, "dialog", "dfocus", ";;bold");
    mc_config_set_string (mc_skin->config, "dialog", "dhotnormal", ";;none");
    mc_config_set_string (mc_skin->config, "dialog", "dhotfocus", ";;bold");
    mc_config_set_string (mc_skin->config, "dialog", "dselnormal", ";;none");
    mc_config_set_string (mc_skin->config, "dialog", "dselfocus", ";;bold");
    mc_config_set_string (mc_skin->config, "error", "_default_", ";;bold");
    mc_config_set_string (mc_skin->config, "error", "errdfocus", ";;reverse+bold");
    mc_config_set_string (mc_skin->config, "error", "errdhotnormal", ";;reverse");
    mc_config_set_string (mc_skin->config, "error", "errdhotfocus", ";;reverse+bold");
    mc_config_set_string (mc_skin->config, "menu", "_default_", ";;reverse");
    mc_config_set_string (mc_skin->config, "menu", "menuhot", ";;none");
    mc_config_set_string (mc_skin->config, "menu", "menusel", ";;none");
    mc_config_set_string (mc_skin->config, "menu", "menuhotsel", ";;bold");
    mc_config_set_string (mc_skin->config, "menu", "menuinactive", ";;reverse");
    mc_config_set_string (mc_skin->config, "popupmenu", "_default_", ";;reverse");
    mc_config_set_string (mc_skin->config, "popupmenu", "menusel", ";;none");
    mc_config_set_string (mc_skin->config, "popupmenu", "menutitle", ";;reverse");
    mc_config_set_string (mc_skin->config, "statusbar", "_default_", ";;reverse");
    mc_config_set_string (mc_skin->config, "help", "_default_", ";;reverse");
    mc_config_set_string (mc_skin->config, "help", "helpitalic", ";;reverse+bold");
    mc_config_set_string (mc_skin->config, "help", "helpbold", ";;reverse+bold");
    mc_config_set_string (mc_skin->config, "help", "helplink", ";;reverse+bold");
    mc_config_set_string (mc_skin->config, "help", "helpslink", ";;bold");
    mc_config_set_string (mc_skin->config, "viewer", "viewbold", ";;bold");
    mc_config_set_string (mc_skin->config, "viewer", "viewunderline", ";;bold");
    mc_config_set_string (mc_skin->config, "editor", "editbold", ";;bold");
    mc_config_set_string (mc_skin->config, "editor", "editmarked", ";;reverse");
    mc_config_set_string (mc_skin->config, "editor", "editframeactive", ";;bold");
    mc_config_set_string (mc_skin->config, "editor", "editframedrag", ";;reverse");
    mc_config_set_string (mc_skin->config, "buttonbar", "button", ";;reverse");
}

/* --------------------------------------------------------------------------------------------- */

void
mc_skin_hardcoded_space_lines (mc_skin_t *mc_skin)
{
    // single lines
    set_lines ("vert", " ");
    set_lines ("horiz", " ");
    set_lines ("lefttop", " ");
    set_lines ("righttop", " ");
    set_lines ("leftbottom", " ");
    set_lines ("rightbottom", " ");
    set_lines ("topmiddle", " ");
    set_lines ("bottommiddle", " ");
    set_lines ("leftmiddle", " ");
    set_lines ("rightmiddle", " ");
    set_lines ("cross", " ");

    set_lines ("dvert", " ");
    set_lines ("dhoriz", " ");
    set_lines ("dlefttop", " ");
    set_lines ("drighttop", " ");
    set_lines ("dleftbottom", " ");
    set_lines ("drightbottom", " ");
    set_lines ("dtopmiddle", " ");
    set_lines ("dbottommiddle", " ");
    set_lines ("dleftmiddle", " ");
    set_lines ("drightmiddle", " ");
}

/* --------------------------------------------------------------------------------------------- */

void
mc_skin_hardcoded_ugly_lines (mc_skin_t *mc_skin)
{
    // single lines
    set_lines ("vert", "|");
    set_lines ("horiz", "-");
    set_lines ("lefttop", "+");
    set_lines ("righttop", "+");
    set_lines ("leftbottom", "+");
    set_lines ("rightbottom", "+");
    set_lines ("topmiddle", "-");
    set_lines ("bottommiddle", "-");
    set_lines ("leftmiddle", "|");
    set_lines ("rightmiddle", "|");
    set_lines ("cross", "+");

    // double lines
    set_lines ("dvert", "|");
    set_lines ("dhoriz", "-");
    set_lines ("dlefttop", "+");
    set_lines ("drighttop", "+");
    set_lines ("dleftbottom", "+");
    set_lines ("drightbottom", "+");
    set_lines ("dtopmiddle", "-");
    set_lines ("dbottommiddle", "-");
    set_lines ("dleftmiddle", "|");
    set_lines ("drightmiddle", "|");
}

/* --------------------------------------------------------------------------------------------- */
