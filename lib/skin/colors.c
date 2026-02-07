/*
   Skins engine.
   Work with colors

   Copyright (C) 2009-2025
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2009
   Egmont Koblinger <egmont@gmail.com>, 2010
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
#include <string.h>

#include "internal.h"

#include "lib/tty/color.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static tty_color_pair_t *
mc_skin_color_get_from_hash (mc_skin_t *mc_skin, const gchar *group, const gchar *key)
{
    gchar kname[BUF_TINY];
    tty_color_pair_t *mc_skin_color;

    if (group == NULL || key == NULL)
        return NULL;

    if (mc_skin == NULL)
        mc_skin = &mc_skin__default;

    g_snprintf (kname, sizeof (kname), "%s.%s", group, key);
    mc_skin_color = (tty_color_pair_t *) g_hash_table_lookup (mc_skin->colors, (gpointer) kname);

    return mc_skin_color;
}

/* --------------------------------------------------------------------------------------------- */

#if 0
static void
mc_skin_color_remove_from_hash (mc_skin_t *mc_skin, const gchar *group, const gchar *key)
{
    gchar kname[BUF_TINY];
    if (group == NULL || key == NULL)
        return;

    if (mc_skin == NULL)
        mc_skin = &mc_skin__default;

    g_snprintf (kname, sizeof (kname), "%s.%s", group, key);
    g_hash_table_remove (mc_skin->colors, (gpointer) kname);
}
#endif

/* --------------------------------------------------------------------------------------------- */

static void
mc_skin_color_add_to_hash (mc_skin_t *mc_skin, const gchar *group, const gchar *key,
                           tty_color_pair_t *mc_skin_color)
{
    gchar *kname;

    kname = g_strdup_printf ("%s.%s", group, key);
    if (kname != NULL)
    {
        if (g_hash_table_lookup (mc_skin->colors, (gpointer) kname) != NULL)
            g_hash_table_remove (mc_skin->colors, (gpointer) kname);

        g_hash_table_insert (mc_skin->colors, (gpointer) kname, (gpointer) mc_skin_color);
    }
}

/* --------------------------------------------------------------------------------------------- */

