/*
   Skins engine.
   Work with colors - backward compability

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
#include <sys/types.h>          /* size_t */
#include "../src/tty/color.h"

#include "../src/global.h"
#include "../src/setup.h"
#include "skin.h"
#include "internal.h"


/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

typedef struct mc_skin_colors_old_struct {
    const char *old_color;
    const char *group;
    const char *key;
} mc_skin_colors_old_t;

/*** file scope variables ************************************************************************/

mc_skin_colors_old_t old_colors[] = {
    {"normal", "core", "_default_"},
    {"selected", "ore", "selected"},
    {"marked", "core", "marked"},
    {"markselect", "core", "markselect"},
    {"errors", "error", "_default_"},
    {"menu", "menu", "_default_"},
    {"reverse", "core", "reverse"},
    {"dnormal", "dialog", "_default_"},
    {"dfocus", "dialog", "dfocus"},
    {"dhotnormal", "dialog", "dhotnormal"},
    {"dhotfocus", "dialog", "dhotfocus"},
    {"viewunderline", "viewer", "viewunderline"},
    {"menuhot", "menu", "menuhot"},
    {"menusel", "menu", "menusel"},
    {"menuhotsel", "menu", "menuhotsel"},
    {"helpnormal", "help", "_default_"},
    {"helpitalic", "help", "helpitalic"},
    {"helpbold", "help", "helpbold"},
    {"helplink", "help", "helplink"},
    {"helpslink", "help", "helpslink"},
    {"gauge", "core", "gauge"},
    {"input", "core", "input"},
    {"editnormal", "editor", "_default_"},
    {"editbold", "editor", "editbold"},
    {"editmarked", "editor", "editmarked"},
    {"editwhitespace", "editor", "editwhitespace"},
    {"editlinestate", "editor", "linestate"},
    {"errdhotnormal", "error", "errdhotnormal"},
    {"errdhotfocus", "error", "errdhotfocus"},
    {NULL, NULL, NULL}
};


/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static gboolean
mc_skin_colors_old_transform (const char *old_color, const char **group, const char **key)
{
    int index;

    if (old_color != NULL)
        for (index = 0; old_colors[index].old_color; index++) {
            if (strcasecmp (old_color, old_colors[index].old_color) == 0) {
                if (group != NULL)
                    *group = old_colors[index].group;
                if (key != NULL)
                    *key = old_colors[index].key;
                return TRUE;
            }
        }
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_skin_colors_old_configure_one (mc_skin_t * mc_skin, const char *the_color_string)
{
    gchar **colors, **orig_colors;
    gchar **key_val;
    const gchar *skin_group, *skin_key;
    gchar *skin_val;

    if (the_color_string == NULL)
        return;

    orig_colors = colors = g_strsplit (the_color_string, ":", -1);
    if (colors == NULL)
        return;

    for (; *colors; colors++) {
        key_val = g_strsplit_set (*colors, "=,", 3);

        if (!key_val)
            continue;

        if (key_val[1] == NULL
            || !mc_skin_colors_old_transform (key_val[0], &skin_group, &skin_key)) {
            g_strfreev (key_val);
            continue;
        }
        if (key_val[2] != NULL)
            skin_val = g_strdup_printf ("%s;%s", key_val[1], key_val[2]);
        else
            skin_val = g_strdup_printf ("%s;", key_val[1]);
        mc_config_set_string (mc_skin->config, skin_group, skin_key, skin_val);

        g_free (skin_val);

        g_strfreev (key_val);
    }
    g_strfreev (orig_colors);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mc_skin_colors_old_configure (mc_skin_t * mc_skin)
{
    mc_skin_colors_old_configure_one (mc_skin, setup_color_string);
    mc_skin_colors_old_configure_one (mc_skin, term_color_string);
    mc_skin_colors_old_configure_one (mc_skin, getenv ("MC_COLOR_TABLE"));
    mc_skin_colors_old_configure_one (mc_skin, command_line_colors);
}

/* --------------------------------------------------------------------------------------------- */
