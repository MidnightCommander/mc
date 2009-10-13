/*
   Skins engine.
   Work with colors

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
#include <string.h>

#include "../src/global.h"
#include "../src/args.h"
#include "../src/tty/color.h"
#include "skin.h"
#include "internal.h"

/*** global variables ****************************************************************************/

int mc_skin_color__cache[MC_SKIN_COLOR_CACHE_COUNT];

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

static mc_skin_color_t *
mc_skin_color_get_from_hash (mc_skin_t * mc_skin, const gchar * group, const gchar * key)
{
    gchar key_name[BUF_TINY];
    mc_skin_color_t *mc_skin_color;

    if (group == NULL || key == NULL)
        return NULL;

    if (mc_skin == NULL)
        mc_skin = &mc_skin__default;

    g_snprintf (key_name, sizeof (key_name), "%s.%s", group, key);
    mc_skin_color = (mc_skin_color_t *) g_hash_table_lookup (mc_skin->colors, (gpointer) key_name);

    return mc_skin_color;
}

/* --------------------------------------------------------------------------------------------- */

#if 0
static void
mc_skin_color_remove_from_hash (mc_skin_t * mc_skin, const gchar * group, const gchar * key)
{
    gchar key_name[BUF_TINY];
    if (group == NULL || key == NULL)
        return;

    if (mc_skin == NULL)
        mc_skin = &mc_skin__default;

    g_snprintf (key_name, sizeof (key_name), "%s.%s", group, key);
    g_hash_table_remove (mc_skin->colors, (gpointer) key_name);
}
#endif
/* --------------------------------------------------------------------------------------------- */

static void
mc_skin_color_add_to_hash (mc_skin_t * mc_skin, const gchar * group, const gchar * key,
                           mc_skin_color_t * mc_skin_color)
{
    gchar *key_name;

    key_name = g_strdup_printf ("%s.%s", group, key);
    if (key_name == NULL)
        return;

    if (g_hash_table_lookup (mc_skin->colors, (gpointer) key_name) != NULL)
        g_hash_table_remove (mc_skin->colors, (gpointer) key_name);

    g_hash_table_insert (mc_skin->colors, (gpointer) key_name, (gpointer) mc_skin_color);
}

/* --------------------------------------------------------------------------------------------- */

static mc_skin_color_t *
mc_skin_color_get_with_defaults (const gchar * group, const gchar * name)
{
    mc_skin_color_t *mc_skin_color;

    mc_skin_color = mc_skin_color_get_from_hash (NULL, group, name);
    if (mc_skin_color != NULL)
        return mc_skin_color;

    mc_skin_color = mc_skin_color_get_from_hash (NULL, group, "_default_");
    if (mc_skin_color != NULL)
        return mc_skin_color;

    mc_skin_color = mc_skin_color_get_from_hash (NULL, "core", "_default_");
    return mc_skin_color;
}

/* --------------------------------------------------------------------------------------------- */