static tty_color_pair_t *
mc_skin_color_get_with_defaults (const gchar *group, const gchar *name)
{
    tty_color_pair_t *mc_skin_color;

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

/* If an alias is found, alloc a new string for the resolved value and free the input parameter.
   Otherwise it's a no-op returning the original string. */
static gchar *
mc_skin_color_look_up_alias (mc_skin_t *mc_skin, gchar *str)
{
    gchar *orig, *str2;
    int hop = 0;

    orig = g_strdup (str);
    str2 = g_strdup (str);

    while (TRUE)
    {
        gchar **values;
        gsize items_count;

        values = mc_config_get_string_list (mc_skin->config, "aliases", str, &items_count);
        if (items_count != 1)
        {
            // No such alias declaration found, that is, we've got the resolved value.
            g_strfreev (values);
            g_free (str2);
            g_free (orig);
            return str;
        }

        g_free (str);
        str = g_strdup (values[0]);
        g_strfreev (values);

        // str2 resolves at half speed than str. This is used for loop detection.
        if (hop++ % 2 != 0)
        {
            values = mc_config_get_string_list (mc_skin->config, "aliases", str2, &items_count);
            g_assert (items_count == 1);
            g_free (str2);
            str2 = g_strdup (values[0]);
            g_strfreev (values);

            if (strcmp (str, str2) == 0)
            {
                // Loop detected.
                fprintf (stderr,
                         "Loop detected while trying to resolve alias \"%s\" in skin \"%s\"\n",
                         orig, mc_skin->name);
                g_free (str);
                g_free (str2);
                return orig;
            }
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static tty_color_pair_t *
mc_skin_color_get_from_ini_file (mc_skin_t *mc_skin, const gchar *group, const gchar *key)
{
    gsize items_count;
    gchar **values;
    tty_color_pair_t *mc_skin_color, *tmp;

    values = mc_config_get_string_list (mc_skin->config, group, key, &items_count);
    if (values == NULL || values[0] == NULL)
    {
        g_strfreev (values);
        return NULL;
    }

    mc_skin_color = g_try_new0 (tty_color_pair_t, 1);
    if (mc_skin_color == NULL)
    {
        g_strfreev (values);
        return NULL;
    }

    tmp = mc_skin_color_get_with_defaults (group, "_default_");
    mc_skin_color->fg = (items_count > 0 && values[0][0])
        ? mc_skin_color_look_up_alias (mc_skin, g_strstrip (g_strdup (values[0])))
        : (tmp != NULL) ? g_strdup (tmp->fg)
                        : NULL;
    mc_skin_color->bg = (items_count > 1 && values[1][0])
        ? mc_skin_color_look_up_alias (mc_skin, g_strstrip (g_strdup (values[1])))
        : (tmp != NULL) ? g_strdup (tmp->bg)
                        : NULL;
    mc_skin_color->attrs = (items_count > 2 && values[2][0])
        ? mc_skin_color_look_up_alias (mc_skin, g_strstrip (g_strdup (values[2])))
        : (tmp != NULL) ? g_strdup (tmp->attrs)
                        : NULL;

    g_strfreev (values);

    mc_skin_color->pair_index = tty_try_alloc_color_pair (mc_skin_color, FALSE);

    return mc_skin_color;
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_skin_color_set_default_for_terminal (mc_skin_t *mc_skin)
{
    tty_color_pair_t *mc_skin_color;

    mc_skin_color = g_try_new0 (tty_color_pair_t, 1);
    if (mc_skin_color != NULL)
    {
        mc_skin_color->fg = g_strdup ("default");
        mc_skin_color->bg = g_strdup ("default");
        mc_skin_color->attrs = NULL;
        mc_skin_color->pair_index = tty_try_alloc_color_pair (mc_skin_color, FALSE);
        mc_skin_color_add_to_hash (mc_skin, "skin", "terminal_default_color", mc_skin_color);
    }
}

/* --------------------------------------------------------------------------------------------- */

/*
 * Helper for mc_skin_color_cache_init ()
 */
static inline void
mc_skin_one_color_init (int color_role, const char *group, const char *key)
{
    tty_color_role_to_pair[color_role - COLOR_MAP_OFFSET] = mc_skin_color_get (group, key);
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_skin_color_cache_init (void)
{
    mc_skin_one_color_init (CORE_DEFAULT_COLOR, "skin", "terminal_default_color");
    mc_skin_one_color_init (CORE_NORMAL_COLOR, "core", "_default_");
    mc_skin_one_color_init (CORE_MARKED_COLOR, "core", "marked");
    mc_skin_one_color_init (CORE_SELECTED_COLOR, "core", "selected");
    mc_skin_one_color_init (CORE_MARKED_SELECTED_COLOR, "core", "markselect");
    mc_skin_one_color_init (CORE_DISABLED_COLOR, "core", "disabled");
    mc_skin_one_color_init (CORE_REVERSE_COLOR, "core", "reverse");
    mc_skin_one_color_init (CORE_HEADER_COLOR, "core", "header");
    mc_skin_one_color_init (CORE_COMMAND_MARK_COLOR, "core", "commandlinemark");
    mc_skin_one_color_init (CORE_SHADOW_COLOR, "core", "shadow");
    mc_skin_one_color_init (CORE_FRAME_COLOR, "core", "frame");

    mc_skin_one_color_init (DIALOG_NORMAL_COLOR, "dialog", "_default_");
    mc_skin_one_color_init (DIALOG_FOCUS_COLOR, "dialog", "dfocus");
    mc_skin_one_color_init (DIALOG_HOT_NORMAL_COLOR, "dialog", "dhotnormal");
    mc_skin_one_color_init (DIALOG_HOT_FOCUS_COLOR, "dialog", "dhotfocus");
    mc_skin_one_color_init (DIALOG_SELECTED_NORMAL_COLOR, "dialog", "dselnormal");
    mc_skin_one_color_init (DIALOG_SELECTED_FOCUS_COLOR, "dialog", "dselfocus");
    mc_skin_one_color_init (DIALOG_TITLE_COLOR, "dialog", "dtitle");
    mc_skin_one_color_init (DIALOG_FRAME_COLOR, "dialog", "dframe");

    mc_skin_one_color_init (ERROR_NORMAL_COLOR, "error", "_default_");
    mc_skin_one_color_init (ERROR_FOCUS_COLOR, "error", "errdfocus");
    mc_skin_one_color_init (ERROR_HOT_NORMAL_COLOR, "error", "errdhotnormal");
    mc_skin_one_color_init (ERROR_HOT_FOCUS_COLOR, "error", "errdhotfocus");
    mc_skin_one_color_init (ERROR_TITLE_COLOR, "error", "errdtitle");
    mc_skin_one_color_init (ERROR_FRAME_COLOR, "error", "errdframe");

    mc_skin_one_color_init (FILEHIGHLIGHT_DEFAULT_COLOR, "filehighlight", "_default_");

    mc_skin_one_color_init (MENU_ENTRY_COLOR, "menu", "_default_");
    mc_skin_one_color_init (MENU_SELECTED_COLOR, "menu", "menusel");
    mc_skin_one_color_init (MENU_HOT_COLOR, "menu", "menuhot");
    mc_skin_one_color_init (MENU_HOTSEL_COLOR, "menu", "menuhotsel");
    mc_skin_one_color_init (MENU_INACTIVE_COLOR, "menu", "menuinactive");
    mc_skin_one_color_init (MENU_FRAME_COLOR, "menu", "menuframe");

    mc_skin_one_color_init (PMENU_ENTRY_COLOR, "popupmenu", "_default_");
    mc_skin_one_color_init (PMENU_SELECTED_COLOR, "popupmenu", "menusel");
    mc_skin_one_color_init (PMENU_TITLE_COLOR, "popupmenu", "menutitle");
    mc_skin_one_color_init (PMENU_FRAME_COLOR, "popupmenu", "menuframe");

    mc_skin_one_color_init (BUTTONBAR_HOTKEY_COLOR, "buttonbar", "hotkey");
    mc_skin_one_color_init (BUTTONBAR_BUTTON_COLOR, "buttonbar", "button");

    mc_skin_one_color_init (STATUSBAR_COLOR, "statusbar", "_default_");

    mc_skin_one_color_init (CORE_GAUGE_COLOR, "core", "gauge");
    mc_skin_one_color_init (CORE_INPUT_COLOR, "core", "input");
    mc_skin_one_color_init (CORE_INPUT_HISTORY_COLOR, "core", "inputhistory");
    mc_skin_one_color_init (CORE_COMMAND_HISTORY_COLOR, "core", "commandhistory");
    mc_skin_one_color_init (CORE_INPUT_MARK_COLOR, "core", "inputmark");
    mc_skin_one_color_init (CORE_INPUT_UNCHANGED_COLOR, "core", "inputunchanged");

    mc_skin_one_color_init (HELP_NORMAL_COLOR, "help", "_default_");
    mc_skin_one_color_init (HELP_ITALIC_COLOR, "help", "helpitalic");
    mc_skin_one_color_init (HELP_BOLD_COLOR, "help", "helpbold");
    mc_skin_one_color_init (HELP_LINK_COLOR, "help", "helplink");
    mc_skin_one_color_init (HELP_SLINK_COLOR, "help", "helpslink");
    mc_skin_one_color_init (HELP_TITLE_COLOR, "help", "helptitle");
    mc_skin_one_color_init (HELP_FRAME_COLOR, "help", "helpframe");

    mc_skin_one_color_init (VIEWER_NORMAL_COLOR, "viewer", "_default_");
    mc_skin_one_color_init (VIEWER_BOLD_COLOR, "viewer", "viewbold");
    mc_skin_one_color_init (VIEWER_UNDERLINED_COLOR, "viewer", "viewunderline");
    mc_skin_one_color_init (VIEWER_BOLD_UNDERLINED_COLOR, "viewer", "viewboldunderline");
    mc_skin_one_color_init (VIEWER_SELECTED_COLOR, "viewer", "viewselected");
    mc_skin_one_color_init (VIEWER_FRAME_COLOR, "viewer", "viewframe");

    mc_skin_one_color_init (EDITOR_NORMAL_COLOR, "editor", "_default_");
    mc_skin_one_color_init (EDITOR_BOLD_COLOR, "editor", "editbold");
    mc_skin_one_color_init (EDITOR_MARKED_COLOR, "editor", "editmarked");
    mc_skin_one_color_init (EDITOR_WHITESPACE_COLOR, "editor", "editwhitespace");
    mc_skin_one_color_init (EDITOR_NONPRINTABLE_COLOR, "editor", "editnonprintable");
    mc_skin_one_color_init (EDITOR_RIGHT_MARGIN_COLOR, "editor", "editrightmargin");
    mc_skin_one_color_init (EDITOR_LINE_STATE_COLOR, "editor", "editlinestate");
    mc_skin_one_color_init (EDITOR_BACKGROUND_COLOR, "editor", "editbg");
    mc_skin_one_color_init (EDITOR_FRAME_COLOR, "editor", "editframe");
    mc_skin_one_color_init (EDITOR_FRAME_ACTIVE_COLOR, "editor", "editframeactive");
    mc_skin_one_color_init (EDITOR_FRAME_DRAG_COLOR, "editor", "editframedrag");

    mc_skin_one_color_init (EDITOR_BOOKMARK_COLOR, "editor", "bookmark");
    mc_skin_one_color_init (EDITOR_BOOKMARK_FOUND_COLOR, "editor", "bookmarkfound");

    mc_skin_one_color_init (DIFFVIEWER_ADDED_COLOR, "diffviewer", "added");
    mc_skin_one_color_init (DIFFVIEWER_CHANGEDLINE_COLOR, "diffviewer", "changedline");
    mc_skin_one_color_init (DIFFVIEWER_CHANGEDNEW_COLOR, "diffviewer", "changednew");
    mc_skin_one_color_init (DIFFVIEWER_CHANGED_COLOR, "diffviewer", "changed");
    mc_skin_one_color_init (DIFFVIEWER_REMOVED_COLOR, "diffviewer", "removed");
    mc_skin_one_color_init (DIFFVIEWER_ERROR_COLOR, "diffviewer", "error");
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mc_skin_color_check_inisection (const gchar *group)
{
    return !((strcasecmp ("skin", group) == 0) || (strcasecmp ("aliases", group) == 0)
             || (strcasecmp ("lines", group) == 0) || (strncasecmp ("widget-", group, 7) == 0));
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_skin_color_check_bw_mode (mc_skin_t *mc_skin)
{
    gchar **groups, **orig_groups;

    if (tty_use_colors () && !mc_global.tty.disable_colors)
        return;

    orig_groups = mc_config_get_groups (mc_skin->config, NULL);

    for (groups = orig_groups; *groups != NULL; groups++)
        if (mc_skin_color_check_inisection (*groups))
            mc_config_del_group (mc_skin->config, *groups);

    g_strfreev (orig_groups);

    mc_skin_hardcoded_blackwhite_colors (mc_skin);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
mc_skin_color_parse_ini_file (mc_skin_t *mc_skin)
{
    gboolean ret = FALSE;
    gsize items_count;
    gchar **groups, **orig_groups;
    tty_color_pair_t *mc_skin_color;

    mc_skin_color_check_bw_mode (mc_skin);

    orig_groups = mc_config_get_groups (mc_skin->config, &items_count);
    if (*orig_groups == NULL)
        goto ret;

    // as first, need to set up default colors
    mc_skin_color_set_default_for_terminal (mc_skin);
    mc_skin_color = mc_skin_color_get_from_ini_file (mc_skin, "core", "_default_");
    if (mc_skin_color == NULL)
        goto ret;

    tty_color_set_defaults (mc_skin_color);
    mc_skin_color_add_to_hash (mc_skin, "core", "_default_", mc_skin_color);

    for (groups = orig_groups; *groups != NULL; groups++)
    {
        gchar **keys, **orig_keys;

        if (!mc_skin_color_check_inisection (*groups))
            continue;

        orig_keys = mc_config_get_keys (mc_skin->config, *groups, NULL);

        for (keys = orig_keys; *keys != NULL; keys++)
        {
            mc_skin_color = mc_skin_color_get_from_ini_file (mc_skin, *groups, *keys);
            if (mc_skin_color != NULL)
                mc_skin_color_add_to_hash (mc_skin, *groups, *keys, mc_skin_color);
        }
        g_strfreev (orig_keys);
    }

    mc_skin_color_cache_init ();

    ret = TRUE;

ret:
    g_strfreev (orig_groups);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

int
mc_skin_color_get (const gchar *group, const gchar *name)
{
    tty_color_pair_t *mc_skin_color;

    mc_skin_color = mc_skin_color_get_with_defaults (group, name);

    return (mc_skin_color != NULL) ? mc_skin_color->pair_index : 0;
}

/* --------------------------------------------------------------------------------------------- */
