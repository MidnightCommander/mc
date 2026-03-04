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
#include "lib/keybind.h"
#include "lib/mcconfig.h"
#include "lib/panel-plugin.h"
#include "lib/vfs/utilvfs.h"
#include "lib/widget.h"

#include "src/viewer/mcviewer.h"

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

    /* Timeouts */
    int timeout;         /* response timeout, sec (0 = 30 default) */
    int connect_timeout; /* connect timeout, sec (0 = 10 default) */

    /* Keepalive */
    gboolean keepalive;     /* SSH keepalive */
    int keepalive_interval; /* keepalive interval, sec (0 = 60 default) */

    /* IP version */
    int ip_version; /* 0=auto, 4=IPv4, 6=IPv6 */
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

    int key_edit;

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
static mc_pp_result_t sftp_put_file (void *plugin_data, const char *local_path,
                                     const char *dest_name);
static mc_pp_result_t sftp_delete_items (void *plugin_data, const char **names, int count);
static const char *sftp_get_title (void *plugin_data);
static mc_pp_result_t sftp_create_item (void *plugin_data);
static mc_pp_result_t sftp_view_item (void *plugin_data, const char *fname, const struct stat *st,
                                      gboolean plain_view);
static mc_pp_result_t sftp_handle_key (void *plugin_data, int key);
static void sftp_disconnect (sftp_data_t *data);

/*** file scope variables ************************************************************************/

#define SFTP_DEFAULT_PORT           22

#define SFTP_PANEL_CONFIG_FILE      "panels.sftp.ini"
#define SFTP_PANEL_CONFIG_GROUP     "sftp-panel"
#define SFTP_PANEL_KEY_EDIT         "hotkey_edit"
#define SFTP_PANEL_KEY_EDIT_DEFAULT "f4"

/* KEY_F(n) = 1000 + n, XCTRL(c) = c & 0x1f — matching lib/tty definitions */
#define SFTP_KEY_F(n) (1000 + (n))
#define SFTP_XCTRL(c) ((c) & 0x1f)

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
    .flags = MC_PPF_NAVIGATE | MC_PPF_GET_FILES | MC_PPF_DELETE | MC_PPF_CUSTOM_TITLE
        | MC_PPF_CREATE | MC_PPF_PUT_FILES | MC_PPF_SHOW_IN_MENU,

    .open = sftp_open,
    .close = sftp_close,
    .get_items = sftp_get_items,

    .chdir = sftp_chdir,
    .enter = sftp_enter,
    .get_local_copy = sftp_get_local_copy,
    .put_file = sftp_put_file,
    .save_file = sftp_put_file,
    .delete_items = sftp_delete_items,
    .get_title = sftp_get_title,
    .view = sftp_view_item,
    .handle_key = sftp_handle_key,
    .create_item = sftp_create_item,
};

/*** file scope functions ************************************************************************/

static void
sftp_entry_free (gpointer p)
{
    sftp_entry_t *e = (sftp_entry_t *) p;

    g_free (e->name);
    g_free (e);
}

/* --------------------------------------------------------------------------------------------- */

static char *
sftp_read_config_string (const char *path, const char *key)
{
    char *value;
    mc_config_t *cfg;

    if (path == NULL || !g_file_test (path, G_FILE_TEST_IS_REGULAR))
        return NULL;

    cfg = mc_config_init (path, TRUE);
    if (cfg == NULL)
        return NULL;

    value = mc_config_get_string (cfg, SFTP_PANEL_CONFIG_GROUP, key, NULL);
    mc_config_deinit (cfg);

    if (value == NULL)
        return NULL;

    g_strstrip (value);
    if (*value == '\0')
    {
        g_free (value);
        return NULL;
    }

    return value;
}

/* --------------------------------------------------------------------------------------------- */

static void
sftp_save_config_defaults (const char *path)
{
    mc_config_t *cfg;

    if (path == NULL || g_file_test (path, G_FILE_TEST_EXISTS))
        return;

    cfg = mc_config_init (path, FALSE);
    if (cfg == NULL)
        return;

    mc_config_set_string (cfg, SFTP_PANEL_CONFIG_GROUP, SFTP_PANEL_KEY_EDIT,
                          SFTP_PANEL_KEY_EDIT_DEFAULT);
    mc_config_save_file (cfg, NULL);
    mc_config_deinit (cfg);
}

