/*
   Skins engine.
   Work with colors

   Copyright (C) 2009-2026
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

typedef struct
{
    int role;
    const char *group;
    const char *key;
} color_keyword_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static const color_keyword_t color_keywords[] = {
    { CORE_DEFAULT_COLOR, "skin", "terminal_default_color" },
    { CORE_NORMAL_COLOR, "core", "_default_" },
    { CORE_MARKED_COLOR, "core", "marked" },
    { CORE_SELECTED_COLOR, "core", "selected" },
    { CORE_MARKED_SELECTED_COLOR, "core", "markselect" },
    { CORE_DISABLED_COLOR, "core", "disabled" },
    { CORE_REVERSE_COLOR, "core", "reverse" },
    { CORE_HEADER_COLOR, "core", "header" },
    { CORE_HINTBAR_COLOR, "core", "hintbar" },
    { CORE_SHELLPROMPT_COLOR, "core", "shellprompt" },
    { CORE_COMMANDLINE_COLOR, "core", "commandline" },
    { CORE_COMMANDLINE_MARK_COLOR, "core", "commandlinemark" },
    { CORE_SHADOW_COLOR, "core", "shadow" },
    { CORE_FRAME_COLOR, "core", "frame" },

    { DIALOG_NORMAL_COLOR, "dialog", "_default_" },
    { DIALOG_FOCUS_COLOR, "dialog", "dfocus" },
    { DIALOG_HOT_NORMAL_COLOR, "dialog", "dhotnormal" },
    { DIALOG_HOT_FOCUS_COLOR, "dialog", "dhotfocus" },
    { DIALOG_SELECTED_NORMAL_COLOR, "dialog", "dselnormal" },
    { DIALOG_SELECTED_FOCUS_COLOR, "dialog", "dselfocus" },
    { DIALOG_TITLE_COLOR, "dialog", "dtitle" },
    { DIALOG_FRAME_COLOR, "dialog", "dframe" },

    { ERROR_NORMAL_COLOR, "error", "_default_" },
    { ERROR_FOCUS_COLOR, "error", "errdfocus" },
    { ERROR_HOT_NORMAL_COLOR, "error", "errdhotnormal" },
    { ERROR_HOT_FOCUS_COLOR, "error", "errdhotfocus" },
    { ERROR_TITLE_COLOR, "error", "errdtitle" },
    { ERROR_FRAME_COLOR, "error", "errdframe" },

    { FILEHIGHLIGHT_DEFAULT_COLOR, "filehighlight", "_default_" },

    { MENU_ENTRY_COLOR, "menu", "_default_" },
    { MENU_SELECTED_COLOR, "menu", "menusel" },
    { MENU_HOT_COLOR, "menu", "menuhot" },
    { MENU_HOTSEL_COLOR, "menu", "menuhotsel" },
    { MENU_INACTIVE_COLOR, "menu", "menuinactive" },
    { MENU_FRAME_COLOR, "menu", "menuframe" },

    { PMENU_ENTRY_COLOR, "popupmenu", "_default_" },
    { PMENU_SELECTED_COLOR, "popupmenu", "menusel" },
    { PMENU_TITLE_COLOR, "popupmenu", "menutitle" },
    { PMENU_FRAME_COLOR, "popupmenu", "menuframe" },

    { BUTTONBAR_HOTKEY_COLOR, "buttonbar", "hotkey" },
    { BUTTONBAR_BUTTON_COLOR, "buttonbar", "button" },

    { STATUSBAR_COLOR, "statusbar", "_default_" },

    { CORE_GAUGE_COLOR, "core", "gauge" },
    { CORE_INPUT_COLOR, "core", "input" },
    { CORE_INPUT_HISTORY_COLOR, "core", "inputhistory" },
    { CORE_COMMAND_HISTORY_COLOR, "core", "commandhistory" },
    { CORE_INPUT_MARK_COLOR, "core", "inputmark" },
    { CORE_INPUT_UNCHANGED_COLOR, "core", "inputunchanged" },

    { HELP_NORMAL_COLOR, "help", "_default_" },
    { HELP_ITALIC_COLOR, "help", "helpitalic" },
    { HELP_BOLD_COLOR, "help", "helpbold" },
    { HELP_LINK_COLOR, "help", "helplink" },
    { HELP_SLINK_COLOR, "help", "helpslink" },
    { HELP_TITLE_COLOR, "help", "helptitle" },
    { HELP_FRAME_COLOR, "help", "helpframe" },

    { VIEWER_NORMAL_COLOR, "viewer", "_default_" },
    { VIEWER_BOLD_COLOR, "viewer", "viewbold" },
    { VIEWER_UNDERLINED_COLOR, "viewer", "viewunderline" },
    { VIEWER_BOLD_UNDERLINED_COLOR, "viewer", "viewboldunderline" },
    { VIEWER_SELECTED_COLOR, "viewer", "viewselected" },
    { VIEWER_FRAME_COLOR, "viewer", "viewframe" },

    { EDITOR_NORMAL_COLOR, "editor", "_default_" },
    { EDITOR_BOLD_COLOR, "editor", "editbold" },
    { EDITOR_MARKED_COLOR, "editor", "editmarked" },
    { EDITOR_WHITESPACE_COLOR, "editor", "editwhitespace" },
    { EDITOR_NONPRINTABLE_COLOR, "editor", "editnonprintable" },
    { EDITOR_RIGHT_MARGIN_COLOR, "editor", "editrightmargin" },
    { EDITOR_LINE_STATE_COLOR, "editor", "editlinestate" },
    { EDITOR_BACKGROUND_COLOR, "editor", "editbg" },
    { EDITOR_FRAME_COLOR, "editor", "editframe" },
    { EDITOR_FRAME_ACTIVE_COLOR, "editor", "editframeactive" },
    { EDITOR_FRAME_DRAG_COLOR, "editor", "editframedrag" },

    { EDITOR_BOOKMARK_COLOR, "editor", "bookmark" },
    { EDITOR_BOOKMARK_FOUND_COLOR, "editor", "bookmarkfound" },

    { DIFFVIEWER_ADDED_COLOR, "diffviewer", "added" },
    { DIFFVIEWER_CHANGEDLINE_COLOR, "diffviewer", "changedline" },
    { DIFFVIEWER_CHANGEDNEW_COLOR, "diffviewer", "changednew" },
    { DIFFVIEWER_CHANGED_COLOR, "diffviewer", "changed" },
    { DIFFVIEWER_REMOVED_COLOR, "diffviewer", "removed" },
    { DIFFVIEWER_ERROR_COLOR, "diffviewer", "error" },
};

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
mc_skin_color_set_default_for_terminal (void)
{
    tty_color_pair_t *mc_skin_color;

    mc_skin_color = g_try_new0 (tty_color_pair_t, 1);
    if (mc_skin_color != NULL)
    {
        mc_skin_color->fg = g_strdup ("default");
        mc_skin_color->bg = g_strdup ("default");
        mc_skin_color->attrs = NULL;
        mc_skin_color->pair_index = tty_try_alloc_color_pair (mc_skin_color, FALSE);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_skin_color_cache_init (void)
{
    for (size_t i = 0; i < G_N_ELEMENTS (color_keywords); i++)
        tty_color_role_to_pair[color_keywords[i].role - TTY_COLOR_MAP_OFFSET] =
            mc_skin_color_get (color_keywords[i].group, color_keywords[i].key);
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
    mc_skin_color_set_default_for_terminal ();
    mc_skin_color = mc_skin_color_get_from_ini_file (mc_skin, "core", "_default_");
    if (mc_skin_color == NULL)
        goto ret;

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
