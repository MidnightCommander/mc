/*
   FTP/FTPS network browser panel plugin (libcurl).

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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <curl/curl.h>

#include "lib/global.h"
#include "lib/mcconfig.h"
#include "lib/keybind.h"
#include "lib/panel-plugin.h"
#include "lib/panel-cache.h"
#include "lib/tty/key.h"
#include "lib/util.h"
#include "lib/vfs/vfs.h"
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
    char *password;
    char *path;
    gboolean passive_mode;

    /* Encoding */
    char *encoding; /* file name encoding, NULL = UTF-8 */

    /* Timeouts */
    int timeout;         /* response timeout, sec (0 = 300 default) */
    int connect_timeout; /* connect timeout, sec (0 = 30 default) */

    /* Keepalive */
    gboolean keepalive;     /* TCP keepalive */
    int keepalive_interval; /* keepalive interval, sec (0 = 60 default) */

    /* IP version */
    int ip_version; /* 0=auto, 4=IPv4, 6=IPv6 */

    /* SSL/TLS mode */
    int ssl_mode; /* 0=none, 1=try, 2=control, 3=all */

    /* Post-connect FTP commands */
    char *post_connect_commands; /* \n-separated SITE/QUOTE commands */

    /* Proxy */
    char *proxy_host;
    int proxy_port;
    char *proxy_user;
    char *proxy_password;
    int proxy_type; /* 0=none, 1=HTTP, 2=SOCKS4, 3=SOCKS5 */
} ftp_connection_t;

typedef struct
{
    mc_panel_host_t *host;

    gboolean at_root;
    char *current_path;
    GPtrArray *entries;

    GPtrArray *connections;
    char *connections_file;

    ftp_connection_t *active_connection;

    mc_pp_dir_cache_t dir_cache;

    int key_edit;
    int key_refresh;

    char *title_buf;
} ftp_data_t;

typedef struct
{
    simple_status_msg_t status_msg; /* base class */
    gboolean first;
    GString *log;     /* accumulated multi-line status log */
    Widget *hline_w;  /* separator between log and button */
    Widget *button_w; /* Abort button */
} ftp_connect_status_msg_t;

typedef struct
{
    status_msg_t *sm;
} ftp_connect_progress_t;

/*** forward declarations (file scope functions) *************************************************/

static void *ftp_open (mc_panel_host_t *host, const char *open_path);
static void ftp_close (void *plugin_data);
static mc_pp_result_t ftp_get_items (void *plugin_data, void *list_ptr);
static mc_pp_result_t ftp_chdir (void *plugin_data, const char *path);
static mc_pp_result_t ftp_enter (void *plugin_data, const char *name, const struct stat *st);
static mc_pp_result_t ftp_get_local_copy (void *plugin_data, const char *fname, char **local_path);
static mc_pp_result_t ftp_put_file (void *plugin_data, const char *local_path,
                                    const char *dest_name);
static mc_pp_result_t ftp_delete_items (void *plugin_data, const char **names, int count);
static const char *ftp_get_title (void *plugin_data);
static mc_pp_result_t ftp_create_item (void *plugin_data);
static mc_pp_result_t ftp_view_item (void *plugin_data, const char *fname, const struct stat *st,
                                     gboolean plain_view);
static mc_pp_result_t ftp_handle_key (void *plugin_data, int key);

static void ftp_connect_status_init_cb (status_msg_t *sm);
static int ftp_connect_status_update_cb (status_msg_t *sm);
static void ftp_connect_status_deinit_cb (status_msg_t *sm);
static gboolean ftp_connect_status_set_stage (ftp_connect_status_msg_t *fsm, const char *fmt, ...)
    G_GNUC_PRINTF (2, 3);

#if LIBCURL_VERSION_NUM >= 0x072000
static int ftp_connect_progress_cb (void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                                    curl_off_t ultotal, curl_off_t ulnow);
#else
static int ftp_connect_progress_cb (void *clientp, double dltotal, double dlnow, double ultotal,
                                    double ulnow);
#endif

/*** file scope variables ************************************************************************/

#define FTP_DEFAULT_PORT              21
#define FTP_CACHE_TTL_DEFAULT         60

#define FTP_PANEL_CONFIG_FILE         "panels.ftp.ini"
#define FTP_PANEL_CONFIG_GROUP        "ftp-panel"
#define FTP_PANEL_KEY_EDIT            "hotkey_edit"
#define FTP_PANEL_KEY_REFRESH         "hotkey_refresh"
#define FTP_PANEL_KEY_EDIT_DEFAULT    "f4"
#define FTP_PANEL_KEY_REFRESH_DEFAULT "ctrl-r"

