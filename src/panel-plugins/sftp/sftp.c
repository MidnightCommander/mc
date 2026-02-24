/*
   SFTP network browser panel plugin (libssh2).

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

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <libssh2.h>
#include <libssh2_sftp.h>

#include "lib/global.h"
#include "lib/panel-plugin.h"
#include "lib/vfs/utilvfs.h"
#include "lib/widget.h"

#include "src/filemanager/dir.h"

/*** file scope type declarations ****************************************************************/

typedef struct
{
    char *label;
    char *host;
    int port;
    char *user;
    char *path;
    char *password;
    char *pubkey;
    char *privkey;
    gboolean use_agent;
} sftp_connection_t;

typedef struct
{
    char *name;
    struct stat st;
    gboolean is_dir;
} sftp_entry_t;

typedef struct
{
    mc_panel_host_t *host;

    gboolean at_root;
    char *current_path;
    GPtrArray *entries;

    GPtrArray *connections;
    char *connections_file;

    sftp_connection_t *active_connection;

    int socket_handle;
    LIBSSH2_SESSION *session;
    LIBSSH2_SFTP *sftp_session;

    char *title_buf;
} sftp_data_t;

/*** forward declarations (file scope functions) *************************************************/

static void *sftp_open (mc_panel_host_t *host, const char *open_path);
static void sftp_close (void *plugin_data);
static mc_pp_result_t sftp_get_items (void *plugin_data, void *list_ptr);
static mc_pp_result_t sftp_chdir (void *plugin_data, const char *path);
static mc_pp_result_t sftp_enter (void *plugin_data, const char *name, const struct stat *st);
static mc_pp_result_t sftp_get_local_copy (void *plugin_data, const char *fname, char **local_path);
static mc_pp_result_t sftp_delete_items (void *plugin_data, const char **names, int count);
static const char *sftp_get_title (void *plugin_data);
static mc_pp_result_t sftp_create_item (void *plugin_data);
static void sftp_disconnect (sftp_data_t *data);

/*** file scope variables ************************************************************************/

#define SFTP_DEFAULT_PORT 22

#ifndef LIBSSH2_INVALID_SOCKET
#define LIBSSH2_INVALID_SOCKET -1
#endif

static guint sftp_libssh2_refcount = 0;

