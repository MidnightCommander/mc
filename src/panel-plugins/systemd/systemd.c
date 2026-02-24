/*
   Systemd unit browser panel plugin.

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

#include "src/filemanager/dir.h"

/*** file scope type declarations ****************************************************************/

typedef enum
{
    UNIT_STATE_ACTIVE,
    UNIT_STATE_INACTIVE,
    UNIT_STATE_FAILED
} unit_state_t;

typedef struct
{
    char *name;
    unit_state_t state;
} systemd_unit_t;

typedef struct
{
    mc_panel_host_t *host;
    char *current_type;         /* NULL = root, "services" = inside type dir */
    GPtrArray *units;           /* array of systemd_unit_t* */
    char *title_buf;            /* owned buffer for get_title() return */
} systemd_data_t;

/*** file scope variables ************************************************************************/

static const struct
{
    const char *dir_name;
    const char *type_arg;
} unit_type_dirs[] = {
    { "services", "service" },
    { "timers", "timer" },
    { "sockets", "socket" },
    { "targets", "target" },
    { "mounts", "mount" },
    { "slices", "slice" },
    { "paths", "path" },
    { "scopes", "scope" },
    { "automounts", "automount" },
    { "swaps", "swap" },
};

static const size_t unit_type_dirs_count = G_N_ELEMENTS (unit_type_dirs);

/*** forward declarations (file scope functions) *************************************************/

static void *systemd_open (mc_panel_host_t *host, const char *open_path);
static void systemd_close (void *plugin_data);
static mc_pp_result_t systemd_get_items (void *plugin_data, void *list_ptr);
static mc_pp_result_t systemd_chdir (void *plugin_data, const char *path);
static mc_pp_result_t systemd_enter (void *plugin_data, const char *name, const struct stat *st);
static mc_pp_result_t systemd_get_local_copy (void *plugin_data, const char *fname,
                                              char **local_path);
static mc_pp_result_t systemd_delete_items (void *plugin_data, const char **names, int count);
static const char *systemd_get_title (void *plugin_data);

static const mc_panel_plugin_t systemd_plugin = {
    .api_version = MC_PANEL_PLUGIN_API_VERSION,
    .name = "systemd",
    .display_name = "Systemd units",
    .proto = "systemd",
    .prefix = NULL,
    .flags = MC_PPF_NAVIGATE | MC_PPF_GET_FILES | MC_PPF_DELETE | MC_PPF_CUSTOM_TITLE,

    .open = systemd_open,
    .close = systemd_close,
    .get_items = systemd_get_items,

    .chdir = systemd_chdir,
    .enter = systemd_enter,
    .get_local_copy = systemd_get_local_copy,
    .delete_items = systemd_delete_items,
    .get_title = systemd_get_title,
    .handle_key = NULL,
};

/*** file scope functions ************************************************************************/

