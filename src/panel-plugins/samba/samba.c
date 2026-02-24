/*
   Samba network browser panel plugin (libsmbclient).

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

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <libsmbclient.h>

#include "lib/global.h"
#include "lib/panel-plugin.h"
#include "lib/widget.h"

#include "src/filemanager/dir.h"

/*** file scope type declarations ****************************************************************/

/* A saved connection bookmark */
typedef struct
{
    char *label;                /* display name (ini group key) */
    char *server;
    char *share;                /* default share/path, may be empty */
    char *username;             /* may include DOMAIN\user */
    char *password;
    char *workgroup;
} smb_connection_t;

/* A directory entry (file/dir/share inside a connection) */
typedef struct
{
    char *name;
    unsigned int smbc_type;     /* SMBC_FILE_SHARE, SMBC_DIR, SMBC_FILE, etc. */
    struct stat st;
} smb_entry_t;

typedef struct
{
    mc_panel_host_t *host;
    SMBCCTX *ctx;

    /* Navigation state */
    gboolean at_root;           /* TRUE = showing saved connections list */
    char *current_url;          /* "smb://SERVER/SHARE/path" when not at root */
    GPtrArray *entries;         /* smb_entry_t* when browsing; NULL at root */

    /* Saved connections */
    GPtrArray *connections;     /* smb_connection_t* — loaded from ini */
    char *connections_file;     /* path to ini file */

    /* Active connection credentials (for auth callback) */
    char *auth_username;
    char *auth_password;
    char *auth_workgroup;

    char *title_buf;
} samba_data_t;

/*** forward declarations (file scope functions) *************************************************/

static void *samba_open (mc_panel_host_t *host, const char *open_path);
static void samba_close (void *plugin_data);
static mc_pp_result_t samba_get_items (void *plugin_data, void *list_ptr);
static mc_pp_result_t samba_chdir (void *plugin_data, const char *path);
static mc_pp_result_t samba_enter (void *plugin_data, const char *name, const struct stat *st);
static mc_pp_result_t samba_get_local_copy (void *plugin_data, const char *fname,
                                            char **local_path);
static mc_pp_result_t samba_delete_items (void *plugin_data, const char **names, int count);
static const char *samba_get_title (void *plugin_data);
static mc_pp_result_t samba_create_item (void *plugin_data);

/*** file scope variables ************************************************************************/

static const mc_panel_plugin_t samba_plugin = {
    .api_version = MC_PANEL_PLUGIN_API_VERSION,
    .name = "samba",
    .display_name = "Samba network",
    .proto = "smb",
    .prefix = NULL,
    .flags =
        MC_PPF_NAVIGATE | MC_PPF_GET_FILES | MC_PPF_DELETE | MC_PPF_CUSTOM_TITLE | MC_PPF_CREATE,

    .open = samba_open,
    .close = samba_close,
    .get_items = samba_get_items,

    .chdir = samba_chdir,
    .enter = samba_enter,
    .get_local_copy = samba_get_local_copy,
    .delete_items = samba_delete_items,
    .get_title = samba_get_title,
    .handle_key = NULL,
    .create_item = samba_create_item,
};

/*** file scope functions ************************************************************************/

