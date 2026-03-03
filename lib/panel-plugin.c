/*
   Panel plugin registry.

   Copyright (C) 2025
   Free Software Foundation, Inc.

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

/** \file panel-plugin.c
 *  \brief Source: panel plugin registry
 *
 *  Maintains a list of registered mc_panel_plugin_t descriptors.
 *  Plugins are registered via mc_panel_plugin_add() — typically called
 *  from the dynamic loader (panel-plugin-loader.c).
 */

#include <config.h>

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/editor-plugin.h"
#include "lib/panel-plugin.h"

#include "src/filemanager/dir.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static GSList *panel_plugin_registry = NULL;
static GSList *editor_plugin_registry = NULL;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static gboolean
mc_pp_dir_list_grow (dir_list *list, int delta)
{
    int size;

    if (list == NULL || delta == 0)
        return (list != NULL);

    size = list->size + delta;
    if (size <= 0)
        size = 128;

    if (size != list->size)
    {
        file_entry_t *fe;

        fe = g_try_renew (file_entry_t, list->list, size);
        if (fe == NULL)
            return FALSE;

        list->list = fe;
        list->size = size;
    }

    list->len = MIN (list->len, list->size);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mc_pp_dir_list_append (dir_list *list, const char *fname, const struct stat *st)
{
    file_entry_t *fentry;

    if (list->len == list->size && !mc_pp_dir_list_grow (list, 128))
        return FALSE;

    fentry = &list->list[list->len];
    fentry->fname = g_string_new (fname);
    fentry->f.marked = 0;
    fentry->f.link_to_dir = S_ISDIR (st->st_mode) ? 1 : 0;
    fentry->f.stale_link = 0;
    fentry->f.dir_size_computed = 0;
    fentry->st = *st;
    fentry->name_sort_key = NULL;
    fentry->extension_sort_key = NULL;

    list->len++;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mc_pp_add_entry (void *list, const char *name, mode_t mode, off_t size, time_t mtime)
{
    struct stat st;

    memset (&st, 0, sizeof (st));
    st.st_mode = mode;
    st.st_size = size;
    st.st_mtime = mtime;
    st.st_uid = getuid ();
    st.st_gid = getgid ();
    st.st_nlink = 1;

    (void) mc_pp_dir_list_append ((dir_list *) list, name, &st);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_panel_plugin_add (const mc_panel_plugin_t *plugin)
{
    if (plugin == NULL)
        return FALSE;

    if (plugin->api_version != MC_PANEL_PLUGIN_API_VERSION)
    {
        fprintf (stderr, "Panel plugin \"%s\": API version %d, expected %d\n",
                 plugin->name != NULL ? plugin->name : "(null)", plugin->api_version,
                 MC_PANEL_PLUGIN_API_VERSION);
        return FALSE;
    }

    if (plugin->name == NULL || plugin->open == NULL || plugin->close == NULL
        || plugin->get_items == NULL)
    {
        fprintf (stderr, "Panel plugin \"%s\": missing required callbacks\n",
                 plugin->name != NULL ? plugin->name : "(null)");
        return FALSE;
    }

    // check for duplicate name
    if (mc_panel_plugin_find_by_name (plugin->name) != NULL)
    {
        fprintf (stderr, "Panel plugin \"%s\": already registered\n", plugin->name);
        return FALSE;
    }

    panel_plugin_registry = g_slist_append (panel_plugin_registry, (gpointer) plugin);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

const GSList *
mc_panel_plugin_list (void)
{
    return panel_plugin_registry;
}

/* --------------------------------------------------------------------------------------------- */

const mc_panel_plugin_t *
mc_panel_plugin_find_by_name (const char *name)
{
    const GSList *iter;

    if (name == NULL)
        return NULL;

    for (iter = panel_plugin_registry; iter != NULL; iter = g_slist_next (iter))
    {
        const mc_panel_plugin_t *p = (const mc_panel_plugin_t *) iter->data;

        if (strcmp (p->name, name) == 0)
            return p;
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

const mc_panel_plugin_t *
mc_panel_plugin_find_by_prefix (const char *prefix)
{
    const GSList *iter;

    if (prefix == NULL)
        return NULL;

    for (iter = panel_plugin_registry; iter != NULL; iter = g_slist_next (iter))
    {
        const mc_panel_plugin_t *p = (const mc_panel_plugin_t *) iter->data;

        if (p->prefix != NULL && strcmp (p->prefix, prefix) == 0)
            return p;
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

void
mc_panel_plugins_shutdown (void)
{
    g_slist_free (panel_plugin_registry);
    panel_plugin_registry = NULL;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mc_editor_plugin_add (const mc_editor_plugin_t *plugin)
{
    if (plugin == NULL)
        return FALSE;

    if (plugin->api_version != MC_EDITOR_PLUGIN_API_VERSION)
    {
        fprintf (stderr, "Editor plugin \"%s\": API version %d, expected %d\n",
                 plugin->name != NULL ? plugin->name : "(null)", plugin->api_version,
                 MC_EDITOR_PLUGIN_API_VERSION);
        return FALSE;
    }

    if (plugin->name == NULL || plugin->open == NULL || plugin->close == NULL)
    {
        fprintf (stderr, "Editor plugin \"%s\": missing required callbacks\n",
                 plugin->name != NULL ? plugin->name : "(null)");
        return FALSE;
    }

    if (mc_editor_plugin_find_by_name (plugin->name) != NULL)
    {
        fprintf (stderr, "Editor plugin \"%s\": already registered\n", plugin->name);
        return FALSE;
    }

    editor_plugin_registry = g_slist_append (editor_plugin_registry, (gpointer) plugin);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

const GSList *
mc_editor_plugin_list (void)
{
    return editor_plugin_registry;
}

/* --------------------------------------------------------------------------------------------- */

const mc_editor_plugin_t *
mc_editor_plugin_find_by_name (const char *name)
{
    const GSList *iter;

    if (name == NULL)
        return NULL;

    for (iter = editor_plugin_registry; iter != NULL; iter = g_slist_next (iter))
    {
        const mc_editor_plugin_t *p = (const mc_editor_plugin_t *) iter->data;

        if (strcmp (p->name, name) == 0)
            return p;
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

void
mc_editor_plugins_shutdown (void)
{
    g_slist_free (editor_plugin_registry);
    editor_plugin_registry = NULL;
}

/* --------------------------------------------------------------------------------------------- */
