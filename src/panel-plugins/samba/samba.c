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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
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
    char *label; /* display name (ini group key) */
    char *server;
    char *share;    /* default share/path, may be empty */
    char *username; /* may include DOMAIN\user */
    char *password;
    char *workgroup;
} smb_connection_t;

/* A directory entry (file/dir/share inside a connection) */
typedef struct
{
    char *name;
    unsigned int smbc_type; /* SMBC_FILE_SHARE, SMBC_DIR, SMBC_FILE, etc. */
    struct stat st;
} smb_entry_t;

typedef struct
{
    mc_panel_host_t *host;
    SMBCCTX *ctx;

    /* Navigation state */
    gboolean at_root;   /* TRUE = showing saved connections list */
    char *current_url;  /* "smb://SERVER/SHARE/path" when not at root */
    GPtrArray *entries; /* smb_entry_t* when browsing; NULL at root */

    /* Saved connections */
    GPtrArray *connections; /* smb_connection_t* — loaded from ini */
    char *connections_file; /* path to ini file */

    /* Active connection credentials (for auth callback) */
    char *auth_username;
    char *auth_password;
    char *auth_workgroup;

    char *title_buf;
} samba_data_t;

typedef struct
{
    int saved_stderr_fd;
    int null_fd;
    gboolean active;
} stderr_silence_t;

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
static void samba_update_title (samba_data_t *data);

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
stderr_silence_begin (stderr_silence_t *s)
{
    s->saved_stderr_fd = -1;
    s->null_fd = -1;
    s->active = FALSE;

    s->saved_stderr_fd = dup (STDERR_FILENO);
    if (s->saved_stderr_fd < 0)
        return;

    s->null_fd = open ("/dev/null", O_WRONLY);
    if (s->null_fd < 0)
    {
        close (s->saved_stderr_fd);
        s->saved_stderr_fd = -1;
        return;
    }

    if (dup2 (s->null_fd, STDERR_FILENO) >= 0)
        s->active = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
stderr_silence_end (stderr_silence_t *s)
{
    if (s->saved_stderr_fd >= 0)
    {
        (void) dup2 (s->saved_stderr_fd, STDERR_FILENO);
        close (s->saved_stderr_fd);
    }
    if (s->null_fd >= 0)
        close (s->null_fd);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
env_is_true (const char *value)
{
    return (value != NULL
            && (*value == '1' || g_ascii_strcasecmp (value, "true") == 0
                || g_ascii_strcasecmp (value, "yes") == 0));
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
samba_logging_enabled (void)
{
    const char *smb_enable;
    const char *mc_enable;

    smb_enable = g_getenv ("MC_SMB_LOG_ENABLE");
    if (smb_enable != NULL)
        return env_is_true (smb_enable);

    mc_enable = g_getenv ("MC_LOG_ENABLE");
    return env_is_true (mc_enable);
}

/* --------------------------------------------------------------------------------------------- */

static void G_GNUC_PRINTF (1, 2)
samba_log (const char *fmt, ...)
{
    const char *log_file;
    FILE *f;
    va_list args;

    if (!samba_logging_enabled ())
        return;

    log_file = g_getenv ("MC_SMB_LOG_FILE");
    if (log_file == NULL || *log_file == '\0')
        log_file = g_getenv ("MC_LOG_FILE");
    if (log_file == NULL || *log_file == '\0')
        log_file = "/tmp/mc-samba.log";

    f = fopen (log_file, "a");
    if (f == NULL)
        return;

    va_start (args, fmt);
    (void) vfprintf (f, fmt, args);
    va_end (args);
    (void) fclose (f);
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

static void
samba_update_title (samba_data_t *data)
{
    g_free (data->title_buf);

    if (data->at_root || data->current_url == NULL)
    {
        data->title_buf = g_strdup ("/");
        return;
    }

    /* Strip "smb:/" to get path for display */
    if (strlen (data->current_url) <= 5)
        data->title_buf = g_strdup ("/");
    else
        data->title_buf = g_strdup (data->current_url + 5);
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
smb_auth_cb (SMBCCTX *ctx, const char *server, const char *share, char *wg, int wg_len, char *un,
             int un_len, char *pw, int pw_len)
{
    samba_data_t *data;

    (void) server;
    (void) share;

    data = (samba_data_t *) smbc_getOptionUserData (ctx);
    if (data == NULL)
        return;

    samba_log ("samba: auth callback server='%s' share='%s' user='%s' workgroup='%s'\n",
               server != NULL ? server : "", share != NULL ? share : "",
               data->auth_username != NULL ? data->auth_username : "",
               data->auth_workgroup != NULL ? data->auth_workgroup : "");

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

    p = url + 6; /* skip "smb://" */
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
    guint count = 0;
    stderr_silence_t sil;

    arr = g_ptr_array_new_with_free_func (smb_entry_free);

    stderr_silence_begin (&sil);
    smbc_set_context (data->ctx);
    errno = 0;
    dh = smbc_opendir (data->current_url);
    if (dh < 0)
    {
        stderr_silence_end (&sil);
        samba_log ("samba: opendir failed url='%s' errno=%d (%s)\n",
                   data->current_url != NULL ? data->current_url : "", errno, g_strerror (errno));
        return arr;
    }

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
        count++;
    }

    if (errno != 0)
        samba_log ("samba: readdir ended with errno=%d (%s), url='%s'\n", errno, g_strerror (errno),
                   data->current_url != NULL ? data->current_url : "");
    samba_log ("samba: loaded entries=%u url='%s' inside_share=%d\n", count,
               data->current_url != NULL ? data->current_url : "", inside_share ? 1 : 0);

    smbc_closedir (dh);
    stderr_silence_end (&sil);
    return arr;
}

/* --------------------------------------------------------------------------------------------- */

static char *
smb_url_up (const char *url)
{
    const char *after_scheme;
    const char *last_slash;

    after_scheme = url + 6; /* past "smb://" */

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
    return (smbc_type == SMBC_WORKGROUP || smbc_type == SMBC_SERVER || smbc_type == SMBC_FILE_SHARE
            || smbc_type == SMBC_DIR);
}

/* --------------------------------------------------------------------------------------------- */

static char *
smb_clean_component (const char *value)
{
    char *tmp;
    char *p;

    if (value == NULL)
        return g_strdup ("");

    tmp = g_strdup (value);
    for (p = tmp; *p != '\0'; p++)
        if (*p == '\\')
            *p = '/';

    if (g_str_has_prefix (tmp, "smb://"))
        p = tmp + strlen ("smb://");
    else
        p = tmp;

    while (*p == '/')
        p++;

    {
        char *result;

        result = g_strdup (p);
        g_free (tmp);
        g_strstrip (result);
        return result;
    }
}

/* --------------------------------------------------------------------------------------------- */

static char *
smb_normalize_share (const char *share_raw, const char *server_norm)
{
    char *s;
    size_t server_len;

    s = smb_clean_component (share_raw);
    server_len = strlen (server_norm);

    while (s[0] == '/')
        memmove (s, s + 1, strlen (s));

    if (server_len > 0 && g_ascii_strncasecmp (s, server_norm, server_len) == 0
        && s[server_len] == '/')
        memmove (s, s + server_len + 1, strlen (s + server_len + 1) + 1);

    while (s[0] == '/')
        memmove (s, s + 1, strlen (s));

    g_strstrip (s);
    return s;
}

/* --------------------------------------------------------------------------------------------- */

static void
smb_extract_host_and_path (const char *raw, char **host_out, char **path_out)
{
    char *clean;
    char *slash;

    clean = smb_clean_component (raw);
    slash = strchr (clean, '/');

    if (slash == NULL)
    {
        *host_out = g_strdup (clean);
        *path_out = g_strdup ("");
    }
    else
    {
        *slash = '\0';
        *host_out = g_strdup (clean);
        *path_out = g_strdup (slash + 1);
    }

    g_strstrip (*host_out);
    g_strstrip (*path_out);
    g_free (clean);
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
    char *server_norm;
    char *share_norm;
    char *srv_host = NULL;
    char *srv_path = NULL;
    char *sh_host = NULL;
    char *sh_path = NULL;
    char *tmp;

    set_auth_from_connection (data, conn);

    smb_extract_host_and_path (conn->server, &srv_host, &srv_path);
    smb_extract_host_and_path (conn->share, &sh_host, &sh_path);

    if (srv_host[0] != '\0')
        server_norm = g_strdup (srv_host);
    else
        server_norm = g_strdup (sh_host);

    if (sh_path[0] != '\0')
    {
        if (srv_path[0] != '\0')
            tmp = g_strdup_printf ("%s/%s", srv_path, sh_path);
        else
            tmp = g_strdup (sh_path);
    }
    else if (srv_path[0] != '\0')
        tmp = g_strdup (srv_path);
    else
        tmp = g_strdup (conn->share != NULL ? conn->share : "");

    share_norm = smb_normalize_share (tmp, server_norm);
    g_free (tmp);

    if (server_norm[0] == '\0')
    {
        samba_log (
            "samba: enter connection failed, empty normalized server from server='%s' share='%s'\n",
            conn->server != NULL ? conn->server : "", conn->share != NULL ? conn->share : "");
        g_free (srv_host);
        g_free (srv_path);
        g_free (sh_host);
        g_free (sh_path);
        g_free (server_norm);
        g_free (share_norm);
        return MC_PPR_FAILED;
    }

    g_free (data->current_url);
    if (share_norm[0] != '\0')
        data->current_url = g_strdup_printf ("smb://%s/%s", server_norm, share_norm);
    else
        data->current_url = g_strdup_printf ("smb://%s", server_norm);

    if (data->entries != NULL)
        g_ptr_array_free (data->entries, TRUE);

    samba_log ("samba: enter connection label='%s' server='%s' share='%s' user='%s' workgroup='%s' "
               "url='%s'\n",
               conn->label != NULL ? conn->label : "", server_norm, share_norm,
               conn->username != NULL ? conn->username : "",
               conn->workgroup != NULL ? conn->workgroup : "",
               data->current_url != NULL ? data->current_url : "");

    smbc_set_context (data->ctx);
    data->entries = smb_load_entries (data);
    samba_log ("samba: enter connection loaded %u entries\n",
               data->entries != NULL ? data->entries->len : 0);
    data->at_root = FALSE;
    samba_update_title (data);
    g_free (srv_host);
    g_free (srv_path);
    g_free (sh_host);
    g_free (sh_path);
    g_free (server_norm);
    g_free (share_norm);

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */
/* Connection dialog */
/* --------------------------------------------------------------------------------------------- */

static gboolean
show_connection_dialog (char **label, char **address, char **username, char **password)
{
    /* clang-format off */
    quick_widget_t quick_widgets[] = {
        QUICK_LABELED_INPUT (N_("Name:"), input_label_above,
                            *label != NULL ? *label : "", "smb-conn-label",
                            label, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
        QUICK_SEPARATOR (FALSE),
        QUICK_LABELED_INPUT (N_("Server/UNC path:"), input_label_above,
                            *address != NULL ? *address : "", "smb-conn-address",
                            address, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
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
        .title = N_ ("SMB Connection"),
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
    stderr_silence_t sil;

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
    samba_update_title (data);

    /* Load saved connections */
    data->connections_file = get_connections_file_path ();
    data->connections = load_connections (data->connections_file);
    samba_log ("samba: open, connections_file='%s', loaded=%u\n",
               data->connections_file != NULL ? data->connections_file : "",
               data->connections != NULL ? data->connections->len : 0);

    /* Create libsmbclient context */
    data->ctx = smbc_new_context ();
    if (data->ctx == NULL)
    {
        samba_log ("samba: smbc_new_context failed\n");
        g_ptr_array_free (data->connections, TRUE);
        g_free (data->connections_file);
        g_free (data);
        return NULL;
    }

    /* Keep libsmbclient debug disabled to avoid writing diagnostic text into mc TTY UI. */
    smbc_setDebug (data->ctx, 0);
    smbc_setOptionDebugToStderr (data->ctx, FALSE);
    samba_log ("samba: libsmbclient debug forced to 0\n");
    smbc_setFunctionAuthDataWithContext (data->ctx, smb_auth_cb);
    smbc_setOptionUserData (data->ctx, data);

    stderr_silence_begin (&sil);
    if (smbc_init_context (data->ctx) == NULL)
    {
        stderr_silence_end (&sil);
        samba_log ("samba: smbc_init_context failed errno=%d (%s)\n", errno, g_strerror (errno));
        smbc_free_context (data->ctx, 0);
        g_ptr_array_free (data->connections, TRUE);
        g_free (data->connections_file);
        g_free (data);
        return NULL;
    }
    stderr_silence_end (&sil);

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

    samba_log ("samba: chdir path='%s' at_root=%d current_url='%s'\n", path != NULL ? path : "",
               data->at_root ? 1 : 0, data->current_url != NULL ? data->current_url : "");

    if (strcmp (path, "..") == 0)
    {
        if (data->at_root)
            return MC_PPR_NOT_SUPPORTED; /* close plugin */

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
                samba_update_title (data);
                return MC_PPR_OK;
            }

            /* Navigate up within the connection */
            g_free (data->current_url);
            data->current_url = parent;

            if (data->entries != NULL)
                g_ptr_array_free (data->entries, TRUE);

            smbc_set_context (data->ctx);
            data->entries = smb_load_entries (data);
            samba_update_title (data);
            samba_log ("samba: chdir up to '%s', entries=%u\n",
                       data->current_url != NULL ? data->current_url : "",
                       data->entries != NULL ? data->entries->len : 0);
            return MC_PPR_OK;
        }
    }

    /* At root: enter a saved connection */
    if (data->at_root)
    {
        const smb_connection_t *conn;

        conn = find_connection (data, path);
        if (conn == NULL)
        {
            samba_log ("samba: chdir root connection '%s' not found\n", path != NULL ? path : "");
            return MC_PPR_FAILED;
        }

        return enter_connection (data, conn);
    }

    /* Inside connection: navigate into subdir/share */
    {
        const smb_entry_t *entry;
        char *new_url;

        entry = find_entry (data, path);
        if (entry == NULL || !smb_entry_is_dir (entry->smbc_type))
        {
            samba_log ("samba: chdir target '%s' is not a directory or missing\n",
                       path != NULL ? path : "");
            return MC_PPR_FAILED;
        }

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
        samba_update_title (data);
        samba_log ("samba: chdir entered '%s', entries=%u\n",
                   data->current_url != NULL ? data->current_url : "",
                   data->entries != NULL ? data->entries->len : 0);
        return MC_PPR_OK;
    }
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
samba_enter (void *plugin_data, const char *name, const struct stat *st)
{
    samba_data_t *data = (samba_data_t *) plugin_data;

    (void) st;
    samba_log ("samba: enter name='%s' at_root=%d current_url='%s'\n", name != NULL ? name : "",
               data->at_root ? 1 : 0, data->current_url != NULL ? data->current_url : "");

    /* At root: enter saved connection */
    if (data->at_root)
    {
        const smb_connection_t *conn;

        conn = find_connection (data, name);
        if (conn == NULL)
        {
            samba_log ("samba: enter root connection '%s' not found\n", name != NULL ? name : "");
            return MC_PPR_FAILED;
        }

        return enter_connection (data, conn);
    }

    /* Inside connection */
    {
        const smb_entry_t *entry;

        entry = find_entry (data, name);
        if (entry == NULL)
        {
            samba_log ("samba: enter '%s' not found in current entries\n",
                       name != NULL ? name : "");
            return MC_PPR_FAILED;
        }

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
            samba_update_title (data);
            samba_log ("samba: enter dir '%s', entries=%u\n",
                       data->current_url != NULL ? data->current_url : "",
                       data->entries != NULL ? data->entries->len : 0);
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
    stderr_silence_t sil;

    if (data->at_root || data->current_url == NULL)
        return MC_PPR_FAILED;

    smb_url = g_strdup_printf ("%s/%s", data->current_url, fname);
    samba_log ("samba: get_local_copy url='%s'\n", smb_url);

    stderr_silence_begin (&sil);
    smbc_set_context (data->ctx);
    errno = 0;
    smb_fd = smbc_open (smb_url, O_RDONLY, 0);
    g_free (smb_url);

    if (smb_fd < 0)
    {
        stderr_silence_end (&sil);
        samba_log ("samba: smbc_open failed errno=%d (%s)\n", errno, g_strerror (errno));
        return MC_PPR_FAILED;
    }

    local_fd = g_file_open_tmp ("mc-pp-smb-XXXXXX", local_path, &error);
    if (local_fd == -1)
    {
        if (error != NULL)
            g_error_free (error);
        smbc_close (smb_fd);
        stderr_silence_end (&sil);
        return MC_PPR_FAILED;
    }

    while ((n = smbc_read (smb_fd, buf, sizeof (buf))) > 0)
    {
        if (write (local_fd, buf, (size_t) n) != n)
        {
            samba_log ("samba: write local copy failed errno=%d (%s)\n", errno, g_strerror (errno));
            close (local_fd);
            smbc_close (smb_fd);
            unlink (*local_path);
            g_free (*local_path);
            *local_path = NULL;
            stderr_silence_end (&sil);
            return MC_PPR_FAILED;
        }
    }

    close (local_fd);
    smbc_close (smb_fd);

    if (n < 0)
    {
        samba_log ("samba: smbc_read failed errno=%d (%s)\n", errno, g_strerror (errno));
        unlink (*local_path);
        g_free (*local_path);
        *local_path = NULL;
        stderr_silence_end (&sil);
        return MC_PPR_FAILED;
    }

    stderr_silence_end (&sil);
    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
samba_delete_items (void *plugin_data, const char **names, int count)
{
    samba_data_t *data = (samba_data_t *) plugin_data;
    int i;
    stderr_silence_t sil;

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
        samba_log ("samba: deleted %d saved connections, now=%u\n", count, data->connections->len);
        return MC_PPR_OK;
    }

    /* Inside connection: delete files/dirs on SMB */
    stderr_silence_begin (&sil);
    smbc_set_context (data->ctx);

    for (i = 0; i < count; i++)
    {
        const smb_entry_t *entry;
        char *url;

        entry = find_entry (data, names[i]);
        url = g_strdup_printf ("%s/%s", data->current_url, names[i]);

        if (entry != NULL && entry->smbc_type == SMBC_DIR)
        {
            errno = 0;
            if (smbc_rmdir (url) != 0)
                samba_log ("samba: rmdir failed url='%s' errno=%d (%s)\n", url, errno,
                           g_strerror (errno));
        }
        else
        {
            errno = 0;
            if (smbc_unlink (url) != 0)
                samba_log ("samba: unlink failed url='%s' errno=%d (%s)\n", url, errno,
                           g_strerror (errno));
        }

        g_free (url);
    }

    if (data->entries != NULL)
        g_ptr_array_free (data->entries, TRUE);
    data->entries = smb_load_entries (data);
    stderr_silence_end (&sil);

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
samba_get_title (void *plugin_data)
{
    samba_data_t *data = (samba_data_t *) plugin_data;
    if (data->title_buf == NULL)
        samba_update_title (data);
    return data->title_buf;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
samba_create_item (void *plugin_data)
{
    samba_data_t *data = (samba_data_t *) plugin_data;
    smb_connection_t *conn;
    char *label = NULL;
    char *address = NULL;
    char *username = NULL;
    char *password = NULL;

    if (!data->at_root)
        return MC_PPR_NOT_SUPPORTED;

    if (!show_connection_dialog (&label, &address, &username, &password))
    {
        g_free (label);
        g_free (address);
        g_free (username);
        g_free (password);
        return MC_PPR_FAILED;
    }

    if (label == NULL || label[0] == '\0' || address == NULL || address[0] == '\0')
    {
        g_free (label);
        g_free (address);
        g_free (username);
        g_free (password);
        return MC_PPR_FAILED;
    }

    conn = g_new0 (smb_connection_t, 1);
    conn->label = label;
    conn->server = address;
    conn->share = g_strdup ("");
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
    samba_log ("samba: created connection label='%s' server='%s' share='%s' user='%s'\n",
               conn->label != NULL ? conn->label : "", conn->server != NULL ? conn->server : "",
               conn->share != NULL ? conn->share : "",
               conn->username != NULL ? conn->username : "");

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