static mc_skin_color_t *
mc_skin_color_get_from_ini_file (mc_skin_t * mc_skin, const gchar * group, const gchar * key)
{
    gsize items_count;
    gchar **values;
    mc_skin_color_t *mc_skin_color, *tmp;

    values = mc_config_get_string_list (mc_skin->config, group, key, &items_count);

    if (values == NULL || *values == NULL) {
        if (values != NULL)
            g_strfreev (values);
        return NULL;
    }
    mc_skin_color = g_new0 (mc_skin_color_t, 1);
    if (mc_skin_color == NULL) {
        g_strfreev (values);
        return NULL;
    }

    switch (items_count) {
    case 0:
        tmp = mc_skin_color_get_with_defaults (group, "_default_");
        if (tmp) {
            mc_skin_color->fgcolor = g_strdup (tmp->fgcolor);
            mc_skin_color->bgcolor = g_strdup (tmp->bgcolor);
        } else {
            g_strfreev (values);
            g_free (mc_skin_color);
            return NULL;
        }
        break;
    case 1:
        mc_skin_color->fgcolor = (values[0]) ? g_strstrip (g_strdup (values[0])) : NULL;
        tmp = mc_skin_color_get_with_defaults (group, "_default_");
        mc_skin_color->bgcolor = (tmp != NULL) ? g_strdup (tmp->bgcolor) : NULL;
        break;
    case 2:
        mc_skin_color->fgcolor = (values[0]) ? g_strstrip (g_strdup (values[0])) : NULL;
        mc_skin_color->bgcolor = (values[1]) ? g_strstrip (g_strdup (values[1])) : NULL;
        break;
    }
    g_strfreev (values);

    mc_skin_color->pair_index =
        tty_try_alloc_color_pair2 (mc_skin_color->fgcolor, mc_skin_color->bgcolor, FALSE);

    return mc_skin_color;
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_skin_color_set_default_for_terminal (mc_skin_t * mc_skin)
{
    mc_skin_color_t *mc_skin_color;
    mc_skin_color = g_new0 (mc_skin_color_t, 1);
    if (mc_skin_color == NULL)
        return;

    mc_skin_color->fgcolor = g_strdup ("default");
    mc_skin_color->bgcolor = g_strdup ("default");
    mc_skin_color->pair_index =
        tty_try_alloc_color_pair2 (mc_skin_color->fgcolor, mc_skin_color->bgcolor, FALSE);
    mc_skin_color_add_to_hash (mc_skin, "skin", "terminal_default_color", mc_skin_color);
}

/* --------------------------------------------------------------------------------------------- */
static void
mc_skin_color_cache_init (void)
{
    DEFAULT_COLOR = mc_skin_color_get ("skin", "terminal_default_color");
    NORMAL_COLOR = mc_skin_color_get ("core", "_default_");
    MARKED_COLOR = mc_skin_color_get ("core", "marked");
    SELECTED_COLOR = mc_skin_color_get ("core", "selected");
    MARKED_SELECTED_COLOR = mc_skin_color_get ("core", "markselect");
    REVERSE_COLOR = mc_skin_color_get ("core", "reverse");

    COLOR_NORMAL = mc_skin_color_get ("dialog", "_default_");
    COLOR_FOCUS = mc_skin_color_get ("dialog", "dfocus");
    COLOR_HOT_NORMAL = mc_skin_color_get ("dialog", "dhotnormal");
    COLOR_HOT_FOCUS = mc_skin_color_get ("dialog", "dhotfocus");

    ERROR_COLOR = mc_skin_color_get ("error", "_default_");
    ERROR_HOT_NORMAL = mc_skin_color_get ("error", "errdhotnormal");
    ERROR_HOT_FOCUS = mc_skin_color_get ("error", "errdhotfocus");

    MENU_ENTRY_COLOR = mc_skin_color_get ("menu", "_default_");
    MENU_SELECTED_COLOR = mc_skin_color_get ("menu", "menusel");
    MENU_HOT_COLOR = mc_skin_color_get ("menu", "menuhot");
    MENU_HOTSEL_COLOR = mc_skin_color_get ("menu", "menuhotsel");

    GAUGE_COLOR = mc_skin_color_get ("core", "gauge");
    INPUT_COLOR = mc_skin_color_get ("core", "input");

    HELP_NORMAL_COLOR = mc_skin_color_get ("help", "_default_");
    HELP_ITALIC_COLOR = mc_skin_color_get ("help", "helpitalic");
    HELP_BOLD_COLOR = mc_skin_color_get ("help", "helpbold");
    HELP_LINK_COLOR = mc_skin_color_get ("help", "helplink");
    HELP_SLINK_COLOR = mc_skin_color_get ("help", "helpslink");

    VIEW_UNDERLINED_COLOR = mc_skin_color_get ("viewer", "viewunderline");

    EDITOR_NORMAL_COLOR = mc_skin_color_get ("editor", "_default_");
    EDITOR_BOLD_COLOR = mc_skin_color_get ("editor", "editbold");
    EDITOR_MARKED_COLOR = mc_skin_color_get ("editor", "editmarked");
    EDITOR_WHITESPACE_COLOR = mc_skin_color_get ("editor", "editwhitespace");
    LINE_STATE_COLOR = mc_skin_color_get ("editor", "linestate");
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mc_skin_color_check_inisection (const gchar * group)
{
    return !((strcasecmp ("skin", group) == 0)
        || (strcasecmp ("lines", group) == 0));
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_skin_color_check_bw_mode (mc_skin_t * mc_skin)
{
    gsize items_count;
    gchar **groups, **orig_groups;

    if (!mc_args__disable_colors)
        return;

    orig_groups = groups = mc_config_get_groups (mc_skin->config, &items_count);

    if (groups == NULL)
        return;

    for (; *groups != NULL; groups++) {
        if (mc_skin_color_check_inisection (*groups))
            mc_config_del_group (mc_skin->config, *groups);
    }
    g_strfreev (orig_groups);
    mc_skin_hardcoded_blackwhite_colors (mc_skin);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
mc_skin_color_parse_ini_file (mc_skin_t * mc_skin)
{
    gsize items_count;
    gchar **groups, **orig_groups;
    gchar **keys, **orig_keys;
    mc_skin_color_t *mc_skin_color;

    mc_skin_color_check_bw_mode (mc_skin);

    orig_groups = groups = mc_config_get_groups (mc_skin->config, &items_count);

    if (groups == NULL || *groups == NULL) {
        if (groups != NULL)
            g_strfreev (groups);
        return FALSE;
    }

    /* as first, need to set up default colors */
    mc_skin_color_set_default_for_terminal (mc_skin);
    mc_skin_color = mc_skin_color_get_from_ini_file (mc_skin, "core", "_default_");
    if (mc_skin_color == NULL)
        return FALSE;

    tty_color_set_defaults (mc_skin_color->fgcolor, mc_skin_color->bgcolor);
    mc_skin_color_add_to_hash (mc_skin, "core", "_default_", mc_skin_color);

    for (; *groups != NULL; groups++) {
        if (!mc_skin_color_check_inisection (*groups))
            continue;

        orig_keys = keys = mc_config_get_keys (mc_skin->config, *groups, &items_count);
        if (keys == NULL || *keys == NULL) {
            if (keys != NULL)
                g_strfreev (keys);
            continue;
        }

        for (; *keys; keys++) {
            mc_skin_color = mc_skin_color_get_from_ini_file (mc_skin, *groups, *keys);
            if (mc_skin_color != NULL)
                mc_skin_color_add_to_hash (mc_skin, *groups, *keys, mc_skin_color);
        }
        g_strfreev (orig_keys);
    }
    g_strfreev (orig_groups);

    mc_skin_color_cache_init ();
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

int
mc_skin_color_get (const gchar * group, const gchar * name)
{
    mc_skin_color_t *mc_skin_color;

    mc_skin_color = mc_skin_color_get_with_defaults (group, name);

    return (mc_skin_color != NULL) ? mc_skin_color->pair_index : 0;
}

/* --------------------------------------------------------------------------------------------- */
