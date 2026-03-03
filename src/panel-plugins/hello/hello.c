/*
   Hello World panel plugin — demonstrates the panel plugin API.

   Copyright (C) 2025
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2026.

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
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/panel-plugin.h"

/*** file scope type declarations ****************************************************************/

typedef struct
{
    mc_panel_host_t *host;
    gboolean in_subdir;  /* TRUE when user entered "subdir" */
    GHashTable *deleted; /* set of "deleted" filenames */
} hello_data_t;

/*** forward declarations (file scope functions) *************************************************/

static void *hello_open (mc_panel_host_t *host, const char *open_path);
static void hello_close (void *plugin_data);
static mc_pp_result_t hello_get_items (void *plugin_data, void *list_ptr);
static mc_pp_result_t hello_chdir (void *plugin_data, const char *path);
static mc_pp_result_t hello_get_local_copy (void *plugin_data, const char *fname,
                                            char **local_path);
static mc_pp_result_t hello_delete_items (void *plugin_data, const char **names, int count);
static const char *hello_get_title (void *plugin_data);

/*** file scope variables ************************************************************************/

static const mc_panel_plugin_t hello_plugin = {
    .api_version = MC_PANEL_PLUGIN_API_VERSION,
    .name = "hello",
    .display_name = "Hello World Plugin",
    .proto = "HelloWorld",
    .prefix = NULL,
    .flags = MC_PPF_NAVIGATE | MC_PPF_GET_FILES | MC_PPF_DELETE | MC_PPF_CUSTOM_TITLE,

    .open = hello_open,
    .close = hello_close,
    .get_items = hello_get_items,

    .chdir = hello_chdir,
    .enter = NULL,
    .get_local_copy = hello_get_local_copy,
    .put_file = NULL,
    .save_file = NULL,
    .delete_items = hello_delete_items,
    .get_title = hello_get_title,
    .handle_key = NULL,
};

/*** file scope functions ************************************************************************/

static void *
hello_open (mc_panel_host_t *host, const char *open_path)
{
    hello_data_t *data;

    (void) open_path;

    data = g_new0 (hello_data_t, 1);
    data->host = host;
    data->in_subdir = FALSE;
    data->deleted = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

    return data;
}

/* --------------------------------------------------------------------------------------------- */

static void
hello_close (void *plugin_data)
{
    hello_data_t *data = (hello_data_t *) plugin_data;

    g_hash_table_destroy (data->deleted);
    g_free (data);
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
hello_get_items (void *plugin_data, void *list_ptr)
{
    hello_data_t *data = (hello_data_t *) plugin_data;

    /* Note: dir_list_init() already creates the ".." entry at index 0,
       so the plugin must NOT add it again. */

    if (data->in_subdir)
    {
        if (!g_hash_table_contains (data->deleted, "deep-file.txt"))
            mc_pp_add_entry (list_ptr, "deep-file.txt", S_IFREG | 0644, 256, time (NULL));
        if (!g_hash_table_contains (data->deleted, "another.dat"))
            mc_pp_add_entry (list_ptr, "another.dat", S_IFREG | 0644, 1024, time (NULL));
    }
    else
    {
        if (!g_hash_table_contains (data->deleted, "hello.txt"))
            mc_pp_add_entry (list_ptr, "hello.txt", S_IFREG | 0644, 42, time (NULL));
        if (!g_hash_table_contains (data->deleted, "world.txt"))
            mc_pp_add_entry (list_ptr, "world.txt", S_IFREG | 0644, 100, time (NULL));
        mc_pp_add_entry (list_ptr, "subdir", S_IFDIR | 0755, 4096, time (NULL));
    }

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
hello_chdir (void *plugin_data, const char *path)
{
    hello_data_t *data = (hello_data_t *) plugin_data;

    if (strcmp (path, "..") == 0)
    {
        if (data->in_subdir)
        {
            data->in_subdir = FALSE;
            return MC_PPR_OK;
        }
        return MC_PPR_CLOSE; /* close plugin */
    }

    if (strcmp (path, "subdir") == 0 && !data->in_subdir)
    {
        data->in_subdir = TRUE;
        return MC_PPR_OK;
    }

    return MC_PPR_FAILED;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
hello_get_local_copy (void *plugin_data, const char *fname, char **local_path)
{
    GError *error = NULL;
    int fd;
    const char *content;

    (void) plugin_data;

    if (strcmp (fname, "hello.txt") == 0)
        content = "Hello, World!\nThis is content from the Hello World panel plugin.\n";
    else if (strcmp (fname, "world.txt") == 0)
        content = "This is world.txt\nAnother virtual file from the plugin.\n";
    else if (strcmp (fname, "deep-file.txt") == 0)
        content = "Deep inside the subdir.\n";
    else if (strcmp (fname, "another.dat") == 0)
        content = "Binary-ish data from the plugin.\n";
    else
        return MC_PPR_FAILED;

    fd = g_file_open_tmp ("mc-pp-XXXXXX", local_path, &error);
    if (fd == -1)
    {
        g_error_free (error);
        return MC_PPR_FAILED;
    }

    if (write (fd, content, strlen (content)) == -1)
    {
        close (fd);
        unlink (*local_path);
        g_free (*local_path);
        *local_path = NULL;
        return MC_PPR_FAILED;
    }
    close (fd);
    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
hello_delete_items (void *plugin_data, const char **names, int count)
{
    hello_data_t *data = (hello_data_t *) plugin_data;
    int i;

    for (i = 0; i < count; i++)
        g_hash_table_add (data->deleted, g_strdup (names[i]));

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
hello_get_title (void *plugin_data)
{
    hello_data_t *data = (hello_data_t *) plugin_data;

    return data->in_subdir ? "/subdir" : "/";
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* Entry point called by the panel plugin loader */
const mc_panel_plugin_t *mc_panel_plugin_register (void);

const mc_panel_plugin_t *
mc_panel_plugin_register (void)
{
    return &hello_plugin;
}

/* --------------------------------------------------------------------------------------------- */