static gboolean
env_is_true (const char *value)
{
    return (value != NULL
            && (*value == '1' || g_ascii_strcasecmp (value, "true") == 0
                || g_ascii_strcasecmp (value, "yes") == 0));
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
ftp_logging_enabled (void)
{
    const char *ftp_enable;
    const char *mc_enable;

    ftp_enable = g_getenv ("MC_FTP_LOG_ENABLE");
    if (ftp_enable != NULL)
        return env_is_true (ftp_enable);

    mc_enable = g_getenv ("MC_LOG_ENABLE");
    return env_is_true (mc_enable);
}

/* --------------------------------------------------------------------------------------------- */

static void G_GNUC_PRINTF (1, 2)
ftp_log (const char *fmt, ...)
{
    const char *log_file;
    FILE *f;
    va_list args;

    if (!ftp_logging_enabled ())
        return;

    log_file = g_getenv ("MC_FTP_LOG_FILE");
    if (log_file == NULL || *log_file == '\0')
        log_file = g_getenv ("MC_LOG_FILE");
    if (log_file == NULL || *log_file == '\0')
        log_file = "/tmp/mc-ftp.log";

    f = fopen (log_file, "a");
    if (f == NULL)
        return;

    (void) fprintf (f, "[ftp-plugin] ");
    va_start (args, fmt);
    (void) vfprintf (f, fmt, args);
    va_end (args);
    (void) fputc ('\n', f);
    (void) fclose (f);
}

/* --------------------------------------------------------------------------------------------- */

#define FTP_LOG(fmt, ...) ftp_log (fmt, ##__VA_ARGS__)

/* --------------------------------------------------------------------------------------------- */
/* Configurable hotkeys (same mechanism as git-panel plugin) */
/* --------------------------------------------------------------------------------------------- */

static int
ftp_parse_hotkey (const char *value, int fallback)
{
    char *s;
    int ret = fallback;

    if (value == NULL)
        return fallback;

    s = g_ascii_strdown (value, -1);
    g_strstrip (s);

    if (strcmp (s, "none") == 0)
        ret = -1;
    else if (strncmp (s, "ctrl-", 5) == 0 && s[5] != '\0' && s[6] == '\0'
             && g_ascii_isalpha ((guchar) s[5]))
        ret = XCTRL (g_ascii_tolower ((guchar) s[5]));
    else if (s[0] == 'f' && g_ascii_isdigit ((guchar) s[1]) != 0)
    {
        int n = atoi (s + 1);

        if (n >= 1 && n <= 24)
            ret = KEY_F (n);
    }
    else if ((strncmp (s, "shift-f", 7) == 0 || strncmp (s, "s-f", 3) == 0)
             && g_ascii_isdigit ((guchar) s[strncmp (s, "shift-f", 7) == 0 ? 7 : 3]) != 0)
    {
        int base = atoi (s + (strncmp (s, "shift-f", 7) == 0 ? 7 : 3));

        if (base >= 1 && base <= 12)
            ret = KEY_F (base + 10);
    }

    g_free (s);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static char *
ftp_read_config_string (const char *path, const char *key)
{
    char *value;
    mc_config_t *cfg;

    if (path == NULL || !exist_file (path))
        return NULL;

    cfg = mc_config_init (path, TRUE);
    value = mc_config_get_string (cfg, FTP_PANEL_CONFIG_GROUP, key, NULL);
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
ftp_save_config_defaults (const char *path)
{
    mc_config_t *cfg;

    if (path == NULL || exist_file (path))
        return;

    cfg = mc_config_init (path, FALSE);
    mc_config_set_string (cfg, FTP_PANEL_CONFIG_GROUP, FTP_PANEL_KEY_EDIT,
                          FTP_PANEL_KEY_EDIT_DEFAULT);
    mc_config_set_string (cfg, FTP_PANEL_CONFIG_GROUP, FTP_PANEL_KEY_REFRESH,
                          FTP_PANEL_KEY_REFRESH_DEFAULT);
    mc_config_save_file (cfg, NULL);
    mc_config_deinit (cfg);
}

/* --------------------------------------------------------------------------------------------- */

static int
ftp_load_hotkey (const char *key, const char *fallback_text, int fallback_key)
{
    char *config_path;
    char *value;
    int hotkey;

    config_path = g_build_filename (mc_config_get_path (), FTP_PANEL_CONFIG_FILE, (char *) NULL);

    value = ftp_read_config_string (config_path, key);
    if (value == NULL && mc_global.sysconfig_dir != NULL)
    {
        char *sys_path;

        sys_path = g_build_filename (mc_global.sysconfig_dir, FTP_PANEL_CONFIG_FILE, (char *) NULL);
        value = ftp_read_config_string (sys_path, key);
        g_free (sys_path);
    }

    ftp_save_config_defaults (config_path);
    g_free (config_path);

    if (value == NULL)
        value = g_strdup (fallback_text);

    hotkey = ftp_parse_hotkey (value, fallback_key);
    g_free (value);
    return hotkey;
}

/* --------------------------------------------------------------------------------------------- */

static guint ftp_curl_refcount = 0;

static const mc_panel_plugin_t ftp_plugin = {
    .api_version = MC_PANEL_PLUGIN_API_VERSION,
    .name = "ftp",
    .display_name = "FTP/FTPS network",
    .proto = "ftp",
    .prefix = NULL,
    .flags = MC_PPF_NAVIGATE | MC_PPF_GET_FILES | MC_PPF_DELETE | MC_PPF_CUSTOM_TITLE
        | MC_PPF_CREATE | MC_PPF_PUT_FILES | MC_PPF_SHOW_IN_MENU,

    .open = ftp_open,
    .close = ftp_close,
    .get_items = ftp_get_items,

    .chdir = ftp_chdir,
    .enter = ftp_enter,
    .view = ftp_view_item,
    .get_local_copy = ftp_get_local_copy,
    .put_file = ftp_put_file,
    .save_file = ftp_put_file,
    .delete_items = ftp_delete_items,
    .get_title = ftp_get_title,
    .handle_key = ftp_handle_key,
    .create_item = ftp_create_item,
};

/*** file scope functions ************************************************************************/

static void
ftp_connection_free (gpointer p)
{
    ftp_connection_t *c = (ftp_connection_t *) p;

    g_free (c->label);
    g_free (c->host);
    g_free (c->user);
    g_free (c->password);
    g_free (c->path);
    g_free (c->encoding);
    g_free (c->post_connect_commands);
    g_free (c->proxy_host);
    g_free (c->proxy_user);
    g_free (c->proxy_password);
    g_free (c);
}

/* --------------------------------------------------------------------------------------------- */

static ftp_connection_t *
ftp_connection_dup (const ftp_connection_t *src)
{
    ftp_connection_t *d;

    d = g_new0 (ftp_connection_t, 1);
    d->label = g_strdup (src->label);
    d->host = g_strdup (src->host);
    d->port = src->port;
    d->user = g_strdup (src->user);
    d->password = g_strdup (src->password);
    d->path = g_strdup (src->path);
    d->passive_mode = src->passive_mode;
    d->encoding = g_strdup (src->encoding);
    d->timeout = src->timeout;
    d->connect_timeout = src->connect_timeout;
    d->keepalive = src->keepalive;
    d->keepalive_interval = src->keepalive_interval;
    d->ip_version = src->ip_version;
    d->ssl_mode = src->ssl_mode;
    d->post_connect_commands = g_strdup (src->post_connect_commands);
    d->proxy_host = g_strdup (src->proxy_host);
    d->proxy_port = src->proxy_port;
    d->proxy_user = g_strdup (src->proxy_user);
    d->proxy_password = g_strdup (src->proxy_password);
    d->proxy_type = src->proxy_type;
    return d;
}

/* --------------------------------------------------------------------------------------------- */

static void
ftp_connection_copy_from (ftp_connection_t *dst, const ftp_connection_t *src)
{
    g_free (dst->label);
    g_free (dst->host);
    g_free (dst->user);
    g_free (dst->password);
    g_free (dst->path);
    g_free (dst->encoding);
    g_free (dst->post_connect_commands);
    g_free (dst->proxy_host);
    g_free (dst->proxy_user);
    g_free (dst->proxy_password);

    dst->label = g_strdup (src->label);
    dst->host = g_strdup (src->host);
    dst->port = src->port;
    dst->user = g_strdup (src->user);
    dst->password = g_strdup (src->password);
    dst->path = g_strdup (src->path);
    dst->passive_mode = src->passive_mode;
    dst->encoding = g_strdup (src->encoding);
    dst->timeout = src->timeout;
    dst->connect_timeout = src->connect_timeout;
    dst->keepalive = src->keepalive;
    dst->keepalive_interval = src->keepalive_interval;
    dst->ip_version = src->ip_version;
    dst->ssl_mode = src->ssl_mode;
    dst->post_connect_commands = g_strdup (src->post_connect_commands);
    dst->proxy_host = g_strdup (src->proxy_host);
    dst->proxy_port = src->proxy_port;
    dst->proxy_user = g_strdup (src->proxy_user);
    dst->proxy_password = g_strdup (src->proxy_password);
    dst->proxy_type = src->proxy_type;
}

/* --------------------------------------------------------------------------------------------- */

static char *
get_connections_file_path (void)
{
    return g_build_filename (g_get_user_config_dir (), "mc", "ftp-connections.ini", (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

static char *
ftp_normalize_host (const char *host_in)
{
    const char *host;
    gsize len;

    if (host_in == NULL)
        return NULL;

    host = host_in;
    while (g_ascii_isspace (*host))
        host++;

    if (g_ascii_strncasecmp (host, "ftp://", 6) == 0)
        host += 6;
    else if (g_ascii_strncasecmp (host, "ftps://", 7) == 0)
        host += 7;

    while (*host == '/')
        host++;

    len = strlen (host);
    while (len > 0 && (host[len - 1] == '/' || g_ascii_isspace (host[len - 1])))
        len--;

    return g_strndup (host, len);
}

/* --------------------------------------------------------------------------------------------- */

/* Simple XOR obfuscation to avoid storing passwords in plain text.
   NOT cryptographically secure — just prevents casual reading. */

static const unsigned char ftp_obfuscation_key[] = "Mc4FtpPanelKey!";

static char *
ftp_password_encode (const char *plain)
{
    size_t i, len, klen;
    guchar *xored;
    gchar *b64;
    char *result;

    if (plain == NULL || plain[0] == '\0')
        return NULL;

    len = strlen (plain);
    klen = sizeof (ftp_obfuscation_key) - 1;
    xored = g_new (guchar, len);

    for (i = 0; i < len; i++)
        xored[i] = (guchar) plain[i] ^ ftp_obfuscation_key[i % klen];

    b64 = g_base64_encode (xored, len);
    g_free (xored);

    result = g_strdup_printf ("enc:%s", b64);
    g_free (b64);

    return result;
}

/* --------------------------------------------------------------------------------------------- */

static char *
ftp_password_decode (const char *encoded)
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

    klen = sizeof (ftp_obfuscation_key) - 1;
    plain = g_new (char, len + 1);

    for (i = 0; i < len; i++)
        plain[i] = (char) (xored[i] ^ ftp_obfuscation_key[i % klen]);
    plain[len] = '\0';

    g_free (xored);
    return plain;
}

/* --------------------------------------------------------------------------------------------- */

static GPtrArray *
load_connections (const char *filepath)
{
    GPtrArray *arr;
    GKeyFile *kf;
    gchar **groups;
    gsize n_groups, i;

    arr = g_ptr_array_new_with_free_func (ftp_connection_free);

    kf = g_key_file_new ();
    if (!g_key_file_load_from_file (kf, filepath, G_KEY_FILE_NONE, NULL))
    {
        g_key_file_free (kf);
        return arr;
    }

    groups = g_key_file_get_groups (kf, &n_groups);
    for (i = 0; i < n_groups; i++)
    {
        ftp_connection_t *conn;
        GError *error = NULL;

        conn = g_new0 (ftp_connection_t, 1);
        conn->label = g_strdup (groups[i]);
        conn->host = g_key_file_get_string (kf, groups[i], "host", NULL);
        conn->user = g_key_file_get_string (kf, groups[i], "user", NULL);
        {
            char *raw_pw = g_key_file_get_string (kf, groups[i], "password", NULL);
            conn->password = ftp_password_decode (raw_pw);
            g_free (raw_pw);
        }
        conn->path = g_key_file_get_string (kf, groups[i], "path", NULL);

        if (conn->host != NULL)
        {
            char *normalized = ftp_normalize_host (conn->host);
            g_free (conn->host);
            conn->host = normalized;
        }

        conn->port = g_key_file_get_integer (kf, groups[i], "port", &error);
        if (error != NULL)
        {
            g_error_free (error);
            conn->port = FTP_DEFAULT_PORT;
        }

        error = NULL;
        conn->passive_mode = g_key_file_get_boolean (kf, groups[i], "passive_mode", &error);
        if (error != NULL)
        {
            g_error_free (error);
            conn->passive_mode = TRUE;
        }

        /* ssl_mode with backward compatibility for use_ftps */
        error = NULL;
        conn->ssl_mode = g_key_file_get_integer (kf, groups[i], "ssl_mode", &error);
        if (error != NULL)
        {
            gboolean use_ftps;
            GError *ftps_error = NULL;

            g_error_free (error);

            use_ftps = g_key_file_get_boolean (kf, groups[i], "use_ftps", &ftps_error);
            if (ftps_error != NULL)
            {
                g_error_free (ftps_error);
                conn->ssl_mode = 0;
            }
            else
            {
                conn->ssl_mode = use_ftps ? 3 : 0;
            }
        }

        /* encoding */
        conn->encoding = g_key_file_get_string (kf, groups[i], "encoding", NULL);

        /* timeouts */
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

        /* keepalive */
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

        /* IP version */
        error = NULL;
        conn->ip_version = g_key_file_get_integer (kf, groups[i], "ip_version", &error);
        if (error != NULL)
        {
            g_error_free (error);
            conn->ip_version = 0;
        }

        /* post-connect commands */
        conn->post_connect_commands =
            g_key_file_get_string (kf, groups[i], "post_connect_commands", NULL);

        /* proxy */
        conn->proxy_host = g_key_file_get_string (kf, groups[i], "proxy_host", NULL);

        error = NULL;
        conn->proxy_port = g_key_file_get_integer (kf, groups[i], "proxy_port", &error);
        if (error != NULL)
        {
            g_error_free (error);
            conn->proxy_port = 0;
        }

        conn->proxy_user = g_key_file_get_string (kf, groups[i], "proxy_user", NULL);
        {
            char *raw_proxy_pw = g_key_file_get_string (kf, groups[i], "proxy_password", NULL);
            conn->proxy_password = ftp_password_decode (raw_proxy_pw);
            g_free (raw_proxy_pw);
        }

        error = NULL;
        conn->proxy_type = g_key_file_get_integer (kf, groups[i], "proxy_type", &error);
        if (error != NULL)
        {
            g_error_free (error);
            conn->proxy_type = 0;
        }

        if (conn->host == NULL || conn->host[0] == '\0')
        {
            ftp_connection_free (conn);
            continue;
        }

        if (conn->port <= 0)
            conn->port = FTP_DEFAULT_PORT;

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
        const ftp_connection_t *conn =
            (const ftp_connection_t *) g_ptr_array_index (connections, i);

        g_key_file_set_string (kf, conn->label, "host", conn->host);
        g_key_file_set_integer (kf, conn->label, "port",
                                conn->port > 0 ? conn->port : FTP_DEFAULT_PORT);

        if (conn->user != NULL)
            g_key_file_set_string (kf, conn->label, "user", conn->user);
        if (conn->path != NULL)
            g_key_file_set_string (kf, conn->label, "path", conn->path);
        if (conn->password != NULL)
        {
            char *enc = ftp_password_encode (conn->password);
            if (enc != NULL)
            {
                g_key_file_set_string (kf, conn->label, "password", enc);
                g_free (enc);
            }
        }

        g_key_file_set_boolean (kf, conn->label, "passive_mode", conn->passive_mode);
        g_key_file_set_integer (kf, conn->label, "ssl_mode", conn->ssl_mode);

        if (conn->encoding != NULL && conn->encoding[0] != '\0')
            g_key_file_set_string (kf, conn->label, "encoding", conn->encoding);

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

        if (conn->post_connect_commands != NULL && conn->post_connect_commands[0] != '\0')
            g_key_file_set_string (kf, conn->label, "post_connect_commands",
                                   conn->post_connect_commands);

        if (conn->proxy_type > 0)
            g_key_file_set_integer (kf, conn->label, "proxy_type", conn->proxy_type);
        if (conn->proxy_host != NULL && conn->proxy_host[0] != '\0')
            g_key_file_set_string (kf, conn->label, "proxy_host", conn->proxy_host);
        if (conn->proxy_port > 0)
            g_key_file_set_integer (kf, conn->label, "proxy_port", conn->proxy_port);
        if (conn->proxy_user != NULL && conn->proxy_user[0] != '\0')
            g_key_file_set_string (kf, conn->label, "proxy_user", conn->proxy_user);
        if (conn->proxy_password != NULL && conn->proxy_password[0] != '\0')
        {
            char *enc_proxy = ftp_password_encode (conn->proxy_password);
            if (enc_proxy != NULL)
            {
                g_key_file_set_string (kf, conn->label, "proxy_password", enc_proxy);
                g_free (enc_proxy);
            }
        }
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

static const ftp_connection_t *
find_connection (const ftp_data_t *data, const char *label)
{
    guint i;

    for (i = 0; i < data->connections->len; i++)
    {
        const ftp_connection_t *c =
            (const ftp_connection_t *) g_ptr_array_index (data->connections, i);

        if (strcmp (c->label, label) == 0)
            return c;
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static const mc_pp_dir_entry_t *
find_entry (const ftp_data_t *data, const char *name)
{
    guint i;

    if (data->entries == NULL)
        return NULL;

    for (i = 0; i < data->entries->len; i++)
    {
        const mc_pp_dir_entry_t *e =
            (const mc_pp_dir_entry_t *) g_ptr_array_index (data->entries, i);

        if (strcmp (e->name, name) == 0)
            return e;
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static char *
ftp_join_path (const char *base, const char *name)
{
    if (strcmp (base, "/") == 0)
        return g_strdup_printf ("/%s", name);

    return g_strdup_printf ("%s/%s", base, name);
}

/* --------------------------------------------------------------------------------------------- */

static char *
ftp_path_up (const char *path)
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

static char *
ftp_build_url (const ftp_connection_t *conn, const char *path)
{
    const char *scheme;

    scheme = (conn->ssl_mode >= 2) ? "ftps" : "ftp";

    if (path != NULL && path[0] != '\0')
        return g_strdup_printf ("%s://%s:%d%s", scheme, conn->host, conn->port, path);

    return g_strdup_printf ("%s://%s:%d/", scheme, conn->host, conn->port);
}

/* --------------------------------------------------------------------------------------------- */

static void
ftp_setup_curl_common (CURL *curl, const ftp_connection_t *conn)
{
    char *userpwd;

    /* Timeouts */
    curl_easy_setopt (curl, CURLOPT_CONNECTTIMEOUT,
                      (long) (conn->connect_timeout > 0 ? conn->connect_timeout : 30));
    curl_easy_setopt (curl, CURLOPT_TIMEOUT, (long) (conn->timeout > 0 ? conn->timeout : 300));
    curl_easy_setopt (curl, CURLOPT_FTP_RESPONSE_TIMEOUT, 60L);

    if (conn->user != NULL && conn->user[0] != '\0')
    {
        if (conn->password != NULL && conn->password[0] != '\0')
            userpwd = g_strdup_printf ("%s:%s", conn->user, conn->password);
        else
            userpwd = g_strdup_printf ("%s:", conn->user);

        curl_easy_setopt (curl, CURLOPT_USERPWD, userpwd);
        g_free (userpwd);
    }

    /* Keepalive */
    if (conn->keepalive)
    {
        curl_easy_setopt (curl, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt (curl, CURLOPT_TCP_KEEPIDLE,
                          (long) (conn->keepalive_interval > 0 ? conn->keepalive_interval : 60));
        curl_easy_setopt (curl, CURLOPT_TCP_KEEPINTVL,
                          (long) (conn->keepalive_interval > 0 ? conn->keepalive_interval : 60));
    }

    /* IP version */
    if (conn->ip_version == 4)
        curl_easy_setopt (curl, CURLOPT_IPRESOLVE, (long) CURL_IPRESOLVE_V4);
    else if (conn->ip_version == 6)
        curl_easy_setopt (curl, CURLOPT_IPRESOLVE, (long) CURL_IPRESOLVE_V6);

    /* SSL/TLS mode */
    switch (conn->ssl_mode)
    {
    case 1:
        curl_easy_setopt (curl, CURLOPT_USE_SSL, (long) CURLUSESSL_TRY);
        break;
    case 2:
        curl_easy_setopt (curl, CURLOPT_USE_SSL, (long) CURLUSESSL_CONTROL);
        break;
    case 3:
        curl_easy_setopt (curl, CURLOPT_USE_SSL, (long) CURLUSESSL_ALL);
        curl_easy_setopt (curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt (curl, CURLOPT_SSL_VERIFYHOST, 0L);
        break;
    default:
        break;
    }

    if (!conn->passive_mode)
        curl_easy_setopt (curl, CURLOPT_FTPPORT, "-");

    /* Proxy */
    if (conn->proxy_type > 0 && conn->proxy_host != NULL && conn->proxy_host[0] != '\0')
    {
        curl_easy_setopt (curl, CURLOPT_PROXY, conn->proxy_host);
        if (conn->proxy_port > 0)
            curl_easy_setopt (curl, CURLOPT_PROXYPORT, (long) conn->proxy_port);
        if (conn->proxy_type == 1)
            curl_easy_setopt (curl, CURLOPT_PROXYTYPE, (long) CURLPROXY_HTTP);
        else if (conn->proxy_type == 2)
            curl_easy_setopt (curl, CURLOPT_PROXYTYPE, (long) CURLPROXY_SOCKS4);
        else if (conn->proxy_type == 3)
            curl_easy_setopt (curl, CURLOPT_PROXYTYPE, (long) CURLPROXY_SOCKS5);

        if (conn->proxy_user != NULL && conn->proxy_user[0] != '\0')
        {
            char *auth;

            if (conn->proxy_password != NULL && conn->proxy_password[0] != '\0')
                auth = g_strdup_printf ("%s:%s", conn->proxy_user, conn->proxy_password);
            else
                auth = g_strdup (conn->proxy_user);
            curl_easy_setopt (curl, CURLOPT_PROXYUSERPWD, auth);
            g_free (auth);
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static struct curl_slist *
ftp_build_post_connect_commands (const ftp_connection_t *conn)
{
    struct curl_slist *cmds = NULL;
    char **lines;
    int i;

    if (conn->post_connect_commands == NULL || conn->post_connect_commands[0] == '\0')
        return NULL;

    lines = g_strsplit (conn->post_connect_commands, "\n", -1);
    for (i = 0; lines[i] != NULL; i++)
    {
        if (lines[i][0] != '\0')
            cmds = curl_slist_append (cmds, lines[i]);
    }
    g_strfreev (lines);

    return cmds;
}

/* --------------------------------------------------------------------------------------------- */
/* FTP LIST parser (unix format) */
/* --------------------------------------------------------------------------------------------- */

static gboolean
parse_ftp_month (const char *mon, int *month_out)
{
    static const char *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    int i;

    for (i = 0; i < 12; i++)
    {
        if (g_ascii_strncasecmp (mon, months[i], 3) == 0)
        {
            *month_out = i;
            return TRUE;
        }
    }

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
parse_ftp_list_line (const char *line, mc_pp_dir_entry_t *entry)
{
    const char *p;
    const char *name_start;
    char perms[11];
    int nlink;
    char owner[64], group[64];
    long long size_val;
    char month_str[4];
    int day, year_or_hour, minute;
    int month;
    struct tm tm_val;
    gboolean has_year;

    if (line == NULL || line[0] == '\0')
        return FALSE;

    /* skip "total NNN" lines */
    if (strncmp (line, "total ", 6) == 0)
        return FALSE;

    p = line;

    /* parse permissions: drwxr-xr-x */
    if (strlen (p) < 10)
        return FALSE;

    memcpy (perms, p, 10);
    perms[10] = '\0';
    p += 10;

    /* skip whitespace */
    while (*p == ' ')
        p++;

    /* nlink */
    if (sscanf (p, "%d", &nlink) != 1)
        nlink = 1;
    while (*p != '\0' && *p != ' ')
        p++;
    while (*p == ' ')
        p++;

    /* owner */
    {
        int k = 0;

        while (*p != '\0' && *p != ' ' && k < (int) sizeof (owner) - 1)
            owner[k++] = *p++;
        owner[k] = '\0';
    }
    while (*p == ' ')
        p++;

    /* group */
    {
        int k = 0;

        while (*p != '\0' && *p != ' ' && k < (int) sizeof (group) - 1)
            group[k++] = *p++;
        group[k] = '\0';
    }
    while (*p == ' ')
        p++;

    /* size */
    if (sscanf (p, "%lld", &size_val) != 1)
        size_val = 0;
    while (*p != '\0' && *p != ' ')
        p++;
    while (*p == ' ')
        p++;

    /* month */
    if (sscanf (p, "%3s", month_str) != 1)
        return FALSE;
    if (!parse_ftp_month (month_str, &month))
        return FALSE;
    while (*p != '\0' && *p != ' ')
        p++;
    while (*p == ' ')
        p++;

    /* day */
    if (sscanf (p, "%d", &day) != 1)
        return FALSE;
    while (*p != '\0' && *p != ' ')
        p++;
    while (*p == ' ')
        p++;

    /* year or time (HH:MM) */
    has_year = TRUE;
    year_or_hour = 0;
    minute = 0;

    {
        const char *colon = strchr (p, ':');

        if (colon != NULL && colon < p + 3)
        {
            /* it's a time */
            has_year = FALSE;
            if (sscanf (p, "%d:%d", &year_or_hour, &minute) != 2)
                return FALSE;
        }
        else
        {
            if (sscanf (p, "%d", &year_or_hour) != 1)
                return FALSE;
        }
    }
    while (*p != '\0' && *p != ' ')
        p++;
    while (*p == ' ')
        p++;

    /* file name (rest of line) */
    name_start = p;
    if (*name_start == '\0')
        return FALSE;

    /* skip symlink targets */
    {
        const char *arrow;
        size_t name_len;

        arrow = strstr (name_start, " -> ");
        if (arrow != NULL)
            name_len = (size_t) (arrow - name_start);
        else
        {
            name_len = strlen (name_start);
            /* trim trailing whitespace/newline */
            while (name_len > 0
                   && (name_start[name_len - 1] == '\n' || name_start[name_len - 1] == '\r'
                       || name_start[name_len - 1] == ' '))
                name_len--;
        }

        if (name_len == 0)
            return FALSE;

        entry->name = g_strndup (name_start, name_len);
    }

    /* skip . and .. */
    if (strcmp (entry->name, ".") == 0 || strcmp (entry->name, "..") == 0)
    {
        g_free (entry->name);
        entry->name = NULL;
        return FALSE;
    }

    /* build struct stat */
    memset (&entry->st, 0, sizeof (entry->st));
    entry->st.st_nlink = nlink;
    entry->st.st_size = (off_t) size_val;
    entry->st.st_uid = getuid ();
    entry->st.st_gid = getgid ();

    /* parse mode from permission string */
    {
        mode_t mode = 0;

        switch (perms[0])
        {
        case 'd':
            mode |= S_IFDIR;
            break;
        case 'l':
            mode |= S_IFLNK;
            break;
        case 'c':
            mode |= S_IFCHR;
            break;
        case 'b':
            mode |= S_IFBLK;
            break;
        case 'p':
            mode |= S_IFIFO;
            break;
        case 's':
            mode |= S_IFSOCK;
            break;
        default:
            mode |= S_IFREG;
            break;
        }

        if (perms[1] == 'r')
            mode |= S_IRUSR;
        if (perms[2] == 'w')
            mode |= S_IWUSR;
        if (perms[3] == 'x' || perms[3] == 's')
            mode |= S_IXUSR;
        if (perms[3] == 's' || perms[3] == 'S')
            mode |= S_ISUID;
        if (perms[4] == 'r')
            mode |= S_IRGRP;
        if (perms[5] == 'w')
            mode |= S_IWGRP;
        if (perms[6] == 'x' || perms[6] == 's')
            mode |= S_IXGRP;
        if (perms[6] == 's' || perms[6] == 'S')
            mode |= S_ISGID;
        if (perms[7] == 'r')
            mode |= S_IROTH;
        if (perms[8] == 'w')
            mode |= S_IWOTH;
        if (perms[9] == 'x' || perms[9] == 't')
            mode |= S_IXOTH;
        if (perms[9] == 't' || perms[9] == 'T')
            mode |= S_ISVTX;

        entry->st.st_mode = mode;
    }

    /* build time */
    memset (&tm_val, 0, sizeof (tm_val));
    tm_val.tm_mon = month;
    tm_val.tm_mday = day;
    tm_val.tm_isdst = -1;

    if (has_year)
    {
        tm_val.tm_year = year_or_hour - 1900;
    }
    else
    {
        time_t now;
        struct tm *now_tm;

        now = time (NULL);
        now_tm = localtime (&now);

        tm_val.tm_year = now_tm->tm_year;
        tm_val.tm_hour = year_or_hour;
        tm_val.tm_min = minute;

        /* if the date would be in the future, assume previous year */
        {
            time_t t = mktime (&tm_val);

            if (t > now + 60 * 60 * 24)
            {
                tm_val.tm_year--;
                t = mktime (&tm_val);
            }
            entry->st.st_mtime = t;
        }
    }

    if (has_year)
        entry->st.st_mtime = mktime (&tm_val);

    entry->is_dir = S_ISDIR (entry->st.st_mode);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* Windows NT / IIS FTP listing parser */
/* --------------------------------------------------------------------------------------------- */

static gboolean
parse_ftp_list_line_nt (const char *line, mc_pp_dir_entry_t *entry)
{
    int month, day, year, hour, minute;
    char ampm[3];
    const char *p;
    long long size_val;

    if (line == NULL || line[0] == '\0')
        return FALSE;

    /* Format: MM-DD-YY  HH:MMAM/PM  <DIR>|size  name */
    /* Example: 07-13-98  09:06PM       <DIR>          aix */
    /* Example: 03-18-98  06:01AM              2109440 nlxb318e.tar */

    if (sscanf (line, "%d-%d-%d %d:%d%2s", &month, &day, &year, &hour, &minute, ampm) != 6)
        return FALSE;

    /* fix 2-digit year */
    if (year < 70)
        year += 2000;
    else if (year < 100)
        year += 1900;

    /* fix AM/PM */
    if (g_ascii_strcasecmp (ampm, "PM") == 0 && hour != 12)
        hour += 12;
    else if (g_ascii_strcasecmp (ampm, "AM") == 0 && hour == 12)
        hour = 0;

    /* skip past date/time to whitespace before <DIR>/size */
    p = line;
    /* skip date */
    while (*p != '\0' && !isspace ((unsigned char) *p))
        p++;
    while (*p == ' ')
        p++;
    /* skip time */
    while (*p != '\0' && !isspace ((unsigned char) *p))
        p++;
    while (*p == ' ')
        p++;

    memset (&entry->st, 0, sizeof (entry->st));
    entry->st.st_nlink = 1;
    entry->st.st_uid = getuid ();
    entry->st.st_gid = getgid ();

    if (strncmp (p, "<DIR>", 5) == 0)
    {
        entry->st.st_mode = S_IFDIR | 0755;
        entry->is_dir = TRUE;
        p += 5;
    }
    else
    {
        if (sscanf (p, "%lld", &size_val) != 1)
            return FALSE;
        entry->st.st_mode = S_IFREG | 0644;
        entry->st.st_size = (off_t) size_val;
        entry->is_dir = FALSE;
        while (*p != '\0' && !isspace ((unsigned char) *p))
            p++;
    }

    /* skip to filename */
    while (*p == ' ')
        p++;

    if (*p == '\0')
        return FALSE;

    {
        size_t name_len = strlen (p);

        while (name_len > 0 && (p[name_len - 1] == '\n' || p[name_len - 1] == '\r'))
            name_len--;

        if (name_len == 0)
            return FALSE;

        entry->name = g_strndup (p, name_len);
    }

    if (strcmp (entry->name, ".") == 0 || strcmp (entry->name, "..") == 0)
    {
        g_free (entry->name);
        entry->name = NULL;
        return FALSE;
    }

    /* build time */
    {
        struct tm tm_val;

        memset (&tm_val, 0, sizeof (tm_val));
        tm_val.tm_year = year - 1900;
        tm_val.tm_mon = month - 1;
        tm_val.tm_mday = day;
        tm_val.tm_hour = hour;
        tm_val.tm_min = minute;
        tm_val.tm_isdst = -1;
        entry->st.st_mtime = mktime (&tm_val);
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* curl write callback for listing */
/* --------------------------------------------------------------------------------------------- */

typedef struct
{
    GString *buf;
} ftp_list_ctx_t;

static size_t
ftp_list_write_cb (void *ptr, size_t size, size_t nmemb, void *userdata)
{
    ftp_list_ctx_t *ctx = (ftp_list_ctx_t *) userdata;
    size_t total = size * nmemb;

    g_string_append_len (ctx->buf, (const char *) ptr, (gssize) total);
    return total;
}

/* --------------------------------------------------------------------------------------------- */

static GPtrArray *
ftp_load_entries (ftp_data_t *data, status_msg_t *sm)
{
    GPtrArray *arr;
    CURL *curl;
    CURLcode res;
    ftp_list_ctx_t ctx;
    char *url;
    char *dir_url;
    char **lines;
    int i;

    arr = g_ptr_array_new_with_free_func (mc_pp_dir_entry_free);

    if (data->active_connection == NULL)
    {
        FTP_LOG ("load_entries: no active connection");
        return arr;
    }

    /* ensure trailing slash for directory listing */
    url = ftp_build_url (data->active_connection, data->current_path);
    if (url[strlen (url) - 1] != '/')
    {
        dir_url = g_strdup_printf ("%s/", url);
        g_free (url);
    }
    else
    {
        dir_url = url;
    }

    FTP_LOG ("load_entries: listing URL = %s", dir_url);

    curl = curl_easy_init ();
    if (curl == NULL)
    {
        FTP_LOG ("load_entries: curl_easy_init failed");
        g_free (dir_url);
        return arr;
    }

    ctx.buf = g_string_new ("");

    curl_easy_setopt (curl, CURLOPT_URL, dir_url);
    ftp_setup_curl_common (curl, data->active_connection);
    curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, ftp_list_write_cb);
    curl_easy_setopt (curl, CURLOPT_WRITEDATA, &ctx);
    /* use full LIST, not NLST */
    curl_easy_setopt (curl, CURLOPT_DIRLISTONLY, 0L);

    if (sm != NULL)
    {
        ftp_connect_progress_t progress;

        progress.sm = sm;
        curl_easy_setopt (curl, CURLOPT_NOPROGRESS, 0L);
#if LIBCURL_VERSION_NUM >= 0x072000
        curl_easy_setopt (curl, CURLOPT_XFERINFOFUNCTION, ftp_connect_progress_cb);
        curl_easy_setopt (curl, CURLOPT_XFERINFODATA, &progress);
#else
        curl_easy_setopt (curl, CURLOPT_PROGRESSFUNCTION, ftp_connect_progress_cb);
        curl_easy_setopt (curl, CURLOPT_PROGRESSDATA, &progress);
#endif
    }

    FTP_LOG ("load_entries: performing curl request...");
    res = curl_easy_perform (curl);
    FTP_LOG ("load_entries: curl result = %d (%s)", (int) res, curl_easy_strerror (res));
    curl_easy_cleanup (curl);
    g_free (dir_url);

    if (res != CURLE_OK)
    {
        FTP_LOG ("load_entries: request failed, returning empty list");
        g_string_free (ctx.buf, TRUE);
        return arr;
    }

    FTP_LOG ("load_entries: received %lu bytes of listing data", (unsigned long) ctx.buf->len);

    lines = g_strsplit (ctx.buf->str, "\n", -1);
    g_string_free (ctx.buf, TRUE);

    {
        int parsed = 0, skipped = 0;

        for (i = 0; lines[i] != NULL; i++)
        {
            mc_pp_dir_entry_t *entry;
            gboolean ok;

            if (lines[i][0] == '\0')
                continue;

            entry = g_new0 (mc_pp_dir_entry_t, 1);

            /* try unix format first, then NT format */
            ok = parse_ftp_list_line (lines[i], entry);
            if (!ok)
                ok = parse_ftp_list_line_nt (lines[i], entry);

            if (ok && entry->name != NULL)
            {
                mode_t mode = entry->st.st_mode;

                if (!S_ISDIR (mode) && !S_ISREG (mode) && !S_ISLNK (mode))
                {
                    mode = S_IFREG | 0644;
                    entry->st.st_mode = mode;
                }

                /* convert filename encoding if needed */
                if (data->active_connection != NULL && data->active_connection->encoding != NULL
                    && data->active_connection->encoding[0] != '\0'
                    && g_ascii_strcasecmp (data->active_connection->encoding, "UTF-8") != 0)
                {
                    gsize bytes_read, bytes_written;
                    char *utf8_name;

                    utf8_name =
                        g_convert (entry->name, -1, "UTF-8", data->active_connection->encoding,
                                   &bytes_read, &bytes_written, NULL);
                    if (utf8_name != NULL)
                    {
                        g_free (entry->name);
                        entry->name = utf8_name;
                    }
                }

                g_ptr_array_add (arr, entry);
                parsed++;
            }
            else
            {
                FTP_LOG ("load_entries: skipped line: '%.80s'", lines[i]);
                g_free (entry->name);
                g_free (entry);
                skipped++;
            }
        }

        FTP_LOG ("load_entries: parsed %d entries, skipped %d lines", parsed, skipped);
    }

    g_strfreev (lines);
    return arr;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
ftp_reload_entries (ftp_data_t *data, gboolean show_progress)
{
    GPtrArray *cached;

    if (data->entries != NULL)
    {
        g_ptr_array_free (data->entries, TRUE);
        data->entries = NULL;
    }

    if (data->at_root)
        return TRUE;

    if (data->active_connection == NULL)
        return FALSE;

    FTP_LOG ("reload_entries: path='%s' show_progress=%d", data->current_path, show_progress);

    cached = mc_pp_dir_cache_lookup (&data->dir_cache, data->current_path);
    if (cached != NULL)
    {
        FTP_LOG ("reload_entries: cache hit for '%s' (%u entries)", data->current_path,
                 cached->len);
        data->entries = cached;
        return TRUE;
    }
    FTP_LOG ("reload_entries: cache miss for '%s'", data->current_path);

    if (show_progress)
    {
        ftp_connect_status_msg_t status;

        memset (&status, 0, sizeof (status));
        status.first = TRUE;
        status.log = g_string_new (NULL);

        status_msg_init (STATUS_MSG (&status), _ ("FTP"), 0.0, ftp_connect_status_init_cb,
                         ftp_connect_status_update_cb, ftp_connect_status_deinit_cb);

        (void) ftp_connect_status_set_stage (&status, _ ("Reading remote directory..."));

        data->entries = ftp_load_entries (data, STATUS_MSG (&status));

        status_msg_deinit (STATUS_MSG (&status));
        g_string_free (status.log, TRUE);
    }
    else
    {
        data->entries = ftp_load_entries (data, NULL);
    }

    if (data->entries != NULL)
    {
        FTP_LOG ("reload_entries: storing %u entries in cache for '%s'", data->entries->len,
                 data->current_path);
        mc_pp_dir_cache_store (&data->dir_cache, data->current_path, data->entries);
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/* Connection dialog: page save helpers                                                          */
/* --------------------------------------------------------------------------------------------- */

#define FTP_TAB_BASIC    (B_USER + 0)
#define FTP_TAB_SECURITY (B_USER + 1)
#define FTP_TAB_ADVANCED (B_USER + 2)
#define FTP_DLG_HEIGHT   24
#define FTP_DLG_WIDTH    56

static void
ftp_save_page_basic (ftp_connection_t *conn, char *label, char *host, const char *port_str,
                     char *user, char *password, char *path)
{
    g_free (conn->label);
    conn->label = label;

    g_free (conn->host);
    conn->host = ftp_normalize_host (host);
    g_free (host);

    conn->port = (port_str != NULL && port_str[0] != '\0') ? atoi (port_str) : FTP_DEFAULT_PORT;
    if (conn->port <= 0)
        conn->port = FTP_DEFAULT_PORT;

    g_free (conn->user);
    conn->user = user;

    g_free (conn->password);
    conn->password = (password != NULL && password[0] != '\0') ? password : NULL;
    if (conn->password == NULL)
        g_free (password);

    g_free (conn->path);
    conn->path = path;
}

/* --------------------------------------------------------------------------------------------- */

static void
ftp_save_page_security (ftp_connection_t *conn, int ssl_mode, gboolean passive_mode,
                        const char *connect_timeout_str, const char *timeout_str,
                        gboolean keepalive, const char *keepalive_interval_str, int ip_version_idx)
{
    conn->ssl_mode = ssl_mode;
    conn->passive_mode = passive_mode;
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

static void
ftp_save_page_advanced (ftp_connection_t *conn, char *encoding, char *post_cmds, int proxy_type,
                        char *proxy_host, const char *proxy_port_str, char *proxy_user,
                        char *proxy_password)
{
    g_free (conn->encoding);
    conn->encoding = (encoding != NULL && encoding[0] != '\0') ? encoding : NULL;
    if (conn->encoding == NULL)
        g_free (encoding);

    g_free (conn->post_connect_commands);
    conn->post_connect_commands = (post_cmds != NULL && post_cmds[0] != '\0') ? post_cmds : NULL;
    if (conn->post_connect_commands == NULL)
        g_free (post_cmds);

    conn->proxy_type = proxy_type;

    g_free (conn->proxy_host);
    conn->proxy_host = (proxy_host != NULL && proxy_host[0] != '\0') ? proxy_host : NULL;
    if (conn->proxy_host == NULL)
        g_free (proxy_host);

    conn->proxy_port = (proxy_port_str != NULL) ? atoi (proxy_port_str) : 0;

    g_free (conn->proxy_user);
    conn->proxy_user = (proxy_user != NULL && proxy_user[0] != '\0') ? proxy_user : NULL;
    if (conn->proxy_user == NULL)
        g_free (proxy_user);

    g_free (conn->proxy_password);
    conn->proxy_password =
        (proxy_password != NULL && proxy_password[0] != '\0') ? proxy_password : NULL;
    if (conn->proxy_password == NULL)
        g_free (proxy_password);
}

/* --------------------------------------------------------------------------------------------- */
/* Connection dialog: Tab 1 — Basic                                                              */
/* --------------------------------------------------------------------------------------------- */

static int
show_connection_tab_basic (ftp_connection_t *conn)
{
    char *label = g_strdup (conn->label != NULL ? conn->label : "");
    char *host = g_strdup (conn->host != NULL ? conn->host : "");
    char *port_str = g_strdup_printf ("%d", conn->port > 0 ? conn->port : FTP_DEFAULT_PORT);
    char *user = g_strdup (conn->user != NULL ? conn->user : "");
    char *path = g_strdup (conn->path != NULL ? conn->path : "/");
    char *password = g_strdup (conn->password != NULL ? conn->password : "");
    int ret;

    /* clang-format off */
    quick_widget_t quick_widgets[] = {
        /* tab buttons */
        QUICK_TOP_BUTTONS (FALSE, TRUE),
            QUICK_BUTTON (N_ ("&Basic"), FTP_TAB_BASIC, NULL, NULL),
            QUICK_BUTTON (N_ ("&Security"), FTP_TAB_SECURITY, NULL, NULL),
            QUICK_BUTTON (N_ ("&Advanced"), FTP_TAB_ADVANCED, NULL, NULL),
        /* page content */
        QUICK_LABELED_INPUT (N_("Connection name:"), input_label_above,
                            label, "ftp-conn-label",
                            &label, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
        QUICK_LABELED_INPUT (N_("Host:"), input_label_above,
                            host, "ftp-conn-host",
                            &host, NULL, FALSE, FALSE, INPUT_COMPLETE_HOSTNAMES),
        QUICK_LABELED_INPUT (N_("Port:"), input_label_above,
                            port_str, "ftp-conn-port",
                            &port_str, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
        QUICK_LABELED_INPUT (N_("User:"), input_label_above,
                            user, "ftp-conn-user",
                            &user, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
        QUICK_LABELED_INPUT (N_("Password:"), input_label_above,
                            password, "ftp-conn-pass",
                            &password, NULL, TRUE, TRUE, INPUT_COMPLETE_NONE),
        QUICK_LABELED_INPUT (N_("Remote path:"), input_label_above,
                            path, "ftp-conn-path",
                            &path, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
        QUICK_BUTTONS_OK_CANCEL,
        QUICK_END,
    };
    /* clang-format on */

    WRect r = { -1, -1, FTP_DLG_HEIGHT, FTP_DLG_WIDTH };

    quick_dialog_t qdlg = {
        .rect = r,
        .title = N_ ("FTP Connection"),
        .help = "[FTP Plugin]",
        .widgets = quick_widgets,
        .callback = NULL,
        .mouse_callback = NULL,
    };

    ret = quick_dialog (&qdlg);

    if (ret != B_CANCEL)
        ftp_save_page_basic (conn, label, host, port_str, user, password, path);
    else
    {
        g_free (label);
        g_free (host);
        g_free (user);
        g_free (path);
        g_free (password);
    }

    g_free (port_str);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */
/* Connection dialog: Tab 2 — Security & Connection                                              */
/* --------------------------------------------------------------------------------------------- */

static int
show_connection_tab_security (ftp_connection_t *conn)
{
    char *connect_timeout_str =
        g_strdup_printf ("%d", conn->connect_timeout > 0 ? conn->connect_timeout : 30);
    char *timeout_str = g_strdup_printf ("%d", conn->timeout > 0 ? conn->timeout : 300);
    char *keepalive_interval_str =
        g_strdup_printf ("%d", conn->keepalive_interval > 0 ? conn->keepalive_interval : 60);
    gboolean passive_mode = conn->passive_mode;
    gboolean keepalive = conn->keepalive;
    int ssl_mode = conn->ssl_mode;
    int ip_version_idx;
    int ret;

    static const char *ssl_mode_labels[] = { N_ ("&None (FTP)"), N_ ("&Try SSL"),
                                             N_ ("SSL &commands only"), N_ ("SSL &all (FTPS)"),
                                             NULL };
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
            QUICK_BUTTON (N_ ("&Basic"), FTP_TAB_BASIC, NULL, NULL),
            QUICK_BUTTON (N_ ("&Security"), FTP_TAB_SECURITY, NULL, NULL),
            QUICK_BUTTON (N_ ("&Advanced"), FTP_TAB_ADVANCED, NULL, NULL),
        /* page content */
        QUICK_START_GROUPBOX (N_("SSL/TLS")),
            QUICK_RADIO (4, ssl_mode_labels, &ssl_mode, NULL),
            QUICK_CHECKBOX (N_("&Passive mode"), &passive_mode, NULL),
        QUICK_STOP_GROUPBOX,
        QUICK_START_GROUPBOX (N_("Connection")),
            QUICK_START_COLUMNS,
                QUICK_LABELED_INPUT (N_("Connect timeout:"), input_label_above,
                                    connect_timeout_str, "ftp-conn-ctimeout",
                                    &connect_timeout_str, NULL, FALSE, FALSE,
                                    INPUT_COMPLETE_NONE),
            QUICK_NEXT_COLUMN,
                QUICK_LABELED_INPUT (N_("Response timeout:"), input_label_above,
                                    timeout_str, "ftp-conn-timeout",
                                    &timeout_str, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
            QUICK_STOP_COLUMNS,
            QUICK_START_COLUMNS,
                QUICK_CHECKBOX (N_("TCP &Keepalive"), &keepalive, NULL),
            QUICK_NEXT_COLUMN,
                QUICK_LABELED_INPUT (N_("Interval:"), input_label_above,
                                    keepalive_interval_str, "ftp-conn-keepalive",
                                    &keepalive_interval_str, NULL, FALSE, FALSE,
                                    INPUT_COMPLETE_NONE),
            QUICK_STOP_COLUMNS,
            QUICK_RADIO (3, ip_version_labels, &ip_version_idx, NULL),
        QUICK_STOP_GROUPBOX,
        QUICK_BUTTONS_OK_CANCEL,
        QUICK_END,
    };
    /* clang-format on */

    WRect r = { -1, -1, FTP_DLG_HEIGHT, FTP_DLG_WIDTH };

    quick_dialog_t qdlg = {
        .rect = r,
        .title = N_ ("FTP Connection"),
        .help = "[FTP Plugin]",
        .widgets = quick_widgets,
        .callback = NULL,
        .mouse_callback = NULL,
    };

    ret = quick_dialog_skip (&qdlg, 2);

    if (ret != B_CANCEL)
        ftp_save_page_security (conn, ssl_mode, passive_mode, connect_timeout_str, timeout_str,
                                keepalive, keepalive_interval_str, ip_version_idx);

    g_free (connect_timeout_str);
    g_free (timeout_str);
    g_free (keepalive_interval_str);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */
/* Connection dialog: Tab 3 — Advanced & Proxy                                                   */
/* --------------------------------------------------------------------------------------------- */

static int
show_connection_tab_advanced (ftp_connection_t *conn)
{
    char *encoding = g_strdup (conn->encoding != NULL ? conn->encoding : "");
    char *post_cmds =
        g_strdup (conn->post_connect_commands != NULL ? conn->post_connect_commands : "");
    char *proxy_host = g_strdup (conn->proxy_host != NULL ? conn->proxy_host : "");
    char *proxy_port_str = g_strdup_printf ("%d", conn->proxy_port > 0 ? conn->proxy_port : 0);
    char *proxy_user = g_strdup (conn->proxy_user != NULL ? conn->proxy_user : "");
    char *proxy_password = g_strdup (conn->proxy_password != NULL ? conn->proxy_password : "");
    int proxy_type = conn->proxy_type;
    int ret;

    static const char *proxy_type_labels[] = { N_ ("Non&e"), N_ ("&HTTP"), N_ ("SOCKS&4"),
                                               N_ ("SOCKS&5"), NULL };

    /* clang-format off */
    quick_widget_t quick_widgets[] = {
        /* tab buttons */
        QUICK_TOP_BUTTONS (FALSE, TRUE),
            QUICK_BUTTON (N_ ("&Basic"), FTP_TAB_BASIC, NULL, NULL),
            QUICK_BUTTON (N_ ("&Security"), FTP_TAB_SECURITY, NULL, NULL),
            QUICK_BUTTON (N_ ("&Advanced"), FTP_TAB_ADVANCED, NULL, NULL),
        /* page content */
        QUICK_START_GROUPBOX (N_("Advanced")),
            QUICK_LABELED_INPUT (N_("Encoding:"), input_label_above,
                                encoding, "ftp-conn-encoding",
                                &encoding, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
            QUICK_LABELED_INPUT (N_("Commands after login:"), input_label_above,
                                post_cmds, "ftp-conn-postcmds",
                                &post_cmds, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
        QUICK_STOP_GROUPBOX,
        QUICK_START_GROUPBOX (N_("Proxy")),
            QUICK_RADIO (4, proxy_type_labels, &proxy_type, NULL),
            QUICK_START_COLUMNS,
                QUICK_LABELED_INPUT (N_("Host:"), input_label_above,
                                    proxy_host, "ftp-conn-phost",
                                    &proxy_host, NULL, FALSE, FALSE, INPUT_COMPLETE_HOSTNAMES),
            QUICK_NEXT_COLUMN,
                QUICK_LABELED_INPUT (N_("Port:"), input_label_above,
                                    proxy_port_str, "ftp-conn-pport",
                                    &proxy_port_str, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
            QUICK_STOP_COLUMNS,
            QUICK_START_COLUMNS,
                QUICK_LABELED_INPUT (N_("User:"), input_label_above,
                                    proxy_user, "ftp-conn-puser",
                                    &proxy_user, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
            QUICK_NEXT_COLUMN,
                QUICK_LABELED_INPUT (N_("Pass:"), input_label_above,
                                    proxy_password, "ftp-conn-ppass",
                                    &proxy_password, NULL, TRUE, TRUE, INPUT_COMPLETE_NONE),
            QUICK_STOP_COLUMNS,
        QUICK_STOP_GROUPBOX,
        QUICK_BUTTONS_OK_CANCEL,
        QUICK_END,
    };
    /* clang-format on */

    WRect r = { -1, -1, FTP_DLG_HEIGHT, FTP_DLG_WIDTH };

    quick_dialog_t qdlg = {
        .rect = r,
        .title = N_ ("FTP Connection"),
        .help = "[FTP Plugin]",
        .widgets = quick_widgets,
        .callback = NULL,
        .mouse_callback = NULL,
    };

    ret = quick_dialog_skip (&qdlg, 3);

    if (ret != B_CANCEL)
        ftp_save_page_advanced (conn, encoding, post_cmds, proxy_type, proxy_host, proxy_port_str,
                                proxy_user, proxy_password);
    else
    {
        g_free (encoding);
        g_free (post_cmds);
        g_free (proxy_host);
        g_free (proxy_user);
        g_free (proxy_password);
    }

    g_free (proxy_port_str);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */
/* Connection dialog: tab loop                                                                   */
/* --------------------------------------------------------------------------------------------- */

static gboolean
show_connection_dialog (ftp_connection_t *conn)
{
    ftp_connection_t *backup;
    int current_tab = FTP_TAB_BASIC;

    backup = ftp_connection_dup (conn);

    while (TRUE)
    {
        int ret;

        switch (current_tab)
        {
        case FTP_TAB_BASIC:
            ret = show_connection_tab_basic (conn);
            break;
        case FTP_TAB_SECURITY:
            ret = show_connection_tab_security (conn);
            break;
        case FTP_TAB_ADVANCED:
            ret = show_connection_tab_advanced (conn);
            break;
        default:
            ret = show_connection_tab_basic (conn);
            break;
        }

        if (ret == B_ENTER)
        {
            /* OK — accept all changes */
            ftp_connection_free (backup);
            return TRUE;
        }

        if (ret == B_CANCEL)
        {
            /* Cancel — rollback all changes */
            ftp_connection_copy_from (conn, backup);
            ftp_connection_free (backup);
            return FALSE;
        }

        /* Tab switch — values already saved to conn by the page function */
        if (ret >= FTP_TAB_BASIC && ret <= FTP_TAB_ADVANCED)
        {
            current_tab = ret;
            continue;
        }

        /* unknown return code — treat as cancel */
        ftp_connection_copy_from (conn, backup);
        ftp_connection_free (backup);
        return FALSE;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
ftp_connect_status_init_cb (status_msg_t *sm)
{
    simple_status_msg_t *ssm = SIMPLE_STATUS_MSG (sm);
    ftp_connect_status_msg_t *fsm = (ftp_connect_status_msg_t *) sm;
    Widget *wd = WIDGET (sm->dlg);
    WGroup *wg = GROUP (sm->dlg);
    WRect r;

    const char *b_name = _ ("&Abort");
    int b_width;
    int wd_width, y;

    b_width = str_term_width1 (b_name) + 4;
    wd_width = MAX (wd->rect.cols, b_width + 6);

    y = 2;
    ssm->label = label_new (y++, 3, NULL);
    group_add_widget_autopos (wg, ssm->label, WPOS_KEEP_TOP | WPOS_CENTER_HORZ, NULL);

    fsm->hline_w = WIDGET (hline_new (y++, -1, -1));
    group_add_widget (wg, fsm->hline_w);

    fsm->button_w = WIDGET (button_new (y++, 3, B_CANCEL, NORMAL_BUTTON, b_name, NULL));
    group_add_widget_autopos (wg, fsm->button_w, WPOS_KEEP_TOP | WPOS_CENTER_HORZ, NULL);

    r = wd->rect;
    r.lines = y + 2;
    r.cols = wd_width;
    widget_set_size_rect (wd, &r);
}

/* --------------------------------------------------------------------------------------------- */

static void
ftp_connect_status_deinit_cb (status_msg_t *sm)
{
    /* log is owned by the caller — not freed here */
    (void) sm;
}

/* --------------------------------------------------------------------------------------------- */

static int
ftp_connect_status_update_cb (status_msg_t *sm)
{
    simple_status_msg_t *ssm = SIMPLE_STATUS_MSG (sm);
    ftp_connect_status_msg_t *fsm = (ftp_connect_status_msg_t *) sm;
    Widget *wd = WIDGET (sm->dlg);
    Widget *lw = WIDGET (ssm->label);
    const char *text;
    int label_lines;
    WRect r;

    text = (fsm->log != NULL && fsm->log->len > 0) ? fsm->log->str : _ ("Please wait...");
    label_set_text (ssm->label, text);

    label_lines = lw->rect.lines;
    r = wd->rect;
    r.lines = MAX (r.lines, label_lines + 6);
    r.cols = MAX (r.cols, lw->rect.cols + 6);
    r.y = (LINES - r.lines) / 2;
    r.x = (COLS - r.cols) / 2;
    widget_set_size_rect (wd, &r);

    /* recenter label horizontally */
    {
        WRect lr = lw->rect;

        lr.x = r.x + (r.cols - lr.cols) / 2;
        widget_set_size_rect (lw, &lr);
    }

    /* reposition hline below label */
    if (fsm->hline_w != NULL)
    {
        WRect hr = fsm->hline_w->rect;

        hr.y = r.y + 2 + label_lines;
        widget_set_size_rect (fsm->hline_w, &hr);
    }

    /* reposition button below hline */
    if (fsm->button_w != NULL)
    {
        WRect br = fsm->button_w->rect;

        br.y = r.y + 3 + label_lines;
        br.x = r.x + (r.cols - br.cols) / 2;
        widget_set_size_rect (fsm->button_w, &br);
    }

    fsm->first = FALSE;

    return status_msg_common_update (sm);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
ftp_connect_status_set_stage (ftp_connect_status_msg_t *fsm, const char *fmt, ...)
{
    va_list ap;
    char *line;

    va_start (ap, fmt);
    line = g_strdup_vprintf (fmt, ap);
    va_end (ap);

    if (fsm->log->len > 0)
        g_string_append_c (fsm->log, '\n');
    g_string_append (fsm->log, line);
    g_free (line);

    return (STATUS_MSG (fsm)->update (STATUS_MSG (fsm)) != B_CANCEL);
}

/* --------------------------------------------------------------------------------------------- */

#if LIBCURL_VERSION_NUM >= 0x072000
static int
ftp_connect_progress_cb (void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal,
                         curl_off_t ulnow)
#else
static int
ftp_connect_progress_cb (void *clientp, double dltotal, double dlnow, double ultotal, double ulnow)
#endif
{
    ftp_connect_progress_t *progress = (ftp_connect_progress_t *) clientp;

    (void) dltotal;
    (void) dlnow;
    (void) ultotal;
    (void) ulnow;

    if (progress == NULL || progress->sm == NULL || progress->sm->update == NULL)
        return 0;

    return (progress->sm->update (progress->sm) == B_CANCEL) ? 1 : 0;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
ftp_activate_connection (ftp_data_t *data, ftp_connection_t *conn)
{
    ftp_connect_status_msg_t status;
    ftp_connect_progress_t progress;
    char *path;
    gboolean ok = FALSE;
    gboolean status_inited = FALSE;
    CURL *curl;
    CURLcode res;
    char *url;

    FTP_LOG ("activate: connecting to %s@%s:%d path=%s ssl_mode=%d passive=%d",
             conn->user != NULL ? conn->user : "(anon)", conn->host, conn->port,
             conn->path != NULL ? conn->path : "/", conn->ssl_mode, conn->passive_mode);

    memset (&status, 0, sizeof (status));
    status.first = TRUE;
    status.log = g_string_new (NULL);

    status_msg_init (STATUS_MSG (&status), _ ("FTP connection"), 0.0, ftp_connect_status_init_cb,
                     ftp_connect_status_update_cb, ftp_connect_status_deinit_cb);
    status_inited = TRUE;
    progress.sm = STATUS_MSG (&status);

    if (!ftp_connect_status_set_stage (&status, _ ("Connecting to %s:%d..."), conn->host,
                                       conn->port))
        goto out;

    /* test connectivity by listing root */
    url = ftp_build_url (conn, conn->path != NULL ? conn->path : "/");

    if (!ftp_connect_status_set_stage (&status, _ ("Initializing network session...")))
    {
        g_free (url);
        goto out;
    }

    curl = curl_easy_init ();
    if (curl == NULL)
    {
        FTP_LOG ("activate: curl_easy_init failed");
        g_free (url);
        goto out;
    }

    {
        char *dir_url;

        if (url[strlen (url) - 1] != '/')
        {
            dir_url = g_strdup_printf ("%s/", url);
            g_free (url);
        }
        else
        {
            dir_url = url;
        }

        {
            struct curl_slist *post_cmds;

            FTP_LOG ("activate: testing URL = %s", dir_url);
            curl_easy_setopt (curl, CURLOPT_URL, dir_url);
            ftp_setup_curl_common (curl, conn);
            curl_easy_setopt (curl, CURLOPT_NOBODY, 1L);
            curl_easy_setopt (curl, CURLOPT_DIRLISTONLY, 1L);
            curl_easy_setopt (curl, CURLOPT_NOPROGRESS, 0L);

            post_cmds = ftp_build_post_connect_commands (conn);
            if (post_cmds != NULL)
                curl_easy_setopt (curl, CURLOPT_QUOTE, post_cmds);

#if LIBCURL_VERSION_NUM >= 0x072000
            curl_easy_setopt (curl, CURLOPT_XFERINFOFUNCTION, ftp_connect_progress_cb);
            curl_easy_setopt (curl, CURLOPT_XFERINFODATA, &progress);
#else
            curl_easy_setopt (curl, CURLOPT_PROGRESSFUNCTION, ftp_connect_progress_cb);
            curl_easy_setopt (curl, CURLOPT_PROGRESSDATA, &progress);
#endif

            if (!ftp_connect_status_set_stage (&status,
                                               _ ("Authenticating and probing directory...")))
            {
                curl_slist_free_all (post_cmds);
                curl_easy_cleanup (curl);
                g_free (dir_url);
                goto out;
            }

            FTP_LOG ("activate: performing test request...");
            res = curl_easy_perform (curl);
            FTP_LOG ("activate: test result = %d (%s)", (int) res, curl_easy_strerror (res));
            curl_slist_free_all (post_cmds);
            curl_easy_cleanup (curl);
            g_free (dir_url);
        }
    }

    if (res != CURLE_OK)
    {
        FTP_LOG ("activate: initial connect failed, code=%d", (int) res);

        if (res == CURLE_ABORTED_BY_CALLBACK)
        {
            FTP_LOG ("activate: aborted by user");
            goto out;
        }

        if (!ftp_connect_status_set_stage (&status, _ ("Initial attempt failed: %s"),
                                           curl_easy_strerror (res)))
            goto out;

        /* if no stored password, prompt the user */
        if (conn->password == NULL || conn->password[0] == '\0')
        {
            char *pwd;
            char *prompt;

            status_msg_deinit (STATUS_MSG (&status));
            status_inited = FALSE;

            FTP_LOG ("activate: no stored password, prompting user");
            prompt = g_strdup_printf (_ ("Enter password for %s@%s"),
                                      conn->user != NULL ? conn->user : "", conn->host);
            pwd = input_dialog (_ ("FTP password"), prompt, "ftp-password", INPUT_PASSWORD,
                                INPUT_COMPLETE_NONE);
            g_free (prompt);

            if (pwd != NULL && pwd[0] != '\0')
            {
                g_free (conn->password);
                conn->password = pwd;

                /* retry with password */
                FTP_LOG ("activate: retrying with entered password...");
                status.first = TRUE;
                status_msg_init (STATUS_MSG (&status), _ ("FTP connection"), 0.0,
                                 ftp_connect_status_init_cb, ftp_connect_status_update_cb,
                                 ftp_connect_status_deinit_cb);
                status_inited = TRUE;

                url = ftp_build_url (conn, conn->path != NULL ? conn->path : "/");
                curl = curl_easy_init ();
                if (curl != NULL)
                {
                    char *dir_url2;

                    if (url[strlen (url) - 1] != '/')
                    {
                        dir_url2 = g_strdup_printf ("%s/", url);
                        g_free (url);
                    }
                    else
                    {
                        dir_url2 = url;
                    }

                    {
                        struct curl_slist *post_cmds2;

                        curl_easy_setopt (curl, CURLOPT_URL, dir_url2);
                        ftp_setup_curl_common (curl, conn);
                        curl_easy_setopt (curl, CURLOPT_NOBODY, 1L);
                        curl_easy_setopt (curl, CURLOPT_DIRLISTONLY, 1L);
                        curl_easy_setopt (curl, CURLOPT_NOPROGRESS, 0L);

                        post_cmds2 = ftp_build_post_connect_commands (conn);
                        if (post_cmds2 != NULL)
                            curl_easy_setopt (curl, CURLOPT_QUOTE, post_cmds2);

#if LIBCURL_VERSION_NUM >= 0x072000
                        curl_easy_setopt (curl, CURLOPT_XFERINFOFUNCTION, ftp_connect_progress_cb);
                        curl_easy_setopt (curl, CURLOPT_XFERINFODATA, &progress);
#else
                        curl_easy_setopt (curl, CURLOPT_PROGRESSFUNCTION, ftp_connect_progress_cb);
                        curl_easy_setopt (curl, CURLOPT_PROGRESSDATA, &progress);
#endif

                        if (!ftp_connect_status_set_stage (&status,
                                                           _ ("Retrying with password...")))
                        {
                            curl_slist_free_all (post_cmds2);
                            curl_easy_cleanup (curl);
                            g_free (dir_url2);
                            goto out;
                        }

                        res = curl_easy_perform (curl);
                        FTP_LOG ("activate: retry result = %d (%s)", (int) res,
                                 curl_easy_strerror (res));
                        curl_slist_free_all (post_cmds2);
                        curl_easy_cleanup (curl);
                        g_free (dir_url2);
                    }
                }
                else
                {
                    FTP_LOG ("activate: curl_easy_init failed on retry");
                    g_free (url);
                    goto out;
                }

                if (res != CURLE_OK)
                {
                    FTP_LOG ("activate: retry failed, giving up");
                    if (res != CURLE_ABORTED_BY_CALLBACK)
                        (void) ftp_connect_status_set_stage (&status, _ ("Retry failed: %s"),
                                                             curl_easy_strerror (res));
                    goto out;
                }
            }
            else
            {
                FTP_LOG ("activate: user cancelled password dialog");
                g_free (pwd);
                goto out;
            }
        }
        else
        {
            FTP_LOG ("activate: has password but connect failed, giving up");
            goto out;
        }
    }

    FTP_LOG ("activate: connection successful");
    if (!ftp_connect_status_set_stage (&status, _ ("Connection successful, loading entries...")))
        goto out;

    path = (conn->path != NULL && conn->path[0] != '\0') ? g_strdup (conn->path) : g_strdup ("/");
    if (path[0] != '/')
    {
        char *tmp = g_strdup_printf ("/%s", path);
        g_free (path);
        path = tmp;
    }

    data->active_connection = conn;
    g_free (data->current_path);
    data->current_path = path;
    data->at_root = FALSE;

    ftp_reload_entries (data, FALSE);
    ok = TRUE;

out:
    if (status_inited)
        status_msg_deinit (STATUS_MSG (&status));
    if (status.log != NULL)
        g_string_free (status.log, TRUE);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */
/* Plugin callbacks */
/* --------------------------------------------------------------------------------------------- */

static void *
ftp_open (mc_panel_host_t *host, const char *open_path)
{
    ftp_data_t *data;

    (void) open_path;

    FTP_LOG ("open: initializing plugin (open_path=%s)", open_path != NULL ? open_path : "(null)");

    if (ftp_curl_refcount == 0)
    {
        FTP_LOG ("open: calling curl_global_init");
        if (curl_global_init (CURL_GLOBAL_DEFAULT) != 0)
        {
            FTP_LOG ("open: curl_global_init FAILED");
            return NULL;
        }
        FTP_LOG ("open: curl_global_init OK");
    }
    ftp_curl_refcount++;

    data = g_new0 (ftp_data_t, 1);
    data->host = host;
    data->at_root = TRUE;
    data->current_path = NULL;
    data->entries = NULL;
    data->title_buf = NULL;
    data->active_connection = NULL;
    mc_pp_dir_cache_init (&data->dir_cache, FTP_CACHE_TTL_DEFAULT);

    data->key_edit = ftp_load_hotkey (FTP_PANEL_KEY_EDIT, FTP_PANEL_KEY_EDIT_DEFAULT, KEY_F (4));
    data->key_refresh =
        ftp_load_hotkey (FTP_PANEL_KEY_REFRESH, FTP_PANEL_KEY_REFRESH_DEFAULT, XCTRL ('r'));
    FTP_LOG ("open: keys edit=%d refresh=%d", data->key_edit, data->key_refresh);

    data->connections_file = get_connections_file_path ();
    FTP_LOG ("open: connections file = %s", data->connections_file);
    data->connections = load_connections (data->connections_file);
    FTP_LOG ("open: loaded %u connections", data->connections->len);

    return data;
}

/* --------------------------------------------------------------------------------------------- */

static void
ftp_close (void *plugin_data)
{
    ftp_data_t *data = (ftp_data_t *) plugin_data;

    FTP_LOG ("close: shutting down plugin");
    data->active_connection = NULL;

    mc_pp_dir_cache_destroy (&data->dir_cache);

    if (data->entries != NULL)
        g_ptr_array_free (data->entries, TRUE);

    g_ptr_array_free (data->connections, TRUE);

    g_free (data->current_path);
    g_free (data->title_buf);
    g_free (data->connections_file);
    g_free (data);

    if (ftp_curl_refcount > 0)
        ftp_curl_refcount--;

    if (ftp_curl_refcount == 0)
        curl_global_cleanup ();
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
ftp_get_items (void *plugin_data, void *list_ptr)
{
    ftp_data_t *data = (ftp_data_t *) plugin_data;
    guint i;

    FTP_LOG ("get_items: at_root=%d, entries=%u, path=%s", data->at_root,
             data->entries != NULL ? data->entries->len : 0,
             data->current_path != NULL ? data->current_path : "(null)");

    if (data->at_root)
    {
        FTP_LOG ("get_items: showing %u connections", data->connections->len);
        for (i = 0; i < data->connections->len; i++)
        {
            const ftp_connection_t *conn =
                (const ftp_connection_t *) g_ptr_array_index (data->connections, i);

            FTP_LOG ("get_items:   [%u] %s (%s:%d)", i, conn->label, conn->host, conn->port);
            mc_pp_add_entry (list_ptr, conn->label, S_IFDIR | 0755, 0, time (NULL));
        }
        return MC_PPR_OK;
    }

    if (data->entries != NULL)
    {
        FTP_LOG ("get_items: showing %u entries", data->entries->len);
        for (i = 0; i < data->entries->len; i++)
        {
            const mc_pp_dir_entry_t *e =
                (const mc_pp_dir_entry_t *) g_ptr_array_index (data->entries, i);
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
ftp_chdir (void *plugin_data, const char *path)
{
    ftp_data_t *data = (ftp_data_t *) plugin_data;

    FTP_LOG ("chdir: path='%s', at_root=%d, current_path='%s'", path, data->at_root,
             data->current_path != NULL ? data->current_path : "(null)");

    if (strcmp (path, "..") == 0)
    {
        if (data->at_root)
        {
            FTP_LOG ("chdir: at root, returning MC_PPR_CLOSE");
            return MC_PPR_CLOSE;
        }

        if (data->current_path != NULL && strcmp (data->current_path, "/") == 0)
        {
            mc_pp_dir_cache_clear (&data->dir_cache);
            data->at_root = TRUE;
            data->active_connection = NULL;

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
            char *parent = ftp_path_up (data->current_path);

            if (parent == NULL)
            {
                mc_pp_dir_cache_clear (&data->dir_cache);
                data->at_root = TRUE;
                data->active_connection = NULL;

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
            ftp_reload_entries (data, TRUE);
            return MC_PPR_OK;
        }
    }

    if (data->at_root)
    {
        ftp_connection_t *conn = (ftp_connection_t *) find_connection (data, path);

        if (conn == NULL)
            return MC_PPR_FAILED;

        return ftp_activate_connection (data, conn) ? MC_PPR_OK : MC_PPR_FAILED;
    }

    {
        const mc_pp_dir_entry_t *entry;
        char *new_path;

        entry = find_entry (data, path);
        if (entry == NULL || !entry->is_dir)
            return MC_PPR_FAILED;

        new_path = ftp_join_path (data->current_path, path);

        g_free (data->current_path);
        data->current_path = new_path;

        ftp_reload_entries (data, TRUE);
        return MC_PPR_OK;
    }
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
ftp_enter (void *plugin_data, const char *name, const struct stat *st)
{
    ftp_data_t *data = (ftp_data_t *) plugin_data;

    (void) st;

    FTP_LOG ("enter: name='%s', at_root=%d", name, data->at_root);

    if (data->at_root)
    {
        ftp_connection_t *conn = (ftp_connection_t *) find_connection (data, name);

        if (conn == NULL)
            return MC_PPR_FAILED;

        return ftp_activate_connection (data, conn) ? MC_PPR_OK : MC_PPR_FAILED;
    }

    {
        const mc_pp_dir_entry_t *entry;

        entry = find_entry (data, name);
        if (entry == NULL)
            return MC_PPR_FAILED;

        if (entry->is_dir)
        {
            char *new_path;

            new_path = ftp_join_path (data->current_path, name);
            g_free (data->current_path);
            data->current_path = new_path;

            ftp_reload_entries (data, TRUE);
            return MC_PPR_OK;
        }
    }

    return MC_PPR_NOT_SUPPORTED;
}

/* --------------------------------------------------------------------------------------------- */

typedef struct
{
    int fd;
} ftp_file_write_ctx_t;

static size_t
ftp_file_write_cb (void *ptr, size_t size, size_t nmemb, void *userdata)
{
    ftp_file_write_ctx_t *ctx = (ftp_file_write_ctx_t *) userdata;
    size_t total = size * nmemb;
    ssize_t written;

    written = write (ctx->fd, ptr, total);
    if (written < 0)
        return 0;

    return (size_t) written;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
ftp_get_local_copy (void *plugin_data, const char *fname, char **local_path)
{
    ftp_data_t *data = (ftp_data_t *) plugin_data;
    CURL *curl;
    CURLcode res;
    char *remote_path;
    char *url;
    int local_fd;
    GError *error = NULL;
    ftp_file_write_ctx_t ctx;

    if (data->at_root || data->active_connection == NULL || data->current_path == NULL)
        return MC_PPR_FAILED;

    remote_path = ftp_join_path (data->current_path, fname);
    url = ftp_build_url (data->active_connection, remote_path);
    g_free (remote_path);

    local_fd = g_file_open_tmp ("mc-pp-ftp-XXXXXX", local_path, &error);
    if (local_fd == -1)
    {
        if (error != NULL)
            g_error_free (error);
        g_free (url);
        return MC_PPR_FAILED;
    }

    curl = curl_easy_init ();
    if (curl == NULL)
    {
        close (local_fd);
        unlink (*local_path);
        g_free (*local_path);
        *local_path = NULL;
        g_free (url);
        return MC_PPR_FAILED;
    }

    ctx.fd = local_fd;

    curl_easy_setopt (curl, CURLOPT_URL, url);
    ftp_setup_curl_common (curl, data->active_connection);
    curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, ftp_file_write_cb);
    curl_easy_setopt (curl, CURLOPT_WRITEDATA, &ctx);

    res = curl_easy_perform (curl);
    curl_easy_cleanup (curl);
    g_free (url);
    close (local_fd);

    if (res != CURLE_OK)
    {
        unlink (*local_path);
        g_free (*local_path);
        *local_path = NULL;
        return MC_PPR_FAILED;
    }

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

typedef struct
{
    int fd;
    off_t remaining;
} ftp_file_read_ctx_t;

static size_t
ftp_file_read_cb (void *ptr, size_t size, size_t nmemb, void *userdata)
{
    ftp_file_read_ctx_t *ctx = (ftp_file_read_ctx_t *) userdata;
    size_t total = size * nmemb;
    ssize_t n;

    if (ctx->remaining <= 0)
        return 0;

    if (total > (size_t) ctx->remaining)
        total = (size_t) ctx->remaining;

    n = read (ctx->fd, ptr, total);
    if (n <= 0)
        return 0;

    ctx->remaining -= n;
    return (size_t) n;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
ftp_put_file (void *plugin_data, const char *local_path, const char *dest_name)
{
    ftp_data_t *data = (ftp_data_t *) plugin_data;
    CURL *curl;
    CURLcode res;
    char *remote_path;
    char *url;
    int local_fd;
    struct stat st_local;
    ftp_file_read_ctx_t ctx;

    if (data->at_root || data->active_connection == NULL || data->current_path == NULL)
        return MC_PPR_FAILED;

    local_fd = open (local_path, O_RDONLY);
    if (local_fd < 0)
        return MC_PPR_FAILED;

    if (fstat (local_fd, &st_local) != 0)
    {
        close (local_fd);
        return MC_PPR_FAILED;
    }

    remote_path = ftp_join_path (data->current_path, dest_name);
    url = ftp_build_url (data->active_connection, remote_path);
    g_free (remote_path);

    curl = curl_easy_init ();
    if (curl == NULL)
    {
        close (local_fd);
        g_free (url);
        return MC_PPR_FAILED;
    }

    ctx.fd = local_fd;
    ctx.remaining = st_local.st_size;

    curl_easy_setopt (curl, CURLOPT_URL, url);
    ftp_setup_curl_common (curl, data->active_connection);
    curl_easy_setopt (curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt (curl, CURLOPT_READFUNCTION, ftp_file_read_cb);
    curl_easy_setopt (curl, CURLOPT_READDATA, &ctx);
    curl_easy_setopt (curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t) st_local.st_size);
    curl_easy_setopt (curl, CURLOPT_FTP_CREATE_MISSING_DIRS, 0L);

    res = curl_easy_perform (curl);
    curl_easy_cleanup (curl);
    g_free (url);
    close (local_fd);

    if (res != CURLE_OK)
        return MC_PPR_FAILED;

    mc_pp_dir_cache_invalidate (&data->dir_cache, data->current_path);
    ftp_reload_entries (data, TRUE);

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
ftp_delete_items (void *plugin_data, const char **names, int count)
{
    ftp_data_t *data = (ftp_data_t *) plugin_data;
    int i;
    gboolean failed = FALSE;

    if (data->at_root)
    {
        for (i = 0; i < count; i++)
        {
            guint j;

            for (j = 0; j < data->connections->len; j++)
            {
                const ftp_connection_t *conn =
                    (const ftp_connection_t *) g_ptr_array_index (data->connections, j);

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

    if (data->active_connection == NULL)
        return MC_PPR_FAILED;

    for (i = 0; i < count; i++)
    {
        const mc_pp_dir_entry_t *entry;
        char *remote_path;
        char *url;
        char *cmd;
        CURL *curl;
        CURLcode res;
        struct curl_slist *header_list = NULL;

        entry = find_entry (data, names[i]);
        if (entry == NULL)
            continue;

        remote_path = ftp_join_path (data->current_path, names[i]);

        /* build URL pointing to parent directory */
        {
            char *parent_url;

            parent_url = ftp_build_url (data->active_connection, data->current_path);
            if (parent_url[strlen (parent_url) - 1] != '/')
            {
                url = g_strdup_printf ("%s/", parent_url);
                g_free (parent_url);
            }
            else
            {
                url = parent_url;
            }
        }

        if (entry->is_dir)
            cmd = g_strdup_printf ("RMD %s", remote_path);
        else
            cmd = g_strdup_printf ("DELE %s", remote_path);

        g_free (remote_path);

        curl = curl_easy_init ();
        if (curl == NULL)
        {
            g_free (url);
            g_free (cmd);
            failed = TRUE;
            continue;
        }

        header_list = curl_slist_append (header_list, cmd);

        curl_easy_setopt (curl, CURLOPT_URL, url);
        ftp_setup_curl_common (curl, data->active_connection);
        curl_easy_setopt (curl, CURLOPT_QUOTE, header_list);
        curl_easy_setopt (curl, CURLOPT_NOBODY, 1L);

        res = curl_easy_perform (curl);

        curl_slist_free_all (header_list);
        curl_easy_cleanup (curl);
        g_free (url);
        g_free (cmd);

        if (res != CURLE_OK)
            failed = TRUE;
    }

    mc_pp_dir_cache_invalidate (&data->dir_cache, data->current_path);
    ftp_reload_entries (data, TRUE);

    return failed ? MC_PPR_FAILED : MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
ftp_get_title (void *plugin_data)
{
    ftp_data_t *data = (ftp_data_t *) plugin_data;

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
ftp_create_item (void *plugin_data)
{
    ftp_data_t *data = (ftp_data_t *) plugin_data;
    ftp_connection_t *conn;

    if (!data->at_root)
        return MC_PPR_NOT_SUPPORTED;

    conn = g_new0 (ftp_connection_t, 1);
    conn->port = FTP_DEFAULT_PORT;
    conn->user = vfs_get_local_username ();
    if (conn->user == NULL)
        conn->user = g_strdup (g_get_user_name ());
    conn->path = g_strdup ("/");
    conn->passive_mode = TRUE;

    if (!show_connection_dialog (conn))
    {
        ftp_connection_free (conn);
        return MC_PPR_FAILED;
    }

    if (conn->label == NULL || conn->label[0] == '\0' || conn->host == NULL
        || conn->host[0] == '\0')
    {
        ftp_connection_free (conn);
        return MC_PPR_FAILED;
    }

    g_ptr_array_add (data->connections, conn);
    save_connections (data->connections_file, data->connections);

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
ftp_edit_connection (ftp_data_t *data)
{
    const GString *current_name;
    ftp_connection_t *conn;

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
            ftp_connection_t *c = (ftp_connection_t *) g_ptr_array_index (data->connections, i);

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
ftp_view_item (void *plugin_data, const char *fname, const struct stat *st, gboolean plain_view)
{
    ftp_data_t *data = (ftp_data_t *) plugin_data;
    const ftp_connection_t *conn;
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
    g_string_append_printf (ini, "port=%d\n", conn->port > 0 ? conn->port : FTP_DEFAULT_PORT);
    if (conn->user != NULL)
        g_string_append_printf (ini, "user=%s\n", conn->user);
    if (conn->path != NULL)
        g_string_append_printf (ini, "path=%s\n", conn->path);
    if (conn->password != NULL)
        g_string_append (ini, "password=***\n");
    g_string_append_printf (ini, "passive_mode=%s\n", conn->passive_mode ? "true" : "false");

    {
        static const char *ssl_labels[] = { "none", "try", "control", "all" };
        int idx = conn->ssl_mode;

        if (idx < 0 || idx > 3)
            idx = 0;
        g_string_append_printf (ini, "ssl_mode=%s\n", ssl_labels[idx]);
    }

    if (conn->encoding != NULL && conn->encoding[0] != '\0')
        g_string_append_printf (ini, "encoding=%s\n", conn->encoding);
    if (conn->timeout > 0)
        g_string_append_printf (ini, "timeout=%d\n", conn->timeout);
    if (conn->connect_timeout > 0)
        g_string_append_printf (ini, "connect_timeout=%d\n", conn->connect_timeout);
    g_string_append_printf (ini, "keepalive=%s\n", conn->keepalive ? "true" : "false");
    if (conn->keepalive_interval > 0)
        g_string_append_printf (ini, "keepalive_interval=%d\n", conn->keepalive_interval);
    if (conn->ip_version != 0)
        g_string_append_printf (ini, "ip_version=%d\n", conn->ip_version);
    if (conn->post_connect_commands != NULL && conn->post_connect_commands[0] != '\0')
        g_string_append_printf (ini, "post_connect_commands=%s\n", conn->post_connect_commands);

    if (conn->proxy_type > 0)
    {
        static const char *proxy_labels[] = { "none", "HTTP", "SOCKS4", "SOCKS5" };
        int pidx = conn->proxy_type;

        if (pidx < 0 || pidx > 3)
            pidx = 0;
        g_string_append_printf (ini, "proxy_type=%s\n", proxy_labels[pidx]);
        if (conn->proxy_host != NULL && conn->proxy_host[0] != '\0')
            g_string_append_printf (ini, "proxy_host=%s\n", conn->proxy_host);
        if (conn->proxy_port > 0)
            g_string_append_printf (ini, "proxy_port=%d\n", conn->proxy_port);
        if (conn->proxy_user != NULL && conn->proxy_user[0] != '\0')
            g_string_append_printf (ini, "proxy_user=%s\n", conn->proxy_user);
        if (conn->proxy_password != NULL)
            g_string_append (ini, "proxy_password=***\n");
    }

    fd = g_file_open_tmp ("mc-ftp-view-XXXXXX", &tmp_path, &error);
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
ftp_handle_key (void *plugin_data, int key)
{
    ftp_data_t *data = (ftp_data_t *) plugin_data;

    if (key == CK_Edit || (data->key_edit >= 0 && key == data->key_edit))
        return ftp_edit_connection (data);

    if (data->key_refresh >= 0 && key == data->key_refresh)
    {
        FTP_LOG ("handle_key: refresh, invalidating cache for '%s'",
                 data->current_path != NULL ? data->current_path : "(null)");
        mc_pp_dir_cache_invalidate (&data->dir_cache, data->current_path);
        return MC_PPR_OK;
    }

    return MC_PPR_NOT_SUPPORTED;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

const mc_panel_plugin_t *mc_panel_plugin_register (void);

const mc_panel_plugin_t *
mc_panel_plugin_register (void)
{
    return &ftp_plugin;
}

/* --------------------------------------------------------------------------------------------- */