/* --------------------------------------------------------------------------------------------- */

static int
sftp_parse_hotkey (const char *value, int fallback)
{
    char *s;
    int ret = fallback;

    if (value == NULL || value[0] == '\0')
        return fallback;

    s = g_ascii_strdown (value, -1);
    g_strstrip (s);

    if (s[0] == '\0')
    {
        g_free (s);
        return fallback;
    }

    if (strcmp (s, "none") == 0)
        ret = -1;
    else if (strncmp (s, "ctrl-", 5) == 0 && s[5] != '\0' && s[6] == '\0'
             && g_ascii_isalpha ((guchar) s[5]))
        ret = SFTP_XCTRL (g_ascii_tolower ((guchar) s[5]));
    else if (s[0] == 'f' && s[1] != '\0' && g_ascii_isdigit ((guchar) s[1]) != 0)
    {
        int n = atoi (s + 1);

        if (n >= 1 && n <= 24)
            ret = SFTP_KEY_F (n);
    }
    else if ((strncmp (s, "shift-f", 7) == 0 || strncmp (s, "s-f", 3) == 0)
             && g_ascii_isdigit ((guchar) s[strncmp (s, "shift-f", 7) == 0 ? 7 : 3]) != 0)
    {
        int base = atoi (s + (strncmp (s, "shift-f", 7) == 0 ? 7 : 3));

        if (base >= 1 && base <= 12)
            ret = SFTP_KEY_F (base + 10);
    }

    g_free (s);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static int
sftp_load_hotkey (const char *key, const char *fallback_text, int fallback_key)
{
    char *config_path;
    char *value;
    int hotkey;

    config_path = g_build_filename (mc_config_get_path (), SFTP_PANEL_CONFIG_FILE, (char *) NULL);

    value = sftp_read_config_string (config_path, key);
    if (value == NULL && mc_global.sysconfig_dir != NULL)
    {
        char *sys_path;

        sys_path =
            g_build_filename (mc_global.sysconfig_dir, SFTP_PANEL_CONFIG_FILE, (char *) NULL);
        value = sftp_read_config_string (sys_path, key);
        g_free (sys_path);
    }

    sftp_save_config_defaults (config_path);
    g_free (config_path);

    if (value == NULL)
        value = g_strdup (fallback_text);

    hotkey = sftp_parse_hotkey (value, fallback_key);
    g_free (value);
    return hotkey;
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

static sftp_connection_t *
sftp_connection_dup (const sftp_connection_t *src)
{
    sftp_connection_t *d;

    d = g_new0 (sftp_connection_t, 1);
    d->label = g_strdup (src->label);
    d->host = g_strdup (src->host);
    d->port = src->port;
    d->user = g_strdup (src->user);
    d->path = g_strdup (src->path);
    d->password = g_strdup (src->password);
    d->pubkey = g_strdup (src->pubkey);
    d->privkey = g_strdup (src->privkey);
    d->use_agent = src->use_agent;
    d->timeout = src->timeout;
    d->connect_timeout = src->connect_timeout;
    d->keepalive = src->keepalive;
    d->keepalive_interval = src->keepalive_interval;
    d->ip_version = src->ip_version;
    return d;
}

/* --------------------------------------------------------------------------------------------- */

static void
sftp_connection_copy_from (sftp_connection_t *dst, const sftp_connection_t *src)
{
    g_free (dst->label);
    g_free (dst->host);
    g_free (dst->user);
    g_free (dst->path);
    g_free (dst->password);
    g_free (dst->pubkey);
    g_free (dst->privkey);

    dst->label = g_strdup (src->label);
    dst->host = g_strdup (src->host);
    dst->port = src->port;
    dst->user = g_strdup (src->user);
    dst->path = g_strdup (src->path);
    dst->password = g_strdup (src->password);
    dst->pubkey = g_strdup (src->pubkey);
    dst->privkey = g_strdup (src->privkey);
    dst->use_agent = src->use_agent;
    dst->timeout = src->timeout;
    dst->connect_timeout = src->connect_timeout;
    dst->keepalive = src->keepalive;
    dst->keepalive_interval = src->keepalive_interval;
    dst->ip_version = src->ip_version;
}

/* --------------------------------------------------------------------------------------------- */

/* Simple XOR obfuscation to avoid storing passwords in plain text.
   NOT cryptographically secure — just prevents casual reading. */

static const unsigned char sftp_obfuscation_key[] = "Mc4SftpPanelKey!";

static char *
sftp_password_encode (const char *plain)
{
    size_t i, len, klen;
    guchar *xored;
    gchar *b64;
    char *result;

    if (plain == NULL || plain[0] == '\0')
        return NULL;

    len = strlen (plain);
    klen = sizeof (sftp_obfuscation_key) - 1;
    xored = g_new (guchar, len);

    for (i = 0; i < len; i++)
        xored[i] = (guchar) plain[i] ^ sftp_obfuscation_key[i % klen];

    b64 = g_base64_encode (xored, len);
    g_free (xored);

    result = g_strdup_printf ("enc:%s", b64);
    g_free (b64);

    return result;
}

/* --------------------------------------------------------------------------------------------- */

static char *
sftp_password_decode (const char *encoded)
{
    guchar *xored;
    gsize len;
    size_t i, klen;
    char *plain;

    if (encoded == NULL)
        return NULL;

    /* backward compatibility: plain text without "enc:" prefix */
    if (strncmp (encoded, "enc:", 4) != 0)
        return g_strdup (encoded);

    xored = g_base64_decode (encoded + 4, &len);
    if (xored == NULL || len == 0)
    {
        g_free (xored);
        return g_strdup ("");
    }

    klen = sizeof (sftp_obfuscation_key) - 1;
    plain = g_new (char, len + 1);

    for (i = 0; i < len; i++)
        plain[i] = (char) (xored[i] ^ sftp_obfuscation_key[i % klen]);
    plain[len] = '\0';

    g_free (xored);
    return plain;
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
        conn->pubkey = g_key_file_get_string (kf, groups[i], "pubkey", NULL);
        conn->privkey = g_key_file_get_string (kf, groups[i], "privkey", NULL);

        {
            char *raw_pw = g_key_file_get_string (kf, groups[i], "password", NULL);

            conn->password = sftp_password_decode (raw_pw);
            g_free (raw_pw);
        }

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

        error = NULL;
        conn->timeout = g_key_file_get_integer (kf, groups[i], "timeout", &error);
        if (error != NULL)
        {
            g_error_free (error);
            conn->timeout = 0;
        }

        error = NULL;
        conn->connect_timeout = g_key_file_get_integer (kf, groups[i], "connect_timeout", &error);
        if (error != NULL)
        {
            g_error_free (error);
            conn->connect_timeout = 0;
        }

        error = NULL;
        conn->keepalive = g_key_file_get_boolean (kf, groups[i], "keepalive", &error);
        if (error != NULL)
        {
            g_error_free (error);
            conn->keepalive = FALSE;
        }

        error = NULL;
        conn->keepalive_interval =
            g_key_file_get_integer (kf, groups[i], "keepalive_interval", &error);
        if (error != NULL)
        {
            g_error_free (error);
            conn->keepalive_interval = 0;
        }

        error = NULL;
        conn->ip_version = g_key_file_get_integer (kf, groups[i], "ip_version", &error);
        if (error != NULL)
        {
            g_error_free (error);
            conn->ip_version = 0;
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
        g_key_file_set_integer (kf, conn->label, "port",
                                conn->port > 0 ? conn->port : SFTP_DEFAULT_PORT);

        if (conn->user != NULL)
            g_key_file_set_string (kf, conn->label, "user", conn->user);
        if (conn->path != NULL)
            g_key_file_set_string (kf, conn->label, "path", conn->path);
        if (conn->password != NULL && conn->password[0] != '\0')
        {
            char *enc = sftp_password_encode (conn->password);

            if (enc != NULL)
            {
                g_key_file_set_string (kf, conn->label, "password", enc);
                g_free (enc);
            }
        }
        if (conn->pubkey != NULL)
            g_key_file_set_string (kf, conn->label, "pubkey", conn->pubkey);
        if (conn->privkey != NULL)
            g_key_file_set_string (kf, conn->label, "privkey", conn->privkey);

        g_key_file_set_boolean (kf, conn->label, "use_agent", conn->use_agent);

        if (conn->timeout > 0)
            g_key_file_set_integer (kf, conn->label, "timeout", conn->timeout);
        if (conn->connect_timeout > 0)
            g_key_file_set_integer (kf, conn->label, "connect_timeout", conn->connect_timeout);
        g_key_file_set_boolean (kf, conn->label, "keepalive", conn->keepalive);
        if (conn->keepalive_interval > 0)
            g_key_file_set_integer (kf, conn->label, "keepalive_interval",
                                    conn->keepalive_interval);
        if (conn->ip_version != 0)
            g_key_file_set_integer (kf, conn->label, "ip_version", conn->ip_version);
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
    if (conn->ip_version == 4)
        hints.ai_family = AF_INET;
    else if (conn->ip_version == 6)
        hints.ai_family = AF_INET6;
    else
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

        /* Apply connect timeout via SO_SNDTIMEO */
        if (conn->connect_timeout > 0)
        {
            struct timeval tv;

            tv.tv_sec = conn->connect_timeout;
            tv.tv_usec = 0;
            setsockopt (sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof (tv));
        }

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

    /* Timeouts */
    {
        long tout = (conn->timeout > 0) ? (long) conn->timeout * 1000 : 30000;

        libssh2_session_set_timeout (data->session, tout);
    }

    if (libssh2_session_handshake (data->session, (libssh2_socket_t) data->socket_handle) != 0)
        goto fail;

    /* Keepalive */
    if (conn->keepalive)
    {
        int interval = conn->keepalive_interval > 0 ? conn->keepalive_interval : 60;

        libssh2_keepalive_config (data->session, 1, (unsigned int) interval);
    }

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
                                                 conn->password)
            == 0)
            goto auth_ok;
    }

    if (conn->password != NULL && conn->password[0] != '\0'
        && sftp_auth_has_method (auth_list, "password"))
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

#define SFTP_TAB_BASIC      (B_USER + 0)
#define SFTP_TAB_CONNECTION (B_USER + 1)

#define SFTP_DLG_HEIGHT     26
#define SFTP_DLG_WIDTH      56

/* --------------------------------------------------------------------------------------------- */

static void
sftp_save_page_basic (sftp_connection_t *conn, char *label, char *host, const char *port_str,
                      char *user, char *password, char *path, char *pubkey, char *privkey,
                      gboolean use_agent)
{
    g_free (conn->label);
    conn->label = label;

    g_free (conn->host);
    conn->host = host;

    conn->port = (port_str != NULL && port_str[0] != '\0') ? atoi (port_str) : SFTP_DEFAULT_PORT;
    if (conn->port <= 0)
        conn->port = SFTP_DEFAULT_PORT;

    g_free (conn->user);
    conn->user = user;

    g_free (conn->password);
    conn->password = (password != NULL && password[0] != '\0') ? password : NULL;
    if (conn->password == NULL)
        g_free (password);

    g_free (conn->path);
    conn->path = path;

    g_free (conn->pubkey);
    conn->pubkey = (pubkey != NULL && pubkey[0] != '\0') ? pubkey : NULL;
    if (conn->pubkey == NULL)
        g_free (pubkey);

    g_free (conn->privkey);
    conn->privkey = (privkey != NULL && privkey[0] != '\0') ? privkey : NULL;
    if (conn->privkey == NULL)
        g_free (privkey);

    conn->use_agent = use_agent;
}

/* --------------------------------------------------------------------------------------------- */

static void
sftp_save_page_connection (sftp_connection_t *conn, const char *connect_timeout_str,
                           const char *timeout_str, gboolean keepalive,
                           const char *keepalive_interval_str, int ip_version_idx)
{
    conn->connect_timeout = (connect_timeout_str != NULL) ? atoi (connect_timeout_str) : 0;
    conn->timeout = (timeout_str != NULL) ? atoi (timeout_str) : 0;
    conn->keepalive = keepalive;
    conn->keepalive_interval = (keepalive_interval_str != NULL) ? atoi (keepalive_interval_str) : 0;

    if (ip_version_idx == 1)
        conn->ip_version = 4;
    else if (ip_version_idx == 2)
        conn->ip_version = 6;
    else
        conn->ip_version = 0;
}

/* --------------------------------------------------------------------------------------------- */
/* Connection dialog: Tab 1 — Basic                                                              */
/* --------------------------------------------------------------------------------------------- */

static int
show_connection_tab_basic (sftp_connection_t *conn)
{
    char *label = g_strdup (conn->label != NULL ? conn->label : "");
    char *host = g_strdup (conn->host != NULL ? conn->host : "");
    char *port_str = g_strdup_printf ("%d", conn->port > 0 ? conn->port : SFTP_DEFAULT_PORT);
    char *user = g_strdup (conn->user != NULL ? conn->user : "");
    char *path = g_strdup (conn->path != NULL ? conn->path : "/");
    char *password = g_strdup (conn->password != NULL ? conn->password : "");
    char *pubkey = g_strdup (conn->pubkey != NULL ? conn->pubkey : "");
    char *privkey = g_strdup (conn->privkey != NULL ? conn->privkey : "");
    gboolean use_agent = conn->use_agent;
    int ret;

    /* clang-format off */
    quick_widget_t quick_widgets[] = {
        /* tab buttons */
        QUICK_TOP_BUTTONS (FALSE, TRUE),
            QUICK_BUTTON (N_ ("&Basic"), SFTP_TAB_BASIC, NULL, NULL),
            QUICK_BUTTON (N_ ("&Connection"), SFTP_TAB_CONNECTION, NULL, NULL),
        /* page content */
        QUICK_LABELED_INPUT (N_("Connection name:"), input_label_above,
                            label, "sftp-conn-label",
                            &label, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
        QUICK_LABELED_INPUT (N_("Host:"), input_label_above,
                            host, "sftp-conn-host",
                            &host, NULL, FALSE, FALSE, INPUT_COMPLETE_HOSTNAMES),
        QUICK_LABELED_INPUT (N_("Port:"), input_label_above,
                            port_str, "sftp-conn-port",
                            &port_str, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
        QUICK_LABELED_INPUT (N_("User:"), input_label_above,
                            user, "sftp-conn-user",
                            &user, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
        QUICK_LABELED_INPUT (N_("Password:"), input_label_above,
                            password, "sftp-conn-pass",
                            &password, NULL, TRUE, TRUE, INPUT_COMPLETE_NONE),
        QUICK_LABELED_INPUT (N_("Remote path:"), input_label_above,
                            path, "sftp-conn-path",
                            &path, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
        QUICK_LABELED_INPUT (N_("Public key file:"), input_label_above,
                            pubkey, "sftp-conn-pubkey",
                            &pubkey, NULL, FALSE, FALSE, INPUT_COMPLETE_FILENAMES),
        QUICK_LABELED_INPUT (N_("Private key file:"), input_label_above,
                            privkey, "sftp-conn-privkey",
                            &privkey, NULL, FALSE, FALSE, INPUT_COMPLETE_FILENAMES),
        QUICK_CHECKBOX (N_("Use SSH &agent"), &use_agent, NULL),
        QUICK_BUTTONS_OK_CANCEL,
        QUICK_END,
    };
    /* clang-format on */

    WRect r = { -1, -1, SFTP_DLG_HEIGHT, SFTP_DLG_WIDTH };

    quick_dialog_t qdlg = {
        .rect = r,
        .title = N_ ("SFTP Connection"),
        .help = "[SFTP Plugin]",
        .widgets = quick_widgets,
        .callback = NULL,
        .mouse_callback = NULL,
    };

    ret = quick_dialog (&qdlg);

    if (ret != B_CANCEL)
        sftp_save_page_basic (conn, label, host, port_str, user, password, path, pubkey, privkey,
                              use_agent);
    else
    {
        g_free (label);
        g_free (host);
        g_free (user);
        g_free (path);
        g_free (password);
        g_free (pubkey);
        g_free (privkey);
    }

    g_free (port_str);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */
/* Connection dialog: Tab 2 — Connection                                                         */
/* --------------------------------------------------------------------------------------------- */

static int
show_connection_tab_connection (sftp_connection_t *conn)
{
    char *connect_timeout_str =
        g_strdup_printf ("%d", conn->connect_timeout > 0 ? conn->connect_timeout : 10);
    char *timeout_str = g_strdup_printf ("%d", conn->timeout > 0 ? conn->timeout : 30);
    char *keepalive_interval_str =
        g_strdup_printf ("%d", conn->keepalive_interval > 0 ? conn->keepalive_interval : 60);
    gboolean keepalive = conn->keepalive;
    int ip_version_idx;
    int ret;

    static const char *ip_version_labels[] = { N_ ("A&uto"), N_ ("IPv&4"), N_ ("IPv&6"), NULL };

    if (conn->ip_version == 4)
        ip_version_idx = 1;
    else if (conn->ip_version == 6)
        ip_version_idx = 2;
    else
        ip_version_idx = 0;

    /* clang-format off */
    quick_widget_t quick_widgets[] = {
        /* tab buttons */
        QUICK_TOP_BUTTONS (FALSE, TRUE),
            QUICK_BUTTON (N_ ("&Basic"), SFTP_TAB_BASIC, NULL, NULL),
            QUICK_BUTTON (N_ ("&Connection"), SFTP_TAB_CONNECTION, NULL, NULL),
        /* page content */
        QUICK_START_GROUPBOX (N_("Timeouts")),
            QUICK_START_COLUMNS,
                QUICK_LABELED_INPUT (N_("Connect timeout:"), input_label_above,
                                    connect_timeout_str, "sftp-conn-ctimeout",
                                    &connect_timeout_str, NULL, FALSE, FALSE,
                                    INPUT_COMPLETE_NONE),
            QUICK_NEXT_COLUMN,
                QUICK_LABELED_INPUT (N_("Session timeout:"), input_label_above,
                                    timeout_str, "sftp-conn-timeout",
                                    &timeout_str, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
            QUICK_STOP_COLUMNS,
        QUICK_STOP_GROUPBOX,
        QUICK_START_GROUPBOX (N_("Keepalive")),
            QUICK_START_COLUMNS,
                QUICK_CHECKBOX (N_("SSH &Keepalive"), &keepalive, NULL),
            QUICK_NEXT_COLUMN,
                QUICK_LABELED_INPUT (N_("Interval:"), input_label_above,
                                    keepalive_interval_str, "sftp-conn-keepalive",
                                    &keepalive_interval_str, NULL, FALSE, FALSE,
                                    INPUT_COMPLETE_NONE),
            QUICK_STOP_COLUMNS,
        QUICK_STOP_GROUPBOX,
        QUICK_START_GROUPBOX (N_("IP version")),
            QUICK_RADIO (3, ip_version_labels, &ip_version_idx, NULL),
        QUICK_STOP_GROUPBOX,
        QUICK_BUTTONS_OK_CANCEL,
        QUICK_END,
    };
    /* clang-format on */

    WRect r = { -1, -1, SFTP_DLG_HEIGHT, SFTP_DLG_WIDTH };

    quick_dialog_t qdlg = {
        .rect = r,
        .title = N_ ("SFTP Connection"),
        .help = "[SFTP Plugin]",
        .widgets = quick_widgets,
        .callback = NULL,
        .mouse_callback = NULL,
    };

    ret = quick_dialog_skip (&qdlg, 2);

    if (ret != B_CANCEL)
        sftp_save_page_connection (conn, connect_timeout_str, timeout_str, keepalive,
                                   keepalive_interval_str, ip_version_idx);

    g_free (connect_timeout_str);
    g_free (timeout_str);
    g_free (keepalive_interval_str);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */
/* Connection dialog: tab loop                                                                   */
/* --------------------------------------------------------------------------------------------- */

static gboolean
show_connection_dialog (sftp_connection_t *conn)
{
    sftp_connection_t *backup;
    int current_tab = SFTP_TAB_BASIC;

    backup = sftp_connection_dup (conn);

    while (TRUE)
    {
        int ret;

        switch (current_tab)
        {
        case SFTP_TAB_BASIC:
            ret = show_connection_tab_basic (conn);
            break;
        case SFTP_TAB_CONNECTION:
            ret = show_connection_tab_connection (conn);
            break;
        default:
            ret = show_connection_tab_basic (conn);
            break;
        }

        if (ret == B_ENTER)
        {
            /* OK — accept all changes */
            sftp_connection_free (backup);
            return TRUE;
        }

        if (ret == B_CANCEL)
        {
            /* Cancel — rollback all changes */
            sftp_connection_copy_from (conn, backup);
            sftp_connection_free (backup);
            return FALSE;
        }

        /* Tab switch — values already saved to conn by the page function */
        if (ret >= SFTP_TAB_BASIC && ret <= SFTP_TAB_CONNECTION)
        {
            current_tab = ret;
            continue;
        }

        /* unknown return code — treat as cancel */
        sftp_connection_copy_from (conn, backup);
        sftp_connection_free (backup);
        return FALSE;
    }
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
    data->key_edit =
        sftp_load_hotkey (SFTP_PANEL_KEY_EDIT, SFTP_PANEL_KEY_EDIT_DEFAULT, SFTP_KEY_F (4));

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
    sftp_data_t *data = (sftp_data_t *) plugin_data;
    guint i;

    if (data->at_root)
    {
        for (i = 0; i < data->connections->len; i++)
        {
            const sftp_connection_t *conn =
                (const sftp_connection_t *) g_ptr_array_index (data->connections, i);

            mc_pp_add_entry (list_ptr, conn->label, S_IFDIR | 0755, 0, time (NULL));
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
            mc_pp_add_entry (list_ptr, e->name, mode, size,
                             e->st.st_mtime != 0 ? e->st.st_mtime : time (NULL));
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
            return MC_PPR_CLOSE; /* close plugin */

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
sftp_put_file (void *plugin_data, const char *local_path, const char *dest_name)
{
    sftp_data_t *data = (sftp_data_t *) plugin_data;
    LIBSSH2_SFTP_HANDLE *fileh;
    char *remote_path;
    int local_fd;

    if (data->at_root || data->sftp_session == NULL || data->current_path == NULL)
        return MC_PPR_FAILED;

    local_fd = open (local_path, O_RDONLY);
    if (local_fd < 0)
        return MC_PPR_FAILED;

    remote_path = sftp_join_path (data->current_path, dest_name);
    fileh = libssh2_sftp_open (
        data->sftp_session, remote_path, LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC,
        LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IROTH);
    g_free (remote_path);

    if (fileh == NULL)
    {
        close (local_fd);
        return MC_PPR_FAILED;
    }

    while (TRUE)
    {
        char buf[8192];
        ssize_t n;

        n = read (local_fd, buf, sizeof (buf));
        if (n == 0)
            break;

        if (n < 0 || libssh2_sftp_write (fileh, buf, (size_t) n) != n)
        {
            close (local_fd);
            libssh2_sftp_close (fileh);
            return MC_PPR_FAILED;
        }
    }

    close (local_fd);
    libssh2_sftp_close (fileh);

    sftp_reload_entries (data);

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
            rc = libssh2_sftp_rmdir_ex (data->sftp_session, remote_path,
                                        (unsigned int) strlen (remote_path));
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

    if (!data->at_root)
        return MC_PPR_NOT_SUPPORTED;

    conn = g_new0 (sftp_connection_t, 1);
    conn->port = SFTP_DEFAULT_PORT;
    conn->user = vfs_get_local_username ();
    if (conn->user == NULL)
        conn->user = g_strdup (g_get_user_name ());
    conn->path = g_strdup ("/");
    conn->use_agent = TRUE;

    if (!show_connection_dialog (conn))
    {
        sftp_connection_free (conn);
        return MC_PPR_FAILED;
    }

    if (conn->label == NULL || conn->label[0] == '\0' || conn->host == NULL
        || conn->host[0] == '\0')
    {
        sftp_connection_free (conn);
        return MC_PPR_FAILED;
    }

    g_ptr_array_add (data->connections, conn);
    save_connections (data->connections_file, data->connections);

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
sftp_edit_connection (sftp_data_t *data)
{
    const GString *current_name;
    sftp_connection_t *conn;

    if (!data->at_root)
        return MC_PPR_NOT_SUPPORTED;

    current_name = data->host->get_current (data->host);
    if (current_name == NULL || current_name->len == 0)
        return MC_PPR_FAILED;

    conn = NULL;
    {
        guint i;

        for (i = 0; i < data->connections->len; i++)
        {
            sftp_connection_t *c = (sftp_connection_t *) g_ptr_array_index (data->connections, i);

            if (strcmp (c->label, current_name->str) == 0)
            {
                conn = c;
                break;
            }
        }
    }

    if (conn == NULL)
        return MC_PPR_FAILED;

    if (!show_connection_dialog (conn))
        return MC_PPR_FAILED;

    if (conn->label == NULL || conn->label[0] == '\0' || conn->host == NULL
        || conn->host[0] == '\0')
        return MC_PPR_FAILED;

    save_connections (data->connections_file, data->connections);

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
sftp_view_item (void *plugin_data, const char *fname, const struct stat *st, gboolean plain_view)
{
    sftp_data_t *data = (sftp_data_t *) plugin_data;
    const sftp_connection_t *conn;
    GString *ini;
    GError *error = NULL;
    char *tmp_path = NULL;
    int fd;

    (void) st;
    (void) plain_view;

    if (!data->at_root)
        return MC_PPR_NOT_SUPPORTED;

    if (fname == NULL)
        return MC_PPR_FAILED;

    conn = find_connection (data, fname);
    if (conn == NULL)
        return MC_PPR_FAILED;

    ini = g_string_new ("");
    g_string_append_printf (ini, "[%s]\n", conn->label);
    g_string_append_printf (ini, "host=%s\n", conn->host);
    g_string_append_printf (ini, "port=%d\n", conn->port > 0 ? conn->port : SFTP_DEFAULT_PORT);
    if (conn->user != NULL)
        g_string_append_printf (ini, "user=%s\n", conn->user);
    if (conn->path != NULL)
        g_string_append_printf (ini, "path=%s\n", conn->path);
    if (conn->password != NULL)
        g_string_append (ini, "password=***\n");
    if (conn->pubkey != NULL && conn->pubkey[0] != '\0')
        g_string_append_printf (ini, "pubkey=%s\n", conn->pubkey);
    if (conn->privkey != NULL && conn->privkey[0] != '\0')
        g_string_append_printf (ini, "privkey=%s\n", conn->privkey);
    g_string_append_printf (ini, "use_agent=%s\n", conn->use_agent ? "true" : "false");

    if (conn->timeout > 0)
        g_string_append_printf (ini, "timeout=%d\n", conn->timeout);
    if (conn->connect_timeout > 0)
        g_string_append_printf (ini, "connect_timeout=%d\n", conn->connect_timeout);
    g_string_append_printf (ini, "keepalive=%s\n", conn->keepalive ? "true" : "false");
    if (conn->keepalive_interval > 0)
        g_string_append_printf (ini, "keepalive_interval=%d\n", conn->keepalive_interval);
    if (conn->ip_version != 0)
        g_string_append_printf (ini, "ip_version=%d\n", conn->ip_version);

    fd = g_file_open_tmp ("mc-sftp-view-XXXXXX", &tmp_path, &error);
    if (fd == -1)
    {
        if (error != NULL)
            g_error_free (error);
        g_string_free (ini, TRUE);
        return MC_PPR_FAILED;
    }
    close (fd);

    if (!g_file_set_contents (tmp_path, ini->str, (gssize) ini->len, NULL))
    {
        unlink (tmp_path);
        g_free (tmp_path);
        g_string_free (ini, TRUE);
        return MC_PPR_FAILED;
    }
    g_string_free (ini, TRUE);

    {
        vfs_path_t *tmp_vpath;

        tmp_vpath = vfs_path_from_str (tmp_path);
        (void) mcview_viewer (NULL, tmp_vpath, 0, 0, 0);
        vfs_path_free (tmp_vpath, TRUE);
    }

    unlink (tmp_path);
    g_free (tmp_path);

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
sftp_handle_key (void *plugin_data, int key)
{
    sftp_data_t *data = (sftp_data_t *) plugin_data;

    if (key == CK_Edit || (data->key_edit >= 0 && key == data->key_edit))
        return sftp_edit_connection (data);

    return MC_PPR_NOT_SUPPORTED;
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