static const mc_panel_plugin_t sftp_plugin = {
    .api_version = MC_PANEL_PLUGIN_API_VERSION,
    .name = "sftp",
    .display_name = "SFTP network",
    .proto = "sftp",
    .prefix = NULL,
    .flags =
        MC_PPF_NAVIGATE | MC_PPF_GET_FILES | MC_PPF_DELETE | MC_PPF_CUSTOM_TITLE | MC_PPF_CREATE,

    .open = sftp_open,
    .close = sftp_close,
    .get_items = sftp_get_items,

    .chdir = sftp_chdir,
    .enter = sftp_enter,
    .get_local_copy = sftp_get_local_copy,
    .delete_items = sftp_delete_items,
    .get_title = sftp_get_title,
    .handle_key = NULL,
    .create_item = sftp_create_item,
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
sftp_entry_free (gpointer p)
{
    sftp_entry_t *e = (sftp_entry_t *) p;

    g_free (e->name);
    g_free (e);
}

/* --------------------------------------------------------------------------------------------- */

static void
sftp_connection_free (gpointer p)
{
    sftp_connection_t *c = (sftp_connection_t *) p;

    g_free (c->label);
    g_free (c->host);
    g_free (c->user);
    g_free (c->path);
    g_free (c->password);
    g_free (c->pubkey);
    g_free (c->privkey);
    g_free (c);
}

/* --------------------------------------------------------------------------------------------- */

static char *
get_connections_file_path (void)
{
    return g_build_filename (g_get_user_config_dir (), "mc", "sftp-connections.ini", (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

static GPtrArray *
load_connections (const char *filepath)
{
    GPtrArray *arr;
    GKeyFile *kf;
    gchar **groups;
    gsize n_groups, i;

    arr = g_ptr_array_new_with_free_func (sftp_connection_free);

    kf = g_key_file_new ();
    if (!g_key_file_load_from_file (kf, filepath, G_KEY_FILE_NONE, NULL))
    {
        g_key_file_free (kf);
        return arr;
    }

    groups = g_key_file_get_groups (kf, &n_groups);
    for (i = 0; i < n_groups; i++)
    {
        sftp_connection_t *conn;
        GError *error = NULL;

        conn = g_new0 (sftp_connection_t, 1);
        conn->label = g_strdup (groups[i]);
        conn->host = g_key_file_get_string (kf, groups[i], "host", NULL);
        conn->user = g_key_file_get_string (kf, groups[i], "user", NULL);
        conn->path = g_key_file_get_string (kf, groups[i], "path", NULL);
        conn->password = g_key_file_get_string (kf, groups[i], "password", NULL);
        conn->pubkey = g_key_file_get_string (kf, groups[i], "pubkey", NULL);
        conn->privkey = g_key_file_get_string (kf, groups[i], "privkey", NULL);

        conn->port = g_key_file_get_integer (kf, groups[i], "port", &error);
        if (error != NULL)
        {
            g_error_free (error);
            conn->port = SFTP_DEFAULT_PORT;
        }

        error = NULL;
        conn->use_agent = g_key_file_get_boolean (kf, groups[i], "use_agent", &error);
        if (error != NULL)
        {
            g_error_free (error);
            conn->use_agent = TRUE;
        }

        if (conn->host == NULL || conn->host[0] == '\0')
        {
            sftp_connection_free (conn);
            continue;
        }

        if (conn->port <= 0)
            conn->port = SFTP_DEFAULT_PORT;

        if (conn->user == NULL || conn->user[0] == '\0')
        {
            conn->user = vfs_get_local_username ();
            if (conn->user == NULL)
                conn->user = g_strdup (g_get_user_name ());
        }

        if (conn->path == NULL || conn->path[0] == '\0')
            conn->path = g_strdup ("/");

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
        const sftp_connection_t *conn =
            (const sftp_connection_t *) g_ptr_array_index (connections, i);

        g_key_file_set_string (kf, conn->label, "host", conn->host);
        g_key_file_set_integer (kf, conn->label, "port", conn->port > 0 ? conn->port : SFTP_DEFAULT_PORT);

        if (conn->user != NULL)
            g_key_file_set_string (kf, conn->label, "user", conn->user);
        if (conn->path != NULL)
            g_key_file_set_string (kf, conn->label, "path", conn->path);
        if (conn->password != NULL)
            g_key_file_set_string (kf, conn->label, "password", conn->password);
        if (conn->pubkey != NULL)
            g_key_file_set_string (kf, conn->label, "pubkey", conn->pubkey);
        if (conn->privkey != NULL)
            g_key_file_set_string (kf, conn->label, "privkey", conn->privkey);

        g_key_file_set_boolean (kf, conn->label, "use_agent", conn->use_agent);
    }

    data = g_key_file_to_data (kf, &length, NULL);
    g_key_file_free (kf);

    if (data == NULL)
        return FALSE;

    dir = g_path_get_dirname (filepath);
    g_mkdir_with_parents (dir, 0700);
    g_free (dir);

    ok = g_file_set_contents (filepath, data, (gssize) length, NULL);
    g_free (data);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

static const sftp_connection_t *
find_connection (const sftp_data_t *data, const char *label)
{
    guint i;

    for (i = 0; i < data->connections->len; i++)
    {
        const sftp_connection_t *c =
            (const sftp_connection_t *) g_ptr_array_index (data->connections, i);

        if (strcmp (c->label, label) == 0)
            return c;
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static const sftp_entry_t *
find_entry (const sftp_data_t *data, const char *name)
{
    guint i;

    if (data->entries == NULL)
        return NULL;

    for (i = 0; i < data->entries->len; i++)
    {
        const sftp_entry_t *e = (const sftp_entry_t *) g_ptr_array_index (data->entries, i);

        if (strcmp (e->name, name) == 0)
            return e;
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static char *
sftp_join_path (const char *base, const char *name)
{
    if (strcmp (base, "/") == 0)
        return g_strdup_printf ("/%s", name);

    return g_strdup_printf ("%s/%s", base, name);
}

/* --------------------------------------------------------------------------------------------- */

static char *
sftp_path_up (const char *path)
{
    const char *last;

    if (path == NULL || strcmp (path, "/") == 0)
        return NULL;

    last = strrchr (path, '/');
    if (last == NULL || last == path)
        return g_strdup ("/");

    return g_strndup (path, (gsize) (last - path));
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
sftp_auth_has_method (const char *auth_list, const char *method)
{
    const char *p;
    size_t method_len;

    if (auth_list == NULL || method == NULL)
        return FALSE;

    p = auth_list;
    method_len = strlen (method);

    while (*p != '\0')
    {
        const char *end;
        size_t token_len;

        end = strchr (p, ',');
        if (end == NULL)
            end = p + strlen (p);

        token_len = (size_t) (end - p);
        if (token_len == method_len && strncmp (p, method, method_len) == 0)
            return TRUE;

        if (*end == '\0')
            break;
        p = end + 1;
    }

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static int
sftp_open_socket (const sftp_connection_t *conn)
{
    struct addrinfo hints, *res = NULL, *curr;
    int sock = LIBSSH2_INVALID_SOCKET;
    char port_buf[BUF_TINY];

    if (conn->host == NULL || conn->host[0] == '\0')
        return LIBSSH2_INVALID_SOCKET;

    memset (&hints, 0, sizeof (hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    g_snprintf (port_buf, sizeof (port_buf), "%d", conn->port > 0 ? conn->port : SFTP_DEFAULT_PORT);

    if (getaddrinfo (conn->host, port_buf, &hints, &res) != 0)
        return LIBSSH2_INVALID_SOCKET;

    for (curr = res; curr != NULL; curr = curr->ai_next)
    {
        sock = socket (curr->ai_family, curr->ai_socktype, curr->ai_protocol);
        if (sock < 0)
            continue;

        if (connect (sock, curr->ai_addr, curr->ai_addrlen) == 0)
            break;

        close (sock);
        sock = LIBSSH2_INVALID_SOCKET;
    }

    freeaddrinfo (res);
    return sock;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
sftp_auth_agent (sftp_data_t *data, const char *user)
{
    LIBSSH2_AGENT *agent;
    struct libssh2_agent_publickey *identity = NULL;
    int rc;

    agent = libssh2_agent_init (data->session);
    if (agent == NULL)
        return FALSE;

    rc = libssh2_agent_connect (agent);
    if (rc != 0)
    {
        libssh2_agent_free (agent);
        return FALSE;
    }

    rc = libssh2_agent_list_identities (agent);
    if (rc != 0)
    {
        libssh2_agent_disconnect (agent);
        libssh2_agent_free (agent);
        return FALSE;
    }

    while (libssh2_agent_get_identity (agent, &identity, identity) == 0)
    {
        rc = libssh2_agent_userauth (agent, user, identity);
        if (rc == 0)
        {
            libssh2_agent_disconnect (agent);
            libssh2_agent_free (agent);
            return TRUE;
        }
    }

    libssh2_agent_disconnect (agent);
    libssh2_agent_free (agent);
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
sftp_connect (sftp_data_t *data, sftp_connection_t *conn)
{
    const char *user;
    const char *auth_list;

    if (data == NULL || conn == NULL)
        return FALSE;

    data->socket_handle = sftp_open_socket (conn);
    if (data->socket_handle == LIBSSH2_INVALID_SOCKET)
        return FALSE;

    data->session = libssh2_session_init ();
    if (data->session == NULL)
        goto fail;

    libssh2_session_set_blocking (data->session, 1);

    if (libssh2_session_handshake (data->session, (libssh2_socket_t) data->socket_handle) != 0)
        goto fail;

    user = (conn->user != NULL && conn->user[0] != '\0') ? conn->user : g_get_user_name ();
    auth_list = libssh2_userauth_list (data->session, user, (unsigned int) strlen (user));

    if (conn->use_agent && sftp_auth_has_method (auth_list, "publickey"))
    {
        if (sftp_auth_agent (data, user))
            goto auth_ok;
    }

    if (conn->privkey != NULL && conn->privkey[0] != '\0'
        && sftp_auth_has_method (auth_list, "publickey"))
    {
        if (libssh2_userauth_publickey_fromfile (data->session, user, conn->pubkey, conn->privkey,
                                                 conn->password) == 0)
            goto auth_ok;
    }

    if (conn->password != NULL && conn->password[0] != '\0' && sftp_auth_has_method (auth_list, "password"))
    {
        if (libssh2_userauth_password (data->session, user, conn->password) == 0)
            goto auth_ok;
    }

    if (sftp_auth_has_method (auth_list, "password"))
    {
        char *pwd;
        char *prompt;

        prompt = g_strdup_printf (_ ("Enter password for %s@%s"), user, conn->host);
        pwd = input_dialog (_ ("SFTP password"), prompt, "sftp-password", INPUT_PASSWORD,
                            INPUT_COMPLETE_NONE);
        g_free (prompt);

        if (pwd != NULL && pwd[0] != '\0'
            && libssh2_userauth_password (data->session, user, pwd) == 0)
        {
            g_free (conn->password);
            conn->password = pwd;
            goto auth_ok;
        }

        g_free (pwd);
    }

    goto fail;

auth_ok:
    data->sftp_session = libssh2_sftp_init (data->session);
    if (data->sftp_session == NULL)
        goto fail;

    data->active_connection = conn;
    return TRUE;

fail:
    sftp_disconnect (data);
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static void
sftp_disconnect (sftp_data_t *data)
{
    if (data->sftp_session != NULL)
    {
        libssh2_sftp_shutdown (data->sftp_session);
        data->sftp_session = NULL;
    }

    if (data->session != NULL)
    {
        libssh2_session_disconnect (data->session, "Normal Shutdown");
        libssh2_session_free (data->session);
        data->session = NULL;
    }

    if (data->socket_handle != LIBSSH2_INVALID_SOCKET)
    {
        close (data->socket_handle);
        data->socket_handle = LIBSSH2_INVALID_SOCKET;
    }

    data->active_connection = NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
sftp_attr_to_stat (const LIBSSH2_SFTP_ATTRIBUTES *attrs, struct stat *st)
{
    memset (st, 0, sizeof (*st));

    st->st_uid = getuid ();
    st->st_gid = getgid ();
    st->st_nlink = 1;

    if ((attrs->flags & LIBSSH2_SFTP_ATTR_UIDGID) != 0)
    {
        st->st_uid = attrs->uid;
        st->st_gid = attrs->gid;
    }

    if ((attrs->flags & LIBSSH2_SFTP_ATTR_SIZE) != 0)
        st->st_size = (off_t) attrs->filesize;

    if ((attrs->flags & LIBSSH2_SFTP_ATTR_ACMODTIME) != 0)
    {
        st->st_atime = attrs->atime;
        st->st_mtime = attrs->mtime;
        st->st_ctime = attrs->mtime;
    }
    else
    {
        st->st_mtime = time (NULL);
    }

    if ((attrs->flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) != 0)
        st->st_mode = attrs->permissions;
}

/* --------------------------------------------------------------------------------------------- */

static GPtrArray *
sftp_load_entries (sftp_data_t *data)
{
    GPtrArray *arr;
    LIBSSH2_SFTP_HANDLE *dirh;

    arr = g_ptr_array_new_with_free_func (sftp_entry_free);

    dirh = libssh2_sftp_opendir (data->sftp_session, data->current_path);
    if (dirh == NULL)
        return arr;

    while (TRUE)
    {
        char mem[BUF_MEDIUM];
        LIBSSH2_SFTP_ATTRIBUTES attrs;
        ssize_t rc;

        rc = libssh2_sftp_readdir_ex (dirh, mem, sizeof (mem), NULL, 0, &attrs);
        if (rc <= 0)
            break;

        if ((size_t) rc >= sizeof (mem))
            rc = (ssize_t) sizeof (mem) - 1;
        mem[rc] = '\0';

        if (strcmp (mem, ".") == 0 || strcmp (mem, "..") == 0)
            continue;

        {
            sftp_entry_t *entry;
            mode_t mode;

            entry = g_new0 (sftp_entry_t, 1);
            entry->name = g_strdup (mem);
            sftp_attr_to_stat (&attrs, &entry->st);

            mode = entry->st.st_mode;
            if (!S_ISDIR (mode) && !S_ISREG (mode) && !S_ISLNK (mode))
            {
                mode = S_IFREG | 0644;
                entry->st.st_mode = mode;
            }

            entry->is_dir = S_ISDIR (entry->st.st_mode);

            g_ptr_array_add (arr, entry);
        }
    }

    libssh2_sftp_closedir (dirh);
    return arr;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
sftp_reload_entries (sftp_data_t *data)
{
    if (data->entries != NULL)
    {
        g_ptr_array_free (data->entries, TRUE);
        data->entries = NULL;
    }

    if (data->at_root)
        return TRUE;

    if (data->sftp_session == NULL)
        return FALSE;

    data->entries = sftp_load_entries (data);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
show_connection_dialog (char **label, char **host, char **port, char **user, char **path,
                        char **password, char **pubkey, char **privkey, gboolean *use_agent)
{
    /* clang-format off */
    quick_widget_t quick_widgets[] = {
        QUICK_LABELED_INPUT (N_("Name:"), input_label_above,
                            *label != NULL ? *label : "", "sftp-conn-label",
                            label, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
        QUICK_SEPARATOR (FALSE),
        QUICK_LABELED_INPUT (N_("Host:"), input_label_above,
                            *host != NULL ? *host : "", "sftp-conn-host",
                            host, NULL, FALSE, FALSE, INPUT_COMPLETE_HOSTNAMES),
        QUICK_SEPARATOR (FALSE),
        QUICK_LABELED_INPUT (N_("Port:"), input_label_above,
                            *port != NULL ? *port : "22", "sftp-conn-port",
                            port, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
        QUICK_SEPARATOR (FALSE),
        QUICK_LABELED_INPUT (N_("User:"), input_label_above,
                            *user != NULL ? *user : "", "sftp-conn-user",
                            user, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
        QUICK_SEPARATOR (FALSE),
        QUICK_LABELED_INPUT (N_("Remote path:"), input_label_above,
                            *path != NULL ? *path : "/", "sftp-conn-path",
                            path, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
        QUICK_SEPARATOR (FALSE),
        QUICK_LABELED_INPUT (N_("Password:"), input_label_above,
                            *password != NULL ? *password : "", "sftp-conn-pass",
                            password, NULL, TRUE, TRUE, INPUT_COMPLETE_NONE),
        QUICK_SEPARATOR (FALSE),
        QUICK_LABELED_INPUT (N_("Public key file:"), input_label_above,
                            *pubkey != NULL ? *pubkey : "", "sftp-conn-pubkey",
                            pubkey, NULL, FALSE, FALSE, INPUT_COMPLETE_FILENAMES),
        QUICK_SEPARATOR (FALSE),
        QUICK_LABELED_INPUT (N_("Private key file:"), input_label_above,
                            *privkey != NULL ? *privkey : "", "sftp-conn-privkey",
                            privkey, NULL, FALSE, FALSE, INPUT_COMPLETE_FILENAMES),
        QUICK_SEPARATOR (FALSE),
        QUICK_CHECKBOX (N_("Use SSH &agent"), use_agent, NULL),
        QUICK_BUTTONS_OK_CANCEL,
        QUICK_END,
    };
    /* clang-format on */

    WRect r = { -1, -1, 0, 56 };

    quick_dialog_t qdlg = {
        .rect = r,
        .title = N_ ("SFTP Connection"),
        .help = "[SFTP Plugin]",
        .widgets = quick_widgets,
        .callback = NULL,
        .mouse_callback = NULL,
    };

    return (quick_dialog (&qdlg) == B_ENTER);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
sftp_activate_connection (sftp_data_t *data, sftp_connection_t *conn)
{
    char *path;

    sftp_disconnect (data);

    if (!sftp_connect (data, conn))
        return FALSE;

    path = (conn->path != NULL && conn->path[0] != '\0') ? g_strdup (conn->path) : g_strdup ("/");
    if (path[0] != '/')
    {
        char *tmp = g_strdup_printf ("/%s", path);
        g_free (path);
        path = tmp;
    }

    g_free (data->current_path);
    data->current_path = path;
    data->at_root = FALSE;

    sftp_reload_entries (data);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* Plugin callbacks */
/* --------------------------------------------------------------------------------------------- */

static void *
sftp_open (mc_panel_host_t *host, const char *open_path)
{
    sftp_data_t *data;

    (void) open_path;

    if (sftp_libssh2_refcount == 0)
    {
        if (libssh2_init (0) != 0)
            return NULL;
    }
    sftp_libssh2_refcount++;

    data = g_new0 (sftp_data_t, 1);
    data->host = host;
    data->at_root = TRUE;
    data->current_path = NULL;
    data->entries = NULL;
    data->title_buf = NULL;

    data->socket_handle = LIBSSH2_INVALID_SOCKET;
    data->session = NULL;
    data->sftp_session = NULL;
    data->active_connection = NULL;

    data->connections_file = get_connections_file_path ();
    data->connections = load_connections (data->connections_file);

    return data;
}

/* --------------------------------------------------------------------------------------------- */

static void
sftp_close (void *plugin_data)
{
    sftp_data_t *data = (sftp_data_t *) plugin_data;

    sftp_disconnect (data);

    if (data->entries != NULL)
        g_ptr_array_free (data->entries, TRUE);

    g_ptr_array_free (data->connections, TRUE);

    g_free (data->current_path);
    g_free (data->title_buf);
    g_free (data->connections_file);
    g_free (data);

    if (sftp_libssh2_refcount > 0)
        sftp_libssh2_refcount--;

    if (sftp_libssh2_refcount == 0)
        libssh2_exit ();
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
sftp_get_items (void *plugin_data, void *list_ptr)
{
    dir_list *list = (dir_list *) list_ptr;
    sftp_data_t *data = (sftp_data_t *) plugin_data;
    guint i;

    if (data->at_root)
    {
        for (i = 0; i < data->connections->len; i++)
        {
            const sftp_connection_t *conn =
                (const sftp_connection_t *) g_ptr_array_index (data->connections, i);

            add_entry (list, conn->label, S_IFDIR | 0755, 0, time (NULL));
        }
        return MC_PPR_OK;
    }

    if (data->entries != NULL)
    {
        for (i = 0; i < data->entries->len; i++)
        {
            const sftp_entry_t *e = (const sftp_entry_t *) g_ptr_array_index (data->entries, i);
            mode_t mode;
            off_t size;

            mode = e->st.st_mode;
            if (!S_ISDIR (mode) && !S_ISREG (mode) && !S_ISLNK (mode))
                mode = S_IFREG | 0644;

            size = S_ISDIR (mode) ? 0 : e->st.st_size;
            add_entry (list, e->name, mode, size, e->st.st_mtime != 0 ? e->st.st_mtime : time (NULL));
        }
    }

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
sftp_chdir (void *plugin_data, const char *path)
{
    sftp_data_t *data = (sftp_data_t *) plugin_data;

    if (strcmp (path, "..") == 0)
    {
        if (data->at_root)
            return MC_PPR_NOT_SUPPORTED;

        if (data->current_path != NULL && strcmp (data->current_path, "/") == 0)
        {
            data->at_root = TRUE;
            sftp_disconnect (data);

            if (data->entries != NULL)
            {
                g_ptr_array_free (data->entries, TRUE);
                data->entries = NULL;
            }

            g_free (data->current_path);
            data->current_path = NULL;
            return MC_PPR_OK;
        }

        {
            char *parent = sftp_path_up (data->current_path);

            if (parent == NULL)
            {
                data->at_root = TRUE;
                sftp_disconnect (data);

                if (data->entries != NULL)
                {
                    g_ptr_array_free (data->entries, TRUE);
                    data->entries = NULL;
                }

                g_free (data->current_path);
                data->current_path = NULL;
                return MC_PPR_OK;
            }

            g_free (data->current_path);
            data->current_path = parent;
            sftp_reload_entries (data);
            return MC_PPR_OK;
        }
    }

    if (data->at_root)
    {
        sftp_connection_t *conn = (sftp_connection_t *) find_connection (data, path);

        if (conn == NULL)
            return MC_PPR_FAILED;

        return sftp_activate_connection (data, conn) ? MC_PPR_OK : MC_PPR_FAILED;
    }

    {
        const sftp_entry_t *entry;
        char *new_path;

        entry = find_entry (data, path);
        if (entry == NULL || !entry->is_dir)
            return MC_PPR_FAILED;

        new_path = sftp_join_path (data->current_path, path);

        g_free (data->current_path);
        data->current_path = new_path;

        sftp_reload_entries (data);
        return MC_PPR_OK;
    }
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
sftp_enter (void *plugin_data, const char *name, const struct stat *st)
{
    sftp_data_t *data = (sftp_data_t *) plugin_data;

    (void) st;

    if (data->at_root)
    {
        sftp_connection_t *conn = (sftp_connection_t *) find_connection (data, name);

        if (conn == NULL)
            return MC_PPR_FAILED;

        return sftp_activate_connection (data, conn) ? MC_PPR_OK : MC_PPR_FAILED;
    }

    {
        const sftp_entry_t *entry;

        entry = find_entry (data, name);
        if (entry == NULL)
            return MC_PPR_FAILED;

        if (entry->is_dir)
        {
            char *new_path;

            new_path = sftp_join_path (data->current_path, name);
            g_free (data->current_path);
            data->current_path = new_path;

            sftp_reload_entries (data);
            return MC_PPR_OK;
        }
    }

    return MC_PPR_NOT_SUPPORTED;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
sftp_get_local_copy (void *plugin_data, const char *fname, char **local_path)
{
    sftp_data_t *data = (sftp_data_t *) plugin_data;
    LIBSSH2_SFTP_HANDLE *fileh;
    char *remote_path;
    int local_fd;
    GError *error = NULL;

    if (data->at_root || data->sftp_session == NULL || data->current_path == NULL)
        return MC_PPR_FAILED;

    remote_path = sftp_join_path (data->current_path, fname);
    fileh = libssh2_sftp_open (data->sftp_session, remote_path, LIBSSH2_FXF_READ, 0);
    g_free (remote_path);

    if (fileh == NULL)
        return MC_PPR_FAILED;

    local_fd = g_file_open_tmp ("mc-pp-sftp-XXXXXX", local_path, &error);
    if (local_fd == -1)
    {
        if (error != NULL)
            g_error_free (error);
        libssh2_sftp_close (fileh);
        return MC_PPR_FAILED;
    }

    while (TRUE)
    {
        char buf[8192];
        ssize_t n;

        n = libssh2_sftp_read (fileh, buf, sizeof (buf));
        if (n == 0)
            break;

        if (n < 0 || write (local_fd, buf, (size_t) n) != n)
        {
            close (local_fd);
            libssh2_sftp_close (fileh);
            unlink (*local_path);
            g_free (*local_path);
            *local_path = NULL;
            return MC_PPR_FAILED;
        }
    }

    close (local_fd);
    libssh2_sftp_close (fileh);

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
sftp_delete_items (void *plugin_data, const char **names, int count)
{
    sftp_data_t *data = (sftp_data_t *) plugin_data;
    int i;
    gboolean failed = FALSE;

    if (data->at_root)
    {
        for (i = 0; i < count; i++)
        {
            guint j;

            for (j = 0; j < data->connections->len; j++)
            {
                const sftp_connection_t *conn =
                    (const sftp_connection_t *) g_ptr_array_index (data->connections, j);

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

    if (data->sftp_session == NULL)
        return MC_PPR_FAILED;

    for (i = 0; i < count; i++)
    {
        const sftp_entry_t *entry;
        char *remote_path;
        int rc;

        entry = find_entry (data, names[i]);
        if (entry == NULL)
            continue;

        remote_path = sftp_join_path (data->current_path, names[i]);

        if (entry->is_dir)
            rc = libssh2_sftp_rmdir_ex (data->sftp_session, remote_path, (unsigned int) strlen (remote_path));
        else
            rc = libssh2_sftp_unlink_ex (data->sftp_session, remote_path,
                                         (unsigned int) strlen (remote_path));

        if (rc != 0)
            failed = TRUE;

        g_free (remote_path);
    }

    sftp_reload_entries (data);

    return failed ? MC_PPR_FAILED : MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
sftp_get_title (void *plugin_data)
{
    sftp_data_t *data = (sftp_data_t *) plugin_data;

    g_free (data->title_buf);

    if (data->at_root || data->active_connection == NULL)
    {
        data->title_buf = g_strdup ("/");
        return data->title_buf;
    }

    data->title_buf = g_strdup_printf ("%s:%s", data->active_connection->host,
                                       data->current_path != NULL ? data->current_path : "/");

    return data->title_buf;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
sftp_create_item (void *plugin_data)
{
    sftp_data_t *data = (sftp_data_t *) plugin_data;
    sftp_connection_t *conn;

    char *label = NULL;
    char *host = NULL;
    char *port = g_strdup ("22");
    char *user = vfs_get_local_username ();
    char *path = g_strdup ("/");
    char *password = NULL;
    char *pubkey = NULL;
    char *privkey = NULL;
    gboolean use_agent = TRUE;

    if (user == NULL)
        user = g_strdup (g_get_user_name ());

    if (!data->at_root)
        return MC_PPR_NOT_SUPPORTED;

    if (!show_connection_dialog (&label, &host, &port, &user, &path, &password, &pubkey, &privkey,
                                 &use_agent))
    {
        g_free (label);
        g_free (host);
        g_free (port);
        g_free (user);
        g_free (path);
        g_free (password);
        g_free (pubkey);
        g_free (privkey);
        return MC_PPR_FAILED;
    }

    if (label == NULL || label[0] == '\0' || host == NULL || host[0] == '\0')
    {
        g_free (label);
        g_free (host);
        g_free (port);
        g_free (user);
        g_free (path);
        g_free (password);
        g_free (pubkey);
        g_free (privkey);
        return MC_PPR_FAILED;
    }

    conn = g_new0 (sftp_connection_t, 1);
    conn->label = label;
    conn->host = host;
    conn->port = (port != NULL && port[0] != '\0') ? atoi (port) : SFTP_DEFAULT_PORT;
    if (conn->port <= 0)
        conn->port = SFTP_DEFAULT_PORT;

    conn->user = user;
    conn->path = path;

    conn->password = (password != NULL && password[0] != '\0') ? password : NULL;
    if (conn->password == NULL)
        g_free (password);

    conn->pubkey = (pubkey != NULL && pubkey[0] != '\0') ? pubkey : NULL;
    if (conn->pubkey == NULL)
        g_free (pubkey);

    conn->privkey = (privkey != NULL && privkey[0] != '\0') ? privkey : NULL;
    if (conn->privkey == NULL)
        g_free (privkey);

    conn->use_agent = use_agent;

    g_free (port);

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
    return &sftp_plugin;
}

/* --------------------------------------------------------------------------------------------- */