static void
add_entry (dir_list *list, const char *name, mode_t mode, off_t size)
{
    struct stat st;

    memset (&st, 0, sizeof (st));
    st.st_mode = mode;
    st.st_size = size;
    st.st_mtime = time (NULL);
    st.st_uid = getuid ();
    st.st_gid = getgid ();
    st.st_nlink = 1;

    dir_list_append (list, name, &st, S_ISDIR (mode), FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static mode_t
state_to_mode (unit_state_t state)
{
    switch (state)
    {
    case UNIT_STATE_ACTIVE:
        return S_IFREG | 0755;
    case UNIT_STATE_FAILED:
        return S_IFREG | 0000;
    case UNIT_STATE_INACTIVE:
    default:
        return S_IFREG | 0644;
    }
}

/* --------------------------------------------------------------------------------------------- */

static const char *
state_to_prefix (unit_state_t state)
{
    switch (state)
    {
    case UNIT_STATE_ACTIVE:
        return "[ACTIVE] ";
    case UNIT_STATE_FAILED:
        return "[FAILED] ";
    case UNIT_STATE_INACTIVE:
    default:
        return "[INACTIVE] ";
    }
}

/* --------------------------------------------------------------------------------------------- */

/* Strip "[STATE] " prefix from display name to get real unit name */
static const char *
strip_state_prefix (const char *display_name)
{
    const char *p;

    if (display_name[0] != '[')
        return display_name;

    p = strchr (display_name, ']');
    if (p == NULL)
        return display_name;

    p++;
    while (*p == ' ')
        p++;

    return p;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
systemd_run_cmd (const char *cmd, char **output)
{
    gchar *std_out = NULL;
    gchar *std_err = NULL;
    gint exit_status = 0;
    GError *error = NULL;
    gboolean ok;

    ok = g_spawn_command_line_sync (cmd, &std_out, &std_err, &exit_status, &error);

    g_free (std_err);

    if (!ok)
    {
        if (error != NULL)
            g_error_free (error);
        g_free (std_out);
        if (output != NULL)
            *output = NULL;
        return FALSE;
    }

    if (output != NULL)
        *output = std_out;
    else
        g_free (std_out);

    return (exit_status == 0);
}

/* --------------------------------------------------------------------------------------------- */

static void
systemd_unit_free (gpointer p)
{
    systemd_unit_t *u = (systemd_unit_t *) p;

    g_free (u->name);
    g_free (u);
}

/* --------------------------------------------------------------------------------------------- */

static GPtrArray *
systemd_load_units (const char *type_arg)
{
    GPtrArray *arr;
    char *cmd;
    char *output = NULL;
    char **lines;
    int i;

    arr = g_ptr_array_new_with_free_func (systemd_unit_free);

    cmd = g_strdup_printf ("systemctl list-units --type=%s --all --no-legend --no-pager", type_arg);

    if (!systemd_run_cmd (cmd, &output) || output == NULL)
    {
        g_free (cmd);
        g_free (output);
        return arr;
    }
    g_free (cmd);

    lines = g_strsplit (output, "\n", -1);
    g_free (output);

    for (i = 0; lines[i] != NULL; i++)
    {
        systemd_unit_t *unit;
        char *line;
        char **tokens;
        int t;
        /* parsed fields */
        const char *unit_name = NULL;
        const char *active_state = NULL;
        int field_idx;

        line = g_strstrip (lines[i]);
        if (line[0] == '\0')
            continue;

        /* skip leading bullet character (● or *) */
        if ((guchar) line[0] > 127 || line[0] == '*')
        {
            /* ● is a multi-byte UTF-8 sequence; skip it */
            if ((guchar) line[0] >= 0xC0)
            {
                /* skip full UTF-8 character */
                if ((guchar) line[0] >= 0xF0)
                    line += 4;
                else if ((guchar) line[0] >= 0xE0)
                    line += 3;
                else
                    line += 2;
            }
            else
                line++;

            line = g_strstrip (line);
        }

        /* tokenize: unit_name load_state active_state sub_state description... */
        tokens = g_strsplit_set (line, " \t", -1);

        /* collect non-empty tokens */
        field_idx = 0;
        for (t = 0; tokens[t] != NULL && field_idx < 3; t++)
        {
            if (tokens[t][0] == '\0')
                continue;

            switch (field_idx)
            {
            case 0:
                unit_name = tokens[t];
                break;
            case 2:
                active_state = tokens[t];
                break;
            default:
                break;
            }
            field_idx++;
        }

        if (unit_name == NULL || active_state == NULL)
        {
            g_strfreev (tokens);
            continue;
        }

        unit = g_new (systemd_unit_t, 1);
        unit->name = g_strdup (unit_name);

        if (strcmp (active_state, "active") == 0)
            unit->state = UNIT_STATE_ACTIVE;
        else if (strcmp (active_state, "failed") == 0)
            unit->state = UNIT_STATE_FAILED;
        else
            unit->state = UNIT_STATE_INACTIVE;

        g_ptr_array_add (arr, unit);
        g_strfreev (tokens);
    }

    g_strfreev (lines);
    return arr;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
find_type_arg (const char *dir_name)
{
    size_t i;

    for (i = 0; i < unit_type_dirs_count; i++)
        if (strcmp (unit_type_dirs[i].dir_name, dir_name) == 0)
            return unit_type_dirs[i].type_arg;

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static const systemd_unit_t *
find_unit (const systemd_data_t *data, const char *display_name)
{
    guint i;
    const char *real_name;

    if (data->units == NULL)
        return NULL;

    real_name = strip_state_prefix (display_name);

    for (i = 0; i < data->units->len; i++)
    {
        const systemd_unit_t *u = (const systemd_unit_t *) g_ptr_array_index (data->units, i);

        if (strcmp (u->name, real_name) == 0)
            return u;
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
reload_units (systemd_data_t *data)
{
    const char *type_arg;

    if (data->current_type == NULL)
        return;

    type_arg = find_type_arg (data->current_type);
    if (type_arg == NULL)
        return;

    if (data->units != NULL)
        g_ptr_array_free (data->units, TRUE);

    data->units = systemd_load_units (type_arg);
}

/* --------------------------------------------------------------------------------------------- */

static void *
systemd_open (mc_panel_host_t *host, const char *open_path)
{
    systemd_data_t *data;

    (void) open_path;

    data = g_new0 (systemd_data_t, 1);
    data->host = host;
    data->current_type = NULL;
    data->units = NULL;
    data->title_buf = NULL;

    return data;
}

/* --------------------------------------------------------------------------------------------- */

static void
systemd_close (void *plugin_data)
{
    systemd_data_t *data = (systemd_data_t *) plugin_data;

    g_free (data->current_type);
    g_free (data->title_buf);

    if (data->units != NULL)
        g_ptr_array_free (data->units, TRUE);

    g_free (data);
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
systemd_get_items (void *plugin_data, void *list_ptr)
{
    dir_list *list = (dir_list *) list_ptr;
    systemd_data_t *data = (systemd_data_t *) plugin_data;

    if (data->current_type == NULL)
    {
        /* root view: show type directories */
        size_t i;

        for (i = 0; i < unit_type_dirs_count; i++)
            add_entry (list, unit_type_dirs[i].dir_name, S_IFDIR | 0755, 4096);
    }
    else
    {
        /* inside a type dir: show units */
        guint i;

        if (data->units != NULL)
        {
            for (i = 0; i < data->units->len; i++)
            {
                const systemd_unit_t *u =
                    (const systemd_unit_t *) g_ptr_array_index (data->units, i);
                char *display_name;

                display_name =
                    g_strdup_printf ("%s%s", state_to_prefix (u->state), u->name);
                add_entry (list, display_name, state_to_mode (u->state), 0);
                g_free (display_name);
            }
        }
    }

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
systemd_chdir (void *plugin_data, const char *path)
{
    systemd_data_t *data = (systemd_data_t *) plugin_data;

    if (strcmp (path, "..") == 0)
    {
        if (data->current_type != NULL)
        {
            /* go back to root */
            g_free (data->current_type);
            data->current_type = NULL;

            if (data->units != NULL)
            {
                g_ptr_array_free (data->units, TRUE);
                data->units = NULL;
            }

            return MC_PPR_OK;
        }

        /* already at root — close plugin */
        return MC_PPR_NOT_SUPPORTED;
    }

    /* entering a type directory */
    if (data->current_type == NULL)
    {
        const char *type_arg;

        type_arg = find_type_arg (path);
        if (type_arg == NULL)
            return MC_PPR_FAILED;

        data->current_type = g_strdup (path);
        data->units = systemd_load_units (type_arg);

        return MC_PPR_OK;
    }

    return MC_PPR_FAILED;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
systemd_enter (void *plugin_data, const char *name, const struct stat *st)
{
    systemd_data_t *data = (systemd_data_t *) plugin_data;
    const systemd_unit_t *u;
    char *cmd;

    (void) st;

    if (data->current_type == NULL)
        return MC_PPR_NOT_SUPPORTED;   /* directories handled by chdir */

    u = find_unit (data, name);
    if (u == NULL)
        return MC_PPR_FAILED;

    if (u->state == UNIT_STATE_ACTIVE)
        cmd = g_strdup_printf ("systemctl restart %s", u->name);
    else
        cmd = g_strdup_printf ("systemctl start %s", u->name);

    systemd_run_cmd (cmd, NULL);
    g_free (cmd);

    reload_units (data);

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
systemd_get_local_copy (void *plugin_data, const char *fname, char **local_path)
{
    systemd_data_t *data = (systemd_data_t *) plugin_data;
    char *cmd;
    char *output = NULL;
    GError *error = NULL;
    int fd;

    if (data->current_type == NULL)
        return MC_PPR_FAILED;

    cmd = g_strdup_printf ("systemctl status %s --no-pager", strip_state_prefix (fname));
    systemd_run_cmd (cmd, &output);
    g_free (cmd);

    if (output == NULL)
        output = g_strdup ("(no status output)\n");

    fd = g_file_open_tmp ("mc-pp-systemd-XXXXXX", local_path, &error);
    if (fd == -1)
    {
        if (error != NULL)
            g_error_free (error);
        g_free (output);
        return MC_PPR_FAILED;
    }

    if (write (fd, output, strlen (output)) == -1)
    {
        close (fd);
        unlink (*local_path);
        g_free (*local_path);
        *local_path = NULL;
        g_free (output);
        return MC_PPR_FAILED;
    }

    close (fd);
    g_free (output);

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
systemd_delete_items (void *plugin_data, const char **names, int count)
{
    systemd_data_t *data = (systemd_data_t *) plugin_data;
    int i;

    if (data->current_type == NULL)
        return MC_PPR_FAILED;

    for (i = 0; i < count; i++)
    {
        char *cmd;

        cmd = g_strdup_printf ("systemctl stop %s", strip_state_prefix (names[i]));
        systemd_run_cmd (cmd, NULL);
        g_free (cmd);
    }

    reload_units (data);

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
systemd_get_title (void *plugin_data)
{
    systemd_data_t *data = (systemd_data_t *) plugin_data;

    g_free (data->title_buf);

    if (data->current_type != NULL)
        data->title_buf = g_strdup_printf ("/%s", data->current_type);
    else
        data->title_buf = g_strdup ("/");

    return data->title_buf;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* Entry point called by the panel plugin loader */
const mc_panel_plugin_t *mc_panel_plugin_register (void);

const mc_panel_plugin_t *
mc_panel_plugin_register (void)
{
    return &systemd_plugin;
}

/* --------------------------------------------------------------------------------------------- */
