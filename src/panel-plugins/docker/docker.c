/*
   Docker resources panel plugin.

   Copyright (C) 2026
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

#include <config.h>

#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/panel-plugin.h"
#include "lib/widget.h"

#include "src/filemanager/dir.h"

/*** file scope type declarations ****************************************************************/

typedef enum
{
    DOCKER_VIEW_ROOT = 0,
    DOCKER_VIEW_CONTAINERS_PROJECTS,
    DOCKER_VIEW_CONTAINERS_ITEMS,
    DOCKER_VIEW_CONTAINER_DETAILS,
    DOCKER_VIEW_IMAGES,
    DOCKER_VIEW_VOLUMES,
    DOCKER_VIEW_NETWORKS
} docker_view_t;

typedef struct
{
    char *name; /* display name */
    char *id;   /* real object id/name used by docker CLI */
    gboolean is_dir;
    off_t size;
} docker_item_t;

typedef struct
{
    mc_panel_host_t *host;

    docker_view_t view;
    GPtrArray *items; /* docker_item_t* for current view */

    char *root_focus;
    char *current_project;
    char *current_container_id;
    char *current_container_name;

    char *title_buf;
} docker_data_t;

/*** forward declarations (file scope functions) *************************************************/

static void *docker_open (mc_panel_host_t *host, const char *open_path);
static void docker_close (void *plugin_data);
static mc_pp_result_t docker_get_items (void *plugin_data, void *list_ptr);
static mc_pp_result_t docker_chdir (void *plugin_data, const char *path);
static mc_pp_result_t docker_enter (void *plugin_data, const char *name, const struct stat *st);
static mc_pp_result_t docker_get_local_copy (void *plugin_data, const char *fname, char **local_path);
static mc_pp_result_t docker_delete_items (void *plugin_data, const char **names, int count);
static const char *docker_get_title (void *plugin_data);
static mc_pp_result_t docker_create_item (void *plugin_data);

/*** file scope variables ************************************************************************/

static const char docker_daemon_info_file[] = "daemon-info.txt";
static const char docker_version_file[] = "version.txt";
static const char docker_inspect_file[] = "inspect.json";
static const char docker_ungrouped_project[] = "ungrouped";

