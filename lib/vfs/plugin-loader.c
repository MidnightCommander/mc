/*
   Dynamic VFS plugin loader.

   Copyright (C) 2011-2025
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

/** \file
 *  \brief Dynamic VFS plugin loader
 *
 *  Scans MC_VFS_PLUGINS_DIR for shared objects exporting mc_vfs_plugin_init(),
 *  loads them, and calls the init function so the plugin can register its
 *  vfs_class via vfs_register_class().
 */

#include <config.h>

#include "lib/global.h"
#include "lib/vfs/vfs.h"

#ifdef HAVE_GMODULE

#include <gmodule.h>
#include <stdio.h>

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static GPtrArray *dynamic_modules = NULL;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
vfs_plugins_load_dynamic (void)
{
    GDir *dir;
    const gchar *filename;
    gchar *plugins_dir;

    plugins_dir = g_build_filename (MC_VFS_PLUGINS_DIR, (char *) NULL);
    dir = g_dir_open (plugins_dir, 0, NULL);
    if (dir == NULL)
    {
        g_free (plugins_dir);
        return;  // no plugins dir -- OK, nothing to load
    }

    dynamic_modules = g_ptr_array_new ();

    while ((filename = g_dir_read_name (dir)) != NULL)
    {
        GModule *module;
        mc_vfs_plugin_init_fn init_fn;
        gchar *path;

        if (!g_str_has_suffix (filename, "." G_MODULE_SUFFIX))
            continue;

        path = g_build_filename (plugins_dir, filename, (char *) NULL);
        module = g_module_open (path, G_MODULE_BIND_LAZY);
        if (module == NULL)
        {
            fprintf (stderr, "VFS plugin %s: %s\n", filename, g_module_error ());
            g_free (path);
            continue;
        }

        if (!g_module_symbol (module, MC_VFS_PLUGIN_ENTRY, (gpointer *) &init_fn))
        {
            fprintf (stderr, "VFS plugin %s: symbol %s not found\n",
                     filename, MC_VFS_PLUGIN_ENTRY);
            g_module_close (module);
            g_free (path);
            continue;
        }

        init_fn ();
        g_module_make_resident (module);  // prevent unload -- vfs_class lives in .so
        g_ptr_array_add (dynamic_modules, module);
        g_free (path);
    }

    g_dir_close (dir);
    g_free (plugins_dir);
}

/* --------------------------------------------------------------------------------------------- */

void
vfs_plugins_unload_dynamic (void)
{
    if (dynamic_modules != NULL)
    {
        g_ptr_array_free (dynamic_modules, TRUE);
        dynamic_modules = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */

#else /* !HAVE_GMODULE */

void
vfs_plugins_load_dynamic (void)
{
    // GModule not available -- dynamic VFS plugins disabled
}

/* --------------------------------------------------------------------------------------------- */

void
vfs_plugins_unload_dynamic (void)
{
    // nothing to do
}

/* --------------------------------------------------------------------------------------------- */

#endif /* HAVE_GMODULE */