static void
add_entry (dir_list *list, const char *name, mode_t mode, off_t size, time_t mtime)
{
    struct stat st;

    memset (&st, 0, sizeof (st));
    st.st_mode = mode;
    st.st_size = size;
    st.st_mtime = mtime;
    st.st_uid = getuid ();
    st.st_gid = getgid ();
    st.st_nlink = 1;

    dir_list_append (list, name, &st, S_ISDIR (mode), FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static void
smb_entry_free (gpointer p)
{
    smb_entry_t *e = (smb_entry_t *) p;

    g_free (e->name);
    g_free (e);
}

/* --------------------------------------------------------------------------------------------- */

static void
smb_connection_free (gpointer p)
{
    smb_connection_t *c = (smb_connection_t *) p;

    g_free (c->label);
    g_free (c->server);
    g_free (c->share);
    g_free (c->username);
    g_free (c->password);
    g_free (c->workgroup);
    g_free (c);
}

/* --------------------------------------------------------------------------------------------- */
/* Connection storage (ini file) */
/* --------------------------------------------------------------------------------------------- */

static char *
get_connections_file_path (void)
{
    return g_build_filename (g_get_user_config_dir (), "mc", "smb-connections.ini", (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

static GPtrArray *
load_connections (const char *filepath)
{
    GPtrArray *arr;
    GKeyFile *kf;
    gchar **groups;
    gsize n_groups, i;

    arr = g_ptr_array_new_with_free_func (smb_connection_free);

    kf = g_key_file_new ();
    if (!g_key_file_load_from_file (kf, filepath, G_KEY_FILE_NONE, NULL))
    {
        g_key_file_free (kf);
        return arr;
    }

    groups = g_key_file_get_groups (kf, &n_groups);
    for (i = 0; i < n_groups; i++)
    {
        smb_connection_t *conn;

        conn = g_new0 (smb_connection_t, 1);
        conn->label = g_strdup (groups[i]);
        conn->server = g_key_file_get_string (kf, groups[i], "server", NULL);
        conn->share = g_key_file_get_string (kf, groups[i], "share", NULL);
        conn->username = g_key_file_get_string (kf, groups[i], "username", NULL);
        conn->password = g_key_file_get_string (kf, groups[i], "password", NULL);
        conn->workgroup = g_key_file_get_string (kf, groups[i], "workgroup", NULL);

        if (conn->server == NULL || conn->server[0] == '\0')
        {
            smb_connection_free (conn);
            continue;
        }

        g_ptr_array_add (arr, conn);
    }

    g_strfreev (groups);
    g_key_file_free (kf);
    return arr;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
save_connections (const char *filepath, GPtrArray *connections)
{
    GKeyFile *kf;
    gchar *data;
    gsize length;
    gboolean ok;
    gchar *dir;
    guint i;

    kf = g_key_file_new ();

    for (i = 0; i < connections->len; i++)
    {
        const smb_connection_t *conn =
            (const smb_connection_t *) g_ptr_array_index (connections, i);

        g_key_file_set_string (kf, conn->label, "server", conn->server);
        if (conn->share != NULL)
            g_key_file_set_string (kf, conn->label, "share", conn->share);
        if (conn->username != NULL)
            g_key_file_set_string (kf, conn->label, "username", conn->username);
        if (conn->password != NULL)
            g_key_file_set_string (kf, conn->label, "password", conn->password);
        if (conn->workgroup != NULL)
            g_key_file_set_string (kf, conn->label, "workgroup", conn->workgroup);
    }

    data = g_key_file_to_data (kf, &length, NULL);
    g_key_file_free (kf);

    if (data == NULL)
        return FALSE;

    /* Ensure directory exists */
    dir = g_path_get_dirname (filepath);
    g_mkdir_with_parents (dir, 0700);
    g_free (dir);

    ok = g_file_set_contents (filepath, data, (gssize) length, NULL);
    g_free (data);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

static const smb_connection_t *
find_connection (const samba_data_t *data, const char *label)
{
    guint i;

    for (i = 0; i < data->connections->len; i++)
    {
        const smb_connection_t *c =
            (const smb_connection_t *) g_ptr_array_index (data->connections, i);

        if (strcmp (c->label, label) == 0)
            return c;
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */
/* libsmbclient helpers */
/* --------------------------------------------------------------------------------------------- */

static void
smb_auth_cb (SMBCCTX *ctx, const char *server, const char *share,
             char *wg, int wg_len, char *un, int un_len, char *pw, int pw_len)
{
    samba_data_t *data;

    (void) server;
    (void) share;

    data = (samba_data_t *) smbc_getOptionUserData (ctx);
    if (data == NULL)
        return;

    if (data->auth_workgroup != NULL)
        g_strlcpy (wg, data->auth_workgroup, (gsize) wg_len);
    if (data->auth_username != NULL)
        g_strlcpy (un, data->auth_username, (gsize) un_len);
    if (data->auth_password != NULL)
        g_strlcpy (pw, data->auth_password, (gsize) pw_len);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
smb_is_inside_share (const char *url)
{
    const char *p;
    int slash_count = 0;

    p = url + 6;                /* skip "smb://" */
    while (*p != '\0')
    {
        if (*p == '/')
            slash_count++;
        p++;
    }

    return (slash_count >= 1);
}

/* --------------------------------------------------------------------------------------------- */

static GPtrArray *
smb_load_entries (samba_data_t *data)
{
    GPtrArray *arr;
    int dh;
    struct smbc_dirent *dirent;
    gboolean inside_share;

    arr = g_ptr_array_new_with_free_func (smb_entry_free);

    smbc_set_context (data->ctx);
    dh = smbc_opendir (data->current_url);
    if (dh < 0)
        return arr;

    inside_share = smb_is_inside_share (data->current_url);

    while ((dirent = smbc_readdir (dh)) != NULL)
    {
        smb_entry_t *entry;

        if (strcmp (dirent->name, ".") == 0 || strcmp (dirent->name, "..") == 0)
            continue;

        /* skip IPC$, printer, comms shares */
        if (dirent->smbc_type == SMBC_IPC_SHARE || dirent->smbc_type == SMBC_PRINTER_SHARE
            || dirent->smbc_type == SMBC_COMMS_SHARE)
            continue;

        entry = g_new0 (smb_entry_t, 1);
        entry->name = g_strdup (dirent->name);
        entry->smbc_type = dirent->smbc_type;

        if (inside_share
            && (dirent->smbc_type == SMBC_FILE || dirent->smbc_type == SMBC_DIR
                || dirent->smbc_type == SMBC_LINK))
        {
            char *full_url;

            full_url = g_strdup_printf ("%s/%s", data->current_url, dirent->name);
            if (smbc_stat (full_url, &entry->st) != 0)
            {
                memset (&entry->st, 0, sizeof (entry->st));
                entry->st.st_mtime = time (NULL);
            }
            g_free (full_url);
        }
        else
        {
            memset (&entry->st, 0, sizeof (entry->st));
            entry->st.st_mtime = time (NULL);
        }

        g_ptr_array_add (arr, entry);
    }

    smbc_closedir (dh);
    return arr;
}

/* --------------------------------------------------------------------------------------------- */

static char *
smb_url_up (const char *url)
{
    const char *after_scheme;
    const char *last_slash;

    after_scheme = url + 6;     /* past "smb://" */

    if (*after_scheme == '\0')
        return NULL;

    last_slash = strrchr (after_scheme, '/');
    if (last_slash == NULL)
        return g_strdup ("smb://");

    return g_strndup (url, (gsize) (last_slash - url));
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
smb_entry_is_dir (unsigned int smbc_type)
{
    return (smbc_type == SMBC_WORKGROUP || smbc_type == SMBC_SERVER
            || smbc_type == SMBC_FILE_SHARE || smbc_type == SMBC_DIR);
}

/* --------------------------------------------------------------------------------------------- */

static const smb_entry_t *
find_entry (const samba_data_t *data, const char *name)
{
    guint i;

    if (data->entries == NULL)
        return NULL;

    for (i = 0; i < data->entries->len; i++)
    {
        const smb_entry_t *e = (const smb_entry_t *) g_ptr_array_index (data->entries, i);

        if (strcmp (e->name, name) == 0)
            return e;
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
set_auth_from_connection (samba_data_t *data, const smb_connection_t *conn)
{
    g_free (data->auth_username);
    g_free (data->auth_password);
    g_free (data->auth_workgroup);

    data->auth_username = g_strdup (conn->username);
    data->auth_password = g_strdup (conn->password);
    data->auth_workgroup = g_strdup (conn->workgroup);
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
enter_connection (samba_data_t *data, const smb_connection_t *conn)
{
    set_auth_from_connection (data, conn);

    g_free (data->current_url);
    if (conn->share != NULL && conn->share[0] != '\0')
        data->current_url = g_strdup_printf ("smb://%s/%s", conn->server, conn->share);
    else
        data->current_url = g_strdup_printf ("smb://%s", conn->server);

    if (data->entries != NULL)
        g_ptr_array_free (data->entries, TRUE);

    smbc_set_context (data->ctx);
    data->entries = smb_load_entries (data);
    data->at_root = FALSE;

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */
/* Connection dialog */
/* --------------------------------------------------------------------------------------------- */

static gboolean
show_connection_dialog (char **label, char **server, char **share,
                        char **username, char **password)
{
    /* clang-format off */
    quick_widget_t quick_widgets[] = {
        QUICK_LABELED_INPUT (N_("Name:"), input_label_above,
                            *label != NULL ? *label : "", "smb-conn-label",
                            label, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
        QUICK_SEPARATOR (FALSE),
        QUICK_LABELED_INPUT (N_("Server:"), input_label_above,
                            *server != NULL ? *server : "", "smb-conn-server",
                            server, NULL, FALSE, FALSE, INPUT_COMPLETE_HOSTNAMES),
        QUICK_SEPARATOR (FALSE),
        QUICK_LABELED_INPUT (N_("Share/path:"), input_label_above,
                            *share != NULL ? *share : "", "smb-conn-share",
                            share, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
        QUICK_SEPARATOR (FALSE),
        QUICK_LABELED_INPUT (N_("Username (DOMAIN\\user):"), input_label_above,
                            *username != NULL ? *username : "", "smb-conn-user",
                            username, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
        QUICK_SEPARATOR (FALSE),
        QUICK_LABELED_INPUT (N_("Password:"), input_label_above,
                            *password != NULL ? *password : "", "smb-conn-pass",
                            password, NULL, TRUE, TRUE, INPUT_COMPLETE_NONE),
        QUICK_BUTTONS_OK_CANCEL,
        QUICK_END,
    };
    /* clang-format on */

    WRect r = { -1, -1, 0, 56 };

    quick_dialog_t qdlg = {
        .rect = r,
        .title = N_("SMB Connection"),
        .help = "[Samba Plugin]",
        .widgets = quick_widgets,
        .callback = NULL,
        .mouse_callback = NULL,
    };

    return (quick_dialog (&qdlg) == B_ENTER);
}

/* --------------------------------------------------------------------------------------------- */
/* Plugin callbacks */
/* --------------------------------------------------------------------------------------------- */

static void *
samba_open (mc_panel_host_t *host, const char *open_path)
{
    samba_data_t *data;

    (void) open_path;

    data = g_new0 (samba_data_t, 1);
    data->host = host;
    data->at_root = TRUE;
    data->current_url = NULL;
    data->entries = NULL;
    data->title_buf = NULL;

    data->auth_username = NULL;
    data->auth_password = NULL;
    data->auth_workgroup = NULL;

    /* Load saved connections */
    data->connections_file = get_connections_file_path ();
    data->connections = load_connections (data->connections_file);

    /* Create libsmbclient context */
    data->ctx = smbc_new_context ();
    if (data->ctx == NULL)
    {
        g_ptr_array_free (data->connections, TRUE);
        g_free (data->connections_file);
        g_free (data);
        return NULL;
    }

    smbc_setDebug (data->ctx, 0);
    smbc_setFunctionAuthDataWithContext (data->ctx, smb_auth_cb);
    smbc_setOptionUserData (data->ctx, data);

    if (smbc_init_context (data->ctx) == NULL)
    {
        smbc_free_context (data->ctx, 0);
        g_ptr_array_free (data->connections, TRUE);
        g_free (data->connections_file);
        g_free (data);
        return NULL;
    }

    return data;
}

/* --------------------------------------------------------------------------------------------- */

static void
samba_close (void *plugin_data)
{
    samba_data_t *data = (samba_data_t *) plugin_data;

    if (data->entries != NULL)
        g_ptr_array_free (data->entries, TRUE);

    g_ptr_array_free (data->connections, TRUE);

    if (data->ctx != NULL)
        smbc_free_context (data->ctx, 1);

    g_free (data->current_url);
    g_free (data->title_buf);
    g_free (data->connections_file);
    g_free (data->auth_username);
    g_free (data->auth_password);
    g_free (data->auth_workgroup);
    g_free (data);
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
samba_get_items (void *plugin_data, void *list_ptr)
{
    dir_list *list = (dir_list *) list_ptr;
    samba_data_t *data = (samba_data_t *) plugin_data;
    guint i;

    if (data->at_root)
    {
        /* Show saved connections as directories */
        for (i = 0; i < data->connections->len; i++)
        {
            const smb_connection_t *conn =
                (const smb_connection_t *) g_ptr_array_index (data->connections, i);

            add_entry (list, conn->label, S_IFDIR | 0755, 0, time (NULL));
        }
        return MC_PPR_OK;
    }

    /* Inside a connection — show SMB entries */
    if (data->entries != NULL)
    {
        for (i = 0; i < data->entries->len; i++)
        {
            const smb_entry_t *e = (const smb_entry_t *) g_ptr_array_index (data->entries, i);
            mode_t mode;
            off_t size;
            time_t mtime;

            if (smb_entry_is_dir (e->smbc_type))
            {
                mode = S_IFDIR | 0755;
                size = 0;
            }
            else
            {
                mode = S_IFREG | 0644;
                size = e->st.st_size;
            }

            mtime = (e->st.st_mtime != 0) ? e->st.st_mtime : time (NULL);
            add_entry (list, e->name, mode, size, mtime);
        }
    }

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
samba_chdir (void *plugin_data, const char *path)
{
    samba_data_t *data = (samba_data_t *) plugin_data;

    if (strcmp (path, "..") == 0)
    {
        if (data->at_root)
            return MC_PPR_NOT_SUPPORTED;        /* close plugin */

        /* Are we at the connection's base URL (smb://SERVER or smb://SERVER/SHARE)? */
        {
            char *parent;

            parent = smb_url_up (data->current_url);
            if (parent == NULL)
            {
                /* At smb:// level — go back to root (connections list) */
                data->at_root = TRUE;
                g_free (data->current_url);
                data->current_url = NULL;

                if (data->entries != NULL)
                {
                    g_ptr_array_free (data->entries, TRUE);
                    data->entries = NULL;
                }
                return MC_PPR_OK;
            }

            /* Navigate up within the connection */
            g_free (data->current_url);
            data->current_url = parent;

            if (data->entries != NULL)
                g_ptr_array_free (data->entries, TRUE);

            smbc_set_context (data->ctx);
            data->entries = smb_load_entries (data);
            return MC_PPR_OK;
        }
    }

    /* At root: enter a saved connection */
    if (data->at_root)
    {
        const smb_connection_t *conn;

        conn = find_connection (data, path);
        if (conn == NULL)
            return MC_PPR_FAILED;

        return enter_connection (data, conn);
    }

    /* Inside connection: navigate into subdir/share */
    {
        const smb_entry_t *entry;
        char *new_url;

        entry = find_entry (data, path);
        if (entry == NULL || !smb_entry_is_dir (entry->smbc_type))
            return MC_PPR_FAILED;

        if (g_str_has_suffix (data->current_url, "/"))
            new_url = g_strdup_printf ("%s%s", data->current_url, path);
        else
            new_url = g_strdup_printf ("%s/%s", data->current_url, path);

        g_free (data->current_url);
        data->current_url = new_url;

        if (data->entries != NULL)
            g_ptr_array_free (data->entries, TRUE);

        smbc_set_context (data->ctx);
        data->entries = smb_load_entries (data);
        return MC_PPR_OK;
    }
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
samba_enter (void *plugin_data, const char *name, const struct stat *st)
{
    samba_data_t *data = (samba_data_t *) plugin_data;

    (void) st;

    /* At root: enter saved connection */
    if (data->at_root)
    {
        const smb_connection_t *conn;

        conn = find_connection (data, name);
        if (conn == NULL)
            return MC_PPR_FAILED;

        return enter_connection (data, conn);
    }

    /* Inside connection */
    {
        const smb_entry_t *entry;

        entry = find_entry (data, name);
        if (entry == NULL)
            return MC_PPR_FAILED;

        if (smb_entry_is_dir (entry->smbc_type))
        {
            char *new_url;

            if (g_str_has_suffix (data->current_url, "/"))
                new_url = g_strdup_printf ("%s%s", data->current_url, name);
            else
                new_url = g_strdup_printf ("%s/%s", data->current_url, name);

            g_free (data->current_url);
            data->current_url = new_url;

            if (data->entries != NULL)
                g_ptr_array_free (data->entries, TRUE);

            smbc_set_context (data->ctx);
            data->entries = smb_load_entries (data);
            return MC_PPR_OK;
        }

        /* File — let mc handle via get_local_copy / viewer */
        return MC_PPR_NOT_SUPPORTED;
    }
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
samba_get_local_copy (void *plugin_data, const char *fname, char **local_path)
{
    samba_data_t *data = (samba_data_t *) plugin_data;
    char *smb_url;
    int smb_fd;
    int local_fd;
    GError *error = NULL;
    char buf[8192];
    ssize_t n;

    if (data->at_root || data->current_url == NULL)
        return MC_PPR_FAILED;

    smb_url = g_strdup_printf ("%s/%s", data->current_url, fname);

    smbc_set_context (data->ctx);
    smb_fd = smbc_open (smb_url, O_RDONLY, 0);
    g_free (smb_url);

    if (smb_fd < 0)
        return MC_PPR_FAILED;

    local_fd = g_file_open_tmp ("mc-pp-smb-XXXXXX", local_path, &error);
    if (local_fd == -1)
    {
        if (error != NULL)
            g_error_free (error);
        smbc_close (smb_fd);
        return MC_PPR_FAILED;
    }

    while ((n = smbc_read (smb_fd, buf, sizeof (buf))) > 0)
    {
        if (write (local_fd, buf, (size_t) n) != n)
        {
            close (local_fd);
            smbc_close (smb_fd);
            unlink (*local_path);
            g_free (*local_path);
            *local_path = NULL;
            return MC_PPR_FAILED;
        }
    }

    close (local_fd);
    smbc_close (smb_fd);

    if (n < 0)
    {
        unlink (*local_path);
        g_free (*local_path);
        *local_path = NULL;
        return MC_PPR_FAILED;
    }

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
samba_delete_items (void *plugin_data, const char **names, int count)
{
    samba_data_t *data = (samba_data_t *) plugin_data;
    int i;

    if (data->at_root)
    {
        /* Delete saved connections */
        for (i = 0; i < count; i++)
        {
            guint j;

            for (j = 0; j < data->connections->len; j++)
            {
                const smb_connection_t *conn =
                    (const smb_connection_t *) g_ptr_array_index (data->connections, j);

                if (strcmp (conn->label, names[i]) == 0)
                {
                    g_ptr_array_remove_index (data->connections, j);
                    break;
                }
            }
        }

        save_connections (data->connections_file, data->connections);
        return MC_PPR_OK;
    }

    /* Inside connection: delete files/dirs on SMB */
    smbc_set_context (data->ctx);

    for (i = 0; i < count; i++)
    {
        const smb_entry_t *entry;
        char *url;

        entry = find_entry (data, names[i]);
        url = g_strdup_printf ("%s/%s", data->current_url, names[i]);

        if (entry != NULL && entry->smbc_type == SMBC_DIR)
            smbc_rmdir (url);
        else
            smbc_unlink (url);

        g_free (url);
    }

    if (data->entries != NULL)
        g_ptr_array_free (data->entries, TRUE);
    data->entries = smb_load_entries (data);

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
samba_get_title (void *plugin_data)
{
    samba_data_t *data = (samba_data_t *) plugin_data;

    g_free (data->title_buf);

    if (data->at_root || data->current_url == NULL)
    {
        data->title_buf = g_strdup ("/");
        return data->title_buf;
    }

    /* Strip "smb:/" to get path for display */
    if (strlen (data->current_url) <= 5)
        data->title_buf = g_strdup ("/");
    else
        data->title_buf = g_strdup (data->current_url + 5);

    return data->title_buf;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
samba_create_item (void *plugin_data)
{
    samba_data_t *data = (samba_data_t *) plugin_data;
    smb_connection_t *conn;
    char *label = NULL;
    char *server = NULL;
    char *share = NULL;
    char *username = NULL;
    char *password = NULL;

    if (!data->at_root)
        return MC_PPR_NOT_SUPPORTED;

    if (!show_connection_dialog (&label, &server, &share, &username, &password))
    {
        g_free (label);
        g_free (server);
        g_free (share);
        g_free (username);
        g_free (password);
        return MC_PPR_FAILED;
    }

    if (label == NULL || label[0] == '\0' || server == NULL || server[0] == '\0')
    {
        g_free (label);
        g_free (server);
        g_free (share);
        g_free (username);
        g_free (password);
        return MC_PPR_FAILED;
    }

    conn = g_new0 (smb_connection_t, 1);
    conn->label = label;
    conn->server = server;
    conn->share = share;
    conn->username = username;
    conn->password = password;

    /* Extract workgroup from DOMAIN\user if present */
    if (username != NULL)
    {
        char *backslash;

        backslash = strchr (username, '\\');
        if (backslash != NULL)
        {
            conn->workgroup = g_strndup (username, (gsize) (backslash - username));
            /* Keep full username — libsmbclient handles DOMAIN\user */
        }
    }

    g_ptr_array_add (data->connections, conn);
    save_connections (data->connections_file, data->connections);

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

const mc_panel_plugin_t *mc_panel_plugin_register (void);

const mc_panel_plugin_t *
mc_panel_plugin_register (void)
{
    return &samba_plugin;
}

/* --------------------------------------------------------------------------------------------- */