static const mc_panel_plugin_t docker_plugin = {
    .api_version = MC_PANEL_PLUGIN_API_VERSION,
    .name = "docker",
    .display_name = "Docker",
    .proto = "docker",
    .prefix = NULL,
    .flags =
        MC_PPF_NAVIGATE | MC_PPF_GET_FILES | MC_PPF_DELETE | MC_PPF_CUSTOM_TITLE | MC_PPF_CREATE,

    .open = docker_open,
    .close = docker_close,
    .get_items = docker_get_items,

    .chdir = docker_chdir,
    .enter = docker_enter,
    .get_local_copy = docker_get_local_copy,
    .delete_items = docker_delete_items,
    .get_title = docker_get_title,
    .handle_key = NULL,
    .create_item = docker_create_item,
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

static void
docker_item_free (gpointer p)
{
    docker_item_t *item = (docker_item_t *) p;

    g_free (item->name);
    g_free (item->id);
    g_free (item);
}

/* --------------------------------------------------------------------------------------------- */

static off_t
parse_size_to_bytes (const char *text)
{
    char *endptr = NULL;
    double value;
    double mult = 1.0;
    const char *unit;

    if (text == NULL || *text == '\0')
        return 0;

    value = g_ascii_strtod (text, &endptr);
    if (endptr == text)
        return 0;

    while (*endptr == ' ')
        endptr++;
    unit = endptr;

    if (g_ascii_strcasecmp (unit, "B") == 0 || *unit == '\0')
        mult = 1.0;
    else if (g_ascii_strcasecmp (unit, "KB") == 0)
        mult = 1000.0;
    else if (g_ascii_strcasecmp (unit, "MB") == 0)
        mult = 1000.0 * 1000.0;
    else if (g_ascii_strcasecmp (unit, "GB") == 0)
        mult = 1000.0 * 1000.0 * 1000.0;
    else if (g_ascii_strcasecmp (unit, "TB") == 0)
        mult = 1000.0 * 1000.0 * 1000.0 * 1000.0;
    else if (g_ascii_strcasecmp (unit, "KiB") == 0)
        mult = 1024.0;
    else if (g_ascii_strcasecmp (unit, "MiB") == 0)
        mult = 1024.0 * 1024.0;
    else if (g_ascii_strcasecmp (unit, "GiB") == 0)
        mult = 1024.0 * 1024.0 * 1024.0;
    else if (g_ascii_strcasecmp (unit, "TiB") == 0)
        mult = 1024.0 * 1024.0 * 1024.0 * 1024.0;

    return (off_t) (value * mult);
}

/* --------------------------------------------------------------------------------------------- */

static void
clear_items (docker_data_t *data)
{
    if (data->items != NULL)
    {
        g_ptr_array_free (data->items, TRUE);
        data->items = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */

static const docker_item_t *
find_item_by_name (const docker_data_t *data, const char *name)
{
    guint i;

    if (data->items == NULL)
        return NULL;

    for (i = 0; i < data->items->len; i++)
    {
        const docker_item_t *item = (const docker_item_t *) g_ptr_array_index (data->items, i);

        if (strcmp (item->name, name) == 0)
            return item;
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static docker_view_t
view_from_root_path (const char *path)
{
    if (strcmp (path, "containers") == 0)
        return DOCKER_VIEW_CONTAINERS_PROJECTS;
    if (strcmp (path, "images") == 0)
        return DOCKER_VIEW_IMAGES;
    if (strcmp (path, "volumes") == 0)
        return DOCKER_VIEW_VOLUMES;
    if (strcmp (path, "networks") == 0)
        return DOCKER_VIEW_NETWORKS;

    return DOCKER_VIEW_ROOT;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
run_cmd (const char *cmd, char **output, char **err_text)
{
    gchar *std_out = NULL;
    gchar *std_err = NULL;
    gint status = 0;
    GError *error = NULL;
    gboolean spawned;
    gboolean exited_ok;

    spawned = g_spawn_command_line_sync (cmd, &std_out, &std_err, &status, &error);
    if (!spawned)
    {
        if (err_text != NULL)
        {
            if (error != NULL && error->message != NULL)
                *err_text = g_strdup (error->message);
            else
                *err_text = g_strdup (_ ("Failed to start docker command"));
        }

        if (output != NULL)
            *output = NULL;

        if (error != NULL)
            g_error_free (error);
        g_free (std_out);
        g_free (std_err);
        return FALSE;
    }

#if GLIB_CHECK_VERSION(2, 70, 0)
    exited_ok = g_spawn_check_wait_status (status, NULL);
#else
    exited_ok = g_spawn_check_exit_status (status, NULL);
#endif

    if (output != NULL)
        *output = std_out;
    else
        g_free (std_out);

    if (err_text != NULL)
        *err_text = std_err;
    else
        g_free (std_err);

    return exited_ok;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
is_ungrouped_project (const char *project)
{
    return (project == NULL || project[0] == '\0' || strcmp (project, docker_ungrouped_project) == 0);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
project_match (const char *selected_project, const char *project_from_docker)
{
    if (is_ungrouped_project (selected_project))
        return is_ungrouped_project (project_from_docker);

    return (project_from_docker != NULL && strcmp (selected_project, project_from_docker) == 0);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
docker_load_containers_output (char **output, char **err_text)
{
    return run_cmd (
        "docker ps -a --format '{{.ID}}\\t{{.Names}}\\t{{.Image}}\\t{{.Status}}\\t{{.Label \"com.docker.compose.project\"}}'",
        output, err_text);
}

/* --------------------------------------------------------------------------------------------- */

static GPtrArray *
parse_projects_from_containers (const char *output, const char *focused_project)
{
    GPtrArray *items;
    GHashTable *seen;
    char **lines;
    int i;

    items = g_ptr_array_new_with_free_func (docker_item_free);
    seen = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

    if (output == NULL)
        goto done;

    lines = g_strsplit (output, "\n", -1);

    for (i = 0; lines[i] != NULL; i++)
    {
        char **parts;
        int part_count = 0;
        const char *project;
        char *project_key;

        if (lines[i][0] == '\0')
            continue;

        parts = g_strsplit (lines[i], "\t", -1);
        while (parts[part_count] != NULL)
            part_count++;

        if (part_count < 5)
        {
            g_strfreev (parts);
            continue;
        }

        project = parts[4];
        project_key = is_ungrouped_project (project) ? g_strdup (docker_ungrouped_project)
                                                     : g_strdup (project);

        if (!g_hash_table_contains (seen, project_key))
        {
            docker_item_t *item = g_new0 (docker_item_t, 1);

            item->id = g_strdup (project_key);
            item->name = g_strdup (project_key);
            item->is_dir = TRUE;
            item->size = 0;

            if (focused_project != NULL && strcmp (item->id, focused_project) == 0)
                g_ptr_array_insert (items, 0, item);
            else
                g_ptr_array_add (items, item);
            g_hash_table_add (seen, project_key);
        }
        else
            g_free (project_key);

        g_strfreev (parts);
    }

    g_strfreev (lines);

done:
    if (!g_hash_table_contains (seen, docker_ungrouped_project))
    {
        docker_item_t *item = g_new0 (docker_item_t, 1);

        item->id = g_strdup (docker_ungrouped_project);
        item->name = g_strdup (docker_ungrouped_project);
        item->is_dir = TRUE;
        item->size = 0;

        g_ptr_array_add (items, item);
    }

    g_hash_table_destroy (seen);
    return items;
}

/* --------------------------------------------------------------------------------------------- */

static GPtrArray *
parse_container_items_from_project (const char *output, const char *project_name,
                                    const char *focused_container_id)
{
    GPtrArray *items;
    char **lines;
    int i;

    items = g_ptr_array_new_with_free_func (docker_item_free);

    if (output == NULL)
        return items;

    lines = g_strsplit (output, "\n", -1);

    for (i = 0; lines[i] != NULL; i++)
    {
        char **parts;
        int part_count = 0;
        const char *project;

        if (lines[i][0] == '\0')
            continue;

        parts = g_strsplit (lines[i], "\t", -1);
        while (parts[part_count] != NULL)
            part_count++;

        if (part_count < 5)
        {
            g_strfreev (parts);
            continue;
        }

        project = parts[4];
        if (!project_match (project_name, project))
        {
            g_strfreev (parts);
            continue;
        }

        {
            docker_item_t *item = g_new0 (docker_item_t, 1);

            item->id = g_strdup (parts[0]);
            item->name = g_strdup_printf ("%s (%s) %s", parts[1], parts[2], parts[3]);
            item->is_dir = TRUE;
            item->size = 0;

            if (focused_container_id != NULL && strcmp (item->id, focused_container_id) == 0)
                g_ptr_array_insert (items, 0, item);
            else
                g_ptr_array_add (items, item);
        }

        g_strfreev (parts);
    }

    g_strfreev (lines);
    return items;
}

/* --------------------------------------------------------------------------------------------- */

static GPtrArray *
parse_generic_list_output (docker_view_t view, const char *output)
{
    GPtrArray *items;
    char **lines;
    int i;

    items = g_ptr_array_new_with_free_func (docker_item_free);
    if (output == NULL)
        return items;

    lines = g_strsplit (output, "\n", -1);

    for (i = 0; lines[i] != NULL; i++)
    {
        docker_item_t *item;
        char **parts;
        int part_count = 0;

        if (lines[i][0] == '\0')
            continue;

        parts = g_strsplit (lines[i], "\t", -1);
        while (parts[part_count] != NULL)
            part_count++;

        if (part_count < 2)
        {
            g_strfreev (parts);
            continue;
        }

        item = g_new0 (docker_item_t, 1);
        item->is_dir = FALSE;
        item->size = 0;
        item->id = g_strdup (parts[0]);

        switch (view)
        {
        case DOCKER_VIEW_IMAGES:
            item->name = g_strdup (parts[1]);
            if (part_count >= 3)
                item->size = parse_size_to_bytes (parts[2]);
            break;
        case DOCKER_VIEW_VOLUMES:
            if (part_count >= 3)
                item->name = g_strdup_printf ("%s (%s)", parts[1], parts[2]);
            else
                item->name = g_strdup (parts[1]);
            break;
        case DOCKER_VIEW_NETWORKS:
            if (part_count >= 4)
                item->name = g_strdup_printf ("%s (%s/%s)", parts[1], parts[2], parts[3]);
            else
                item->name = g_strdup (parts[1]);
            break;
        default:
            item->name = g_strdup (parts[1]);
            break;
        }

        g_ptr_array_add (items, item);
        g_strfreev (parts);
    }

    g_strfreev (lines);
    return items;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
reload_items (docker_data_t *data)
{
    char *output = NULL;
    char *err_text = NULL;

    clear_items (data);

    switch (data->view)
    {
    case DOCKER_VIEW_ROOT:
        return TRUE;

    case DOCKER_VIEW_CONTAINERS_PROJECTS:
        if (!docker_load_containers_output (&output, &err_text))
            goto cmd_failed;

        data->items = parse_projects_from_containers (output, data->current_project);
        break;

    case DOCKER_VIEW_CONTAINERS_ITEMS:
        if (!docker_load_containers_output (&output, &err_text))
            goto cmd_failed;

        data->items =
            parse_container_items_from_project (output, data->current_project, data->current_container_id);
        break;

    case DOCKER_VIEW_CONTAINER_DETAILS:
    {
        docker_item_t *item;

        data->items = g_ptr_array_new_with_free_func (docker_item_free);

        item = g_new0 (docker_item_t, 1);
        item->id = g_strdup (docker_inspect_file);
        item->name = g_strdup (docker_inspect_file);
        item->is_dir = FALSE;
        item->size = 0;
        g_ptr_array_add (data->items, item);

        break;
    }

    case DOCKER_VIEW_IMAGES:
        if (!run_cmd ("docker images --format '{{.ID}}\\t{{.Repository}}:{{.Tag}}\\t{{.Size}}'", &output,
                      &err_text))
            goto cmd_failed;

        data->items = parse_generic_list_output (data->view, output);
        break;

    case DOCKER_VIEW_VOLUMES:
        if (!run_cmd ("docker volume ls --format '{{.Name}}\\t{{.Name}}\\t{{.Driver}}'", &output,
                      &err_text))
            goto cmd_failed;

        data->items = parse_generic_list_output (data->view, output);
        break;

    case DOCKER_VIEW_NETWORKS:
        if (!run_cmd ("docker network ls --format '{{.ID}}\\t{{.Name}}\\t{{.Driver}}\\t{{.Scope}}'",
                      &output, &err_text))
            goto cmd_failed;

        data->items = parse_generic_list_output (data->view, output);
        break;

    default:
        data->items = g_ptr_array_new_with_free_func (docker_item_free);
        break;
    }

    g_free (output);
    g_free (err_text);

    if (data->items == NULL)
        data->items = g_ptr_array_new_with_free_func (docker_item_free);

    return TRUE;

cmd_failed:
    if (err_text != NULL && err_text[0] != '\0')
        message (D_ERROR, MSG_ERROR, "%s", err_text);

    g_free (output);
    g_free (err_text);

    data->items = g_ptr_array_new_with_free_func (docker_item_free);
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
write_temp_content (const char *prefix, const char *content, char **local_path)
{
    GError *error = NULL;
    int fd;

    fd = g_file_open_tmp (prefix, local_path, &error);
    if (fd == -1)
    {
        if (error != NULL)
            g_error_free (error);
        return FALSE;
    }

    if (content == NULL)
        content = "";

    if (write (fd, content, strlen (content)) == -1)
    {
        close (fd);
        unlink (*local_path);
        g_free (*local_path);
        *local_path = NULL;
        return FALSE;
    }

    close (fd);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
show_create_container_dialog (char **image, char **name, char **command, gboolean *detach)
{
    /* clang-format off */
    quick_widget_t quick_widgets[] = {
        QUICK_LABELED_INPUT (N_("Image:"), input_label_above,
                            *image != NULL ? *image : "", "docker-create-image",
                            image, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
        QUICK_SEPARATOR (FALSE),
        QUICK_LABELED_INPUT (N_("Container name:"), input_label_above,
                            *name != NULL ? *name : "", "docker-create-name",
                            name, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
        QUICK_SEPARATOR (FALSE),
        QUICK_LABELED_INPUT (N_("Command:"), input_label_above,
                            *command != NULL ? *command : "", "docker-create-cmd",
                            command, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
        QUICK_SEPARATOR (FALSE),
        QUICK_CHECKBOX (N_("Run in &detached mode (-d)"), detach, NULL),
        QUICK_BUTTONS_OK_CANCEL,
        QUICK_END,
    };
    /* clang-format on */

    WRect r = { -1, -1, 0, 56 };

    quick_dialog_t qdlg = {
        .rect = r,
        .title = N_ ("Create Docker Container"),
        .help = "[Docker Plugin]",
        .widgets = quick_widgets,
        .callback = NULL,
        .mouse_callback = NULL,
    };

    return (quick_dialog (&qdlg) == B_ENTER);
}

/* --------------------------------------------------------------------------------------------- */

static void
set_view (docker_data_t *data, docker_view_t new_view)
{
    data->view = new_view;
    clear_items (data);
}

/* --------------------------------------------------------------------------------------------- */
/* Plugin callbacks */
/* --------------------------------------------------------------------------------------------- */

static void *
docker_open (mc_panel_host_t *host, const char *open_path)
{
    docker_data_t *data;

    (void) open_path;

    data = g_new0 (docker_data_t, 1);
    data->host = host;
    data->view = DOCKER_VIEW_ROOT;
    data->items = NULL;
    data->root_focus = NULL;
    data->current_project = NULL;
    data->current_container_id = NULL;
    data->current_container_name = NULL;
    data->title_buf = NULL;

    return data;
}

/* --------------------------------------------------------------------------------------------- */

static void
docker_close (void *plugin_data)
{
    docker_data_t *data = (docker_data_t *) plugin_data;

    clear_items (data);
    g_free (data->root_focus);
    g_free (data->current_project);
    g_free (data->current_container_id);
    g_free (data->current_container_name);
    g_free (data->title_buf);
    g_free (data);
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
docker_get_items (void *plugin_data, void *list_ptr)
{
    dir_list *list = (dir_list *) list_ptr;
    docker_data_t *data = (docker_data_t *) plugin_data;
    guint idx;

    if (data->view == DOCKER_VIEW_ROOT)
    {
        const char *sections[] = { "containers", "images", "volumes", "networks" };
        int sec_i;

        for (sec_i = 0; sec_i < 4; sec_i++)
            if (data->root_focus != NULL && strcmp (sections[sec_i], data->root_focus) == 0)
                add_entry (list, sections[sec_i], S_IFDIR | 0755, 0);

        for (sec_i = 0; sec_i < 4; sec_i++)
            if (data->root_focus == NULL || strcmp (sections[sec_i], data->root_focus) != 0)
                add_entry (list, sections[sec_i], S_IFDIR | 0755, 0);

        add_entry (list, docker_daemon_info_file, S_IFREG | 0644, 0);
        add_entry (list, docker_version_file, S_IFREG | 0644, 0);
        return MC_PPR_OK;
    }

    if (data->items == NULL)
        reload_items (data);

    if (data->items != NULL)
    {
        for (idx = 0; idx < data->items->len; idx++)
        {
            const docker_item_t *item = (const docker_item_t *) g_ptr_array_index (data->items, idx);
            mode_t mode = item->is_dir ? (S_IFDIR | 0755) : (S_IFREG | 0644);

            add_entry (list, item->name, mode, item->size);
        }
    }

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
docker_chdir (void *plugin_data, const char *path)
{
    docker_data_t *data = (docker_data_t *) plugin_data;

    if (strcmp (path, "..") == 0)
    {
        switch (data->view)
        {
        case DOCKER_VIEW_ROOT:
            return MC_PPR_NOT_SUPPORTED;

        case DOCKER_VIEW_CONTAINERS_PROJECTS:
            g_free (data->root_focus);
            data->root_focus = g_strdup ("containers");
            set_view (data, DOCKER_VIEW_ROOT);
            return MC_PPR_OK;

        case DOCKER_VIEW_IMAGES:
            g_free (data->root_focus);
            data->root_focus = g_strdup ("images");
            set_view (data, DOCKER_VIEW_ROOT);
            return MC_PPR_OK;

        case DOCKER_VIEW_VOLUMES:
            g_free (data->root_focus);
            data->root_focus = g_strdup ("volumes");
            set_view (data, DOCKER_VIEW_ROOT);
            return MC_PPR_OK;

        case DOCKER_VIEW_NETWORKS:
            g_free (data->root_focus);
            data->root_focus = g_strdup ("networks");
            set_view (data, DOCKER_VIEW_ROOT);
            return MC_PPR_OK;

        case DOCKER_VIEW_CONTAINERS_ITEMS:
            set_view (data, DOCKER_VIEW_CONTAINERS_PROJECTS);
            reload_items (data);
            return MC_PPR_OK;

        case DOCKER_VIEW_CONTAINER_DETAILS:
            set_view (data, DOCKER_VIEW_CONTAINERS_ITEMS);
            reload_items (data);
            return MC_PPR_OK;

        default:
            set_view (data, DOCKER_VIEW_ROOT);
            return MC_PPR_OK;
        }
    }

    if (data->view == DOCKER_VIEW_ROOT)
    {
        docker_view_t next = view_from_root_path (path);

        if (next == DOCKER_VIEW_ROOT)
            return MC_PPR_FAILED;

        g_free (data->root_focus);
        data->root_focus = g_strdup (path);
        set_view (data, next);
        reload_items (data);
        return MC_PPR_OK;
    }

    if (data->view == DOCKER_VIEW_CONTAINERS_PROJECTS)
    {
        const docker_item_t *project = find_item_by_name (data, path);

        if (project == NULL)
            return MC_PPR_FAILED;

        g_free (data->current_project);
        data->current_project = g_strdup (project->id);
        g_free (data->current_container_id);
        data->current_container_id = NULL;
        g_free (data->current_container_name);
        data->current_container_name = NULL;
        set_view (data, DOCKER_VIEW_CONTAINERS_ITEMS);
        reload_items (data);
        return MC_PPR_OK;
    }

    if (data->view == DOCKER_VIEW_CONTAINERS_ITEMS)
    {
        const docker_item_t *container = find_item_by_name (data, path);

        if (container == NULL)
            return MC_PPR_FAILED;

        g_free (data->current_container_id);
        data->current_container_id = g_strdup (container->id);

        g_free (data->current_container_name);
        data->current_container_name = g_strdup (container->name);

        set_view (data, DOCKER_VIEW_CONTAINER_DETAILS);
        reload_items (data);
        return MC_PPR_OK;
    }

    return MC_PPR_FAILED;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
docker_enter (void *plugin_data, const char *name, const struct stat *st)
{
    docker_data_t *data = (docker_data_t *) plugin_data;

    (void) st;
    if (data->view == DOCKER_VIEW_ROOT)
    {
        docker_view_t next = view_from_root_path (name);

        if (next != DOCKER_VIEW_ROOT)
        {
            g_free (data->root_focus);
            data->root_focus = g_strdup (name);
            set_view (data, next);
            reload_items (data);
            return MC_PPR_OK;
        }

        if (strcmp (name, docker_daemon_info_file) == 0 || strcmp (name, docker_version_file) == 0)
            return MC_PPR_NOT_SUPPORTED;

        return MC_PPR_FAILED;
    }

    if (data->view == DOCKER_VIEW_CONTAINERS_PROJECTS || data->view == DOCKER_VIEW_CONTAINERS_ITEMS)
        return docker_chdir (plugin_data, name);

    if (data->view == DOCKER_VIEW_CONTAINER_DETAILS)
    {
        if (strcmp (name, docker_inspect_file) == 0)
            return MC_PPR_NOT_SUPPORTED;

        return MC_PPR_FAILED;
    }

    if (find_item_by_name (data, name) != NULL)
        return MC_PPR_NOT_SUPPORTED;

    return MC_PPR_FAILED;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
docker_get_local_copy (void *plugin_data, const char *fname, char **local_path)
{
    docker_data_t *data = (docker_data_t *) plugin_data;
    char *output = NULL;
    char *err_text = NULL;
    const char *cmd = NULL;
    char *cmd_dynamic = NULL;
    gboolean ok;

    if (data->view == DOCKER_VIEW_ROOT)
    {
        if (strcmp (fname, docker_daemon_info_file) == 0)
            cmd = "docker info";
        else if (strcmp (fname, docker_version_file) == 0)
            cmd = "docker version";
        else
            return MC_PPR_FAILED;
    }
    else if (data->view == DOCKER_VIEW_CONTAINER_DETAILS)
    {
        char *quoted_id;

        if (data->current_container_id == NULL || strcmp (fname, docker_inspect_file) != 0)
            return MC_PPR_FAILED;

        quoted_id = g_shell_quote (data->current_container_id);
        cmd_dynamic = g_strdup_printf ("docker inspect %s", quoted_id);
        g_free (quoted_id);
        cmd = cmd_dynamic;
    }
    else if (data->view == DOCKER_VIEW_CONTAINERS_ITEMS)
    {
        const docker_item_t *container = find_item_by_name (data, fname);
        char *quoted_id;

        if (container == NULL)
            return MC_PPR_FAILED;

        quoted_id = g_shell_quote (container->id);
        cmd_dynamic = g_strdup_printf ("docker inspect %s", quoted_id);
        g_free (quoted_id);
        cmd = cmd_dynamic;
    }
    else
    {
        const docker_item_t *item = find_item_by_name (data, fname);
        char *quoted_id;

        if (item == NULL)
            return MC_PPR_FAILED;

        quoted_id = g_shell_quote (item->id);
        cmd_dynamic = g_strdup_printf ("docker inspect %s", quoted_id);
        g_free (quoted_id);
        cmd = cmd_dynamic;
    }

    ok = run_cmd (cmd, &output, &err_text);
    g_free (cmd_dynamic);

    if (!ok)
    {
        if (err_text != NULL && err_text[0] != '\0')
            message (D_ERROR, MSG_ERROR, "%s", err_text);
        g_free (output);
        g_free (err_text);
        return MC_PPR_FAILED;
    }

    ok = write_temp_content ("mc-pp-docker-XXXXXX", output, local_path);

    g_free (output);
    g_free (err_text);

    return ok ? MC_PPR_OK : MC_PPR_FAILED;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
docker_delete_items (void *plugin_data, const char **names, int count)
{
    docker_data_t *data = (docker_data_t *) plugin_data;
    int i;

    if (data->view == DOCKER_VIEW_ROOT || data->view == DOCKER_VIEW_CONTAINERS_PROJECTS
        || data->view == DOCKER_VIEW_CONTAINER_DETAILS)
        return MC_PPR_NOT_SUPPORTED;

    for (i = 0; i < count; i++)
    {
        const docker_item_t *item;
        char *quoted;
        char *cmd;
        char *output = NULL;
        char *err_text = NULL;

        item = find_item_by_name (data, names[i]);
        if (item == NULL)
            continue;

        quoted = g_shell_quote (item->id);

        switch (data->view)
        {
        case DOCKER_VIEW_CONTAINERS_ITEMS:
            cmd = g_strdup_printf ("docker rm %s", quoted);
            break;
        case DOCKER_VIEW_IMAGES:
            cmd = g_strdup_printf ("docker rmi %s", quoted);
            break;
        case DOCKER_VIEW_VOLUMES:
            cmd = g_strdup_printf ("docker volume rm %s", quoted);
            break;
        case DOCKER_VIEW_NETWORKS:
            cmd = g_strdup_printf ("docker network rm %s", quoted);
            break;
        default:
            g_free (quoted);
            return MC_PPR_NOT_SUPPORTED;
        }

        if (!run_cmd (cmd, &output, &err_text) && err_text != NULL && err_text[0] != '\0')
            message (D_ERROR, MSG_ERROR, "%s", err_text);

        g_free (output);
        g_free (err_text);
        g_free (cmd);
        g_free (quoted);
    }

    reload_items (data);
    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
docker_get_title (void *plugin_data)
{
    docker_data_t *data = (docker_data_t *) plugin_data;

    g_free (data->title_buf);

    switch (data->view)
    {
    case DOCKER_VIEW_ROOT:
        data->title_buf = g_strdup ("/");
        break;

    case DOCKER_VIEW_CONTAINERS_PROJECTS:
        data->title_buf = g_strdup ("/containers");
        break;

    case DOCKER_VIEW_CONTAINERS_ITEMS:
        data->title_buf = g_strdup_printf ("/containers/%s",
                                           data->current_project != NULL ? data->current_project
                                                                         : docker_ungrouped_project);
        break;

    case DOCKER_VIEW_CONTAINER_DETAILS:
        data->title_buf = g_strdup_printf ("/containers/%s/%s",
                                           data->current_project != NULL ? data->current_project
                                                                         : docker_ungrouped_project,
                                           data->current_container_name != NULL
                                               ? data->current_container_name
                                               : "container");
        break;

    case DOCKER_VIEW_IMAGES:
        data->title_buf = g_strdup ("/images");
        break;

    case DOCKER_VIEW_VOLUMES:
        data->title_buf = g_strdup ("/volumes");
        break;

    case DOCKER_VIEW_NETWORKS:
        data->title_buf = g_strdup ("/networks");
        break;

    default:
        data->title_buf = g_strdup ("/");
        break;
    }

    return data->title_buf;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
docker_create_item (void *plugin_data)
{
    docker_data_t *data = (docker_data_t *) plugin_data;
    char *image = NULL;
    char *name = NULL;
    char *command = NULL;
    gboolean detach = TRUE;
    char *q_image = NULL;
    char *q_name = NULL;
    char *q_cmd = NULL;
    char *q_project = NULL;
    char *cmd = NULL;
    char *output = NULL;
    char *err_text = NULL;
    mc_pp_result_t result = MC_PPR_FAILED;

    if (data->view != DOCKER_VIEW_CONTAINERS_ITEMS && data->view != DOCKER_VIEW_CONTAINERS_PROJECTS)
        return MC_PPR_NOT_SUPPORTED;

    if (!show_create_container_dialog (&image, &name, &command, &detach))
        goto done;

    if (image == NULL || image[0] == '\0')
        goto done;

    q_image = g_shell_quote (image);

    if (name != NULL && name[0] != '\0')
        q_name = g_shell_quote (name);

    if (command != NULL && command[0] != '\0')
        q_cmd = g_shell_quote (command);

    if (data->view == DOCKER_VIEW_CONTAINERS_ITEMS && data->current_project != NULL
        && !is_ungrouped_project (data->current_project))
        q_project = g_shell_quote (data->current_project);

    cmd = g_strdup ("docker run ");

    if (detach)
    {
        char *tmp = g_strconcat (cmd, "-d ", (char *) NULL);
        g_free (cmd);
        cmd = tmp;
    }

    if (q_name != NULL)
    {
        char *tmp = g_strconcat (cmd, "--name ", q_name, " ", (char *) NULL);
        g_free (cmd);
        cmd = tmp;
    }

    if (q_project != NULL)
    {
        char *tmp = g_strconcat (cmd, "--label com.docker.compose.project=", q_project, " ",
                                 (char *) NULL);
        g_free (cmd);
        cmd = tmp;
    }

    {
        char *tmp = g_strconcat (cmd, q_image, (char *) NULL);
        g_free (cmd);
        cmd = tmp;
    }

    if (q_cmd != NULL)
    {
        char *tmp = g_strconcat (cmd, " sh -c ", q_cmd, (char *) NULL);
        g_free (cmd);
        cmd = tmp;
    }

    if (!run_cmd (cmd, &output, &err_text))
    {
        if (err_text != NULL && err_text[0] != '\0')
            message (D_ERROR, MSG_ERROR, "%s", err_text);
        goto done;
    }

    reload_items (data);
    result = MC_PPR_OK;

done:
    g_free (image);
    g_free (name);
    g_free (command);
    g_free (q_image);
    g_free (q_name);
    g_free (q_cmd);
    g_free (q_project);
    g_free (cmd);
    g_free (output);
    g_free (err_text);

    return result;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

const mc_panel_plugin_t *mc_panel_plugin_register (void);

const mc_panel_plugin_t *
mc_panel_plugin_register (void)
{
    return &docker_plugin;
}

/* --------------------------------------------------------------------------------------------- */
