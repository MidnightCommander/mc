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

#include "lib/global.h"
#include "lib/panel-plugin.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static GSList *panel_plugin_registry = NULL;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
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
