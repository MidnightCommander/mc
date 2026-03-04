/*
   Shell link connection manager panel plugin.

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

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/keybind.h"
#include "lib/mcconfig.h"
#include "lib/panel-plugin.h"
#include "lib/tty/key.h"
#include "lib/util.h"
#include "lib/widget.h"

#include "src/viewer/mcviewer.h"

/*** file scope type declarations ****************************************************************/

typedef struct
{
    char *label;         /* connection name */
    char *host;          /* hostname */
    char *user;          /* username, NULL = current */
    char *password;      /* stored password (optional) */
    char *path;          /* initial path, NULL = / */
    gboolean compressed; /* SSH compression (-C) */
} shell_connection_t;

typedef struct
{
    mc_panel_host_t *host;
    GPtrArray *connections;
    char *connections_file;
    int key_edit;
    int key_clone;
    char *title_buf;
} shell_data_t;

/*** forward declarations (file scope functions) *************************************************/

static void *shell_open (mc_panel_host_t *host, const char *open_path);
static void shell_close (void *plugin_data);
static mc_pp_result_t shell_get_items (void *plugin_data, void *list_ptr);
static mc_pp_result_t shell_enter (void *plugin_data, const char *name, const struct stat *st);
static mc_pp_result_t shell_delete_items (void *plugin_data, const char **names, int count);
static const char *shell_get_title (void *plugin_data);
static mc_pp_result_t shell_create_item (void *plugin_data);
static mc_pp_result_t shell_view_item (void *plugin_data, const char *fname, const struct stat *st,
                                       gboolean plain_view);
static mc_pp_result_t shell_handle_key (void *plugin_data, int key);

/*** file scope variables ************************************************************************/

#define SHELL_PANEL_CONFIG_FILE       "panels.shell-link.ini"
#define SHELL_PANEL_CONFIG_GROUP      "shell-link-panel"
#define SHELL_PANEL_KEY_EDIT          "hotkey_edit"
#define SHELL_PANEL_KEY_EDIT_DEFAULT  "f4"
#define SHELL_PANEL_KEY_CLONE         "hotkey_clone"
#define SHELL_PANEL_KEY_CLONE_DEFAULT "shift-f5"

/* KEY_F(n) = 1000 + n, XCTRL(c) = c & 0x1f — matching lib/tty definitions */
#define SHELL_KEY_F(n)   (1000 + (n))
#define SHELL_XCTRL(c)   ((c) & 0x1f)

#define SHELL_DLG_HEIGHT 16
#define SHELL_DLG_WIDTH  52

static const mc_panel_plugin_t shell_plugin = {
    .api_version = MC_PANEL_PLUGIN_API_VERSION,
    .name = "shell-link",
    .display_name = "Shell link (FISH)",
    .proto = "sh",
    .prefix = NULL,
    .flags = MC_PPF_CUSTOM_TITLE | MC_PPF_CREATE | MC_PPF_DELETE | MC_PPF_SHOW_IN_MENU,

    .open = shell_open,
    .close = shell_close,
    .get_items = shell_get_items,

    .chdir = NULL,
    .enter = shell_enter,
    .view = shell_view_item,
    .get_local_copy = NULL,
    .put_file = NULL,
    .save_file = NULL,
    .delete_items = shell_delete_items,
    .get_title = shell_get_title,
    .handle_key = shell_handle_key,
    .create_item = shell_create_item,
};

/*** file scope functions ************************************************************************/

static void
shell_connection_free (gpointer p)
{
    shell_connection_t *c = (shell_connection_t *) p;

    g_free (c->label);
    g_free (c->host);
    g_free (c->user);
    g_free (c->password);
    g_free (c->path);
    g_free (c);
}

/* --------------------------------------------------------------------------------------------- */

/* Simple XOR obfuscation to avoid storing passwords in plain text.
   NOT cryptographically secure - only prevents casual reading. */

static const unsigned char shell_obfuscation_key[] = "Mc4ShellPanelKey!";

static char *
shell_password_encode (const char *plain)
{
    size_t i, len, klen;
    guchar *xored;
    gchar *b64;
    char *result;

    if (plain == NULL || plain[0] == '\0')
        return NULL;

    len = strlen (plain);
    klen = sizeof (shell_obfuscation_key) - 1;
    xored = g_new (guchar, len);

    for (i = 0; i < len; i++)
        xored[i] = (guchar) plain[i] ^ shell_obfuscation_key[i % klen];

    b64 = g_base64_encode (xored, len);
    g_free (xored);

    result = g_strdup_printf ("enc:%s", b64);
    g_free (b64);

    return result;
}

/* --------------------------------------------------------------------------------------------- */

static char *
shell_password_decode (const char *encoded)
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

    klen = sizeof (shell_obfuscation_key) - 1;
    plain = g_new (char, len + 1);

    for (i = 0; i < len; i++)
        plain[i] = (char) (xored[i] ^ shell_obfuscation_key[i % klen]);
    plain[len] = '\0';

    g_free (xored);
    return plain;
}

/* --------------------------------------------------------------------------------------------- */

static char *
shell_read_config_string (const char *path, const char *key)
{
    char *value;
    mc_config_t *cfg;

    if (path == NULL || !g_file_test (path, G_FILE_TEST_IS_REGULAR))
        return NULL;

    cfg = mc_config_init (path, TRUE);
    if (cfg == NULL)
        return NULL;

    value = mc_config_get_string (cfg, SHELL_PANEL_CONFIG_GROUP, key, NULL);
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
shell_save_config_defaults (const char *path)
{
    mc_config_t *cfg;

    if (path == NULL || g_file_test (path, G_FILE_TEST_EXISTS))
        return;

    cfg = mc_config_init (path, FALSE);
    if (cfg == NULL)
        return;

    mc_config_set_string (cfg, SHELL_PANEL_CONFIG_GROUP, SHELL_PANEL_KEY_EDIT,
                          SHELL_PANEL_KEY_EDIT_DEFAULT);
    mc_config_set_string (cfg, SHELL_PANEL_CONFIG_GROUP, SHELL_PANEL_KEY_CLONE,
                          SHELL_PANEL_KEY_CLONE_DEFAULT);
    mc_config_save_file (cfg, NULL);
    mc_config_deinit (cfg);
}

/* --------------------------------------------------------------------------------------------- */

static int
shell_parse_hotkey (const char *value, int fallback)
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
        ret = SHELL_XCTRL (g_ascii_tolower ((guchar) s[5]));
    else if (s[0] == 'f' && s[1] != '\0' && g_ascii_isdigit ((guchar) s[1]) != 0)
    {
        int n = atoi (s + 1);

        if (n >= 1 && n <= 24)
            ret = SHELL_KEY_F (n);
    }
    else if ((strncmp (s, "shift-f", 7) == 0 || strncmp (s, "s-f", 3) == 0)
             && g_ascii_isdigit ((guchar) s[strncmp (s, "shift-f", 7) == 0 ? 7 : 3]) != 0)
    {
        int base = atoi (s + (strncmp (s, "shift-f", 7) == 0 ? 7 : 3));

        if (base >= 1 && base <= 12)
            ret = SHELL_KEY_F (base + 10);
    }

    g_free (s);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static int
shell_load_hotkey (const char *key, const char *fallback_text, int fallback_key)
{
    char *config_path;
    char *value;
    int hotkey;

    config_path = g_build_filename (mc_config_get_path (), SHELL_PANEL_CONFIG_FILE, (char *) NULL);

    value = shell_read_config_string (config_path, key);
    if (value == NULL && mc_global.sysconfig_dir != NULL)
    {
        char *sys_path;

        sys_path =
            g_build_filename (mc_global.sysconfig_dir, SHELL_PANEL_CONFIG_FILE, (char *) NULL);
        value = shell_read_config_string (sys_path, key);
        g_free (sys_path);
    }

    shell_save_config_defaults (config_path);
    g_free (config_path);

    if (value == NULL)
        value = g_strdup (fallback_text);

    hotkey = shell_parse_hotkey (value, fallback_key);
    g_free (value);
    return hotkey;
}

/* --------------------------------------------------------------------------------------------- */

static char *
get_connections_file_path (void)
{
    return g_build_filename (g_get_user_config_dir (), "mc", "shell-connections.ini",
                             (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

static GPtrArray *
load_connections (const char *filepath)
{
    GPtrArray *arr;
    GKeyFile *kf;
    gchar **groups;
    gsize n_groups, i;

    arr = g_ptr_array_new_with_free_func (shell_connection_free);

    kf = g_key_file_new ();
    if (!g_key_file_load_from_file (kf, filepath, G_KEY_FILE_NONE, NULL))
    {
        g_key_file_free (kf);
        return arr;
    }

    groups = g_key_file_get_groups (kf, &n_groups);
    for (i = 0; i < n_groups; i++)
    {
        shell_connection_t *conn;
        GError *error = NULL;

        conn = g_new0 (shell_connection_t, 1);
        conn->label = g_strdup (groups[i]);
        conn->host = g_key_file_get_string (kf, groups[i], "host", NULL);
        conn->user = g_key_file_get_string (kf, groups[i], "user", NULL);
        {
            char *raw_pw = g_key_file_get_string (kf, groups[i], "password", NULL);

            conn->password = shell_password_decode (raw_pw);
            g_free (raw_pw);
        }
        conn->path = g_key_file_get_string (kf, groups[i], "path", NULL);

        conn->compressed = g_key_file_get_boolean (kf, groups[i], "compressed", &error);
        if (error != NULL)
        {
            g_error_free (error);
            conn->compressed = FALSE;
        }

        if (conn->host == NULL || conn->host[0] == '\0')
        {
            shell_connection_free (conn);
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
        const shell_connection_t *conn =
            (const shell_connection_t *) g_ptr_array_index (connections, i);

        g_key_file_set_string (kf, conn->label, "host", conn->host);

        if (conn->user != NULL)
            g_key_file_set_string (kf, conn->label, "user", conn->user);
        if (conn->password != NULL && conn->password[0] != '\0')
        {
            char *enc = shell_password_encode (conn->password);

            if (enc != NULL)
                g_key_file_set_string (kf, conn->label, "password", enc);
            g_free (enc);
        }
        if (conn->path != NULL)
            g_key_file_set_string (kf, conn->label, "path", conn->path);
        g_key_file_set_boolean (kf, conn->label, "compressed", conn->compressed);
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

static const shell_connection_t *
find_connection (const shell_data_t *data, const char *label)
{
    guint i;

    for (i = 0; i < data->connections->len; i++)
    {
        const shell_connection_t *c =
            (const shell_connection_t *) g_ptr_array_index (data->connections, i);

        if (strcmp (c->label, label) == 0)
            return c;
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
shell_label_exists (const shell_data_t *data, const char *label,
                    const shell_connection_t *except_conn)
{
    guint i;

    if (label == NULL || label[0] == '\0')
        return FALSE;

    for (i = 0; i < data->connections->len; i++)
    {
        const shell_connection_t *c =
            (const shell_connection_t *) g_ptr_array_index (data->connections, i);

        if (c == except_conn)
            continue;

        if (strcmp (c->label, label) == 0)
            return TRUE;
    }

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static char *
shell_build_url (const shell_connection_t *conn)
{
    const char *path;
    char *userinfo = NULL;
    char *user_esc = NULL;
    char *pass_esc = NULL;

    path = (conn->path != NULL) ? conn->path : "";
    while (*path == '/')
        path++;

    if (conn->user != NULL && conn->user[0] != '\0')
    {
        user_esc = g_uri_escape_string (conn->user, NULL, FALSE);

        if (conn->password != NULL && conn->password[0] != '\0')
        {
            pass_esc = g_uri_escape_string (conn->password, NULL, FALSE);
            userinfo = g_strdup_printf ("%s:%s", user_esc, pass_esc);
        }
        else
            userinfo = g_strdup (user_esc);
    }

    g_free (user_esc);
    g_free (pass_esc);

    if (userinfo != NULL)
    {
        char *url;

        url = g_strdup_printf ("/sh://%s@%s%s/%s", userinfo, conn->host,
                               conn->compressed ? ":C" : "", path);
        g_free (userinfo);
        return url;
    }

    return g_strdup_printf ("/sh://%s%s/%s", conn->host, conn->compressed ? ":C" : "", path);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
show_connection_dialog (shell_connection_t *conn)
{
    char *label = g_strdup (conn->label != NULL ? conn->label : "");
    char *host = g_strdup (conn->host != NULL ? conn->host : "");
    char *user = g_strdup (conn->user != NULL ? conn->user : "");
    char *password = g_strdup (conn->password != NULL ? conn->password : "");
    char *path = g_strdup (conn->path != NULL ? conn->path : "");
    gboolean compressed = conn->compressed;
    int ret;

    /* clang-format off */
    quick_widget_t quick_widgets[] = {
        QUICK_LABELED_INPUT (N_("Connection name:"), input_label_above,
                            label, "shell-conn-label",
                            &label, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
        QUICK_LABELED_INPUT (N_("Host:"), input_label_above,
                            host, "shell-conn-host",
                            &host, NULL, FALSE, FALSE, INPUT_COMPLETE_HOSTNAMES),
        QUICK_LABELED_INPUT (N_("User:"), input_label_above,
                            user, "shell-conn-user",
                            &user, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
        QUICK_LABELED_INPUT (N_("Password:"), input_label_above,
                            password, "shell-conn-pass",
                            &password, NULL, TRUE, TRUE, INPUT_COMPLETE_NONE),
        QUICK_LABELED_INPUT (N_("Remote path:"), input_label_above,
                            path, "shell-conn-path",
                            &path, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
        QUICK_CHECKBOX (N_("SSH &compression"), &compressed, NULL),
        QUICK_BUTTONS_OK_CANCEL,
        QUICK_END,
    };
    /* clang-format on */

    WRect r = { -1, -1, SHELL_DLG_HEIGHT, SHELL_DLG_WIDTH };

    quick_dialog_t qdlg = {
        .rect = r,
        .title = N_ ("Shell Link Connection"),
        .help = "[Shell Link Plugin]",
        .widgets = quick_widgets,
        .callback = NULL,
        .mouse_callback = NULL,
    };

    ret = quick_dialog (&qdlg);

    if (ret == B_ENTER)
    {
        g_free (conn->label);
        conn->label = label;

        g_free (conn->host);
        conn->host = host;

        g_free (conn->user);
        conn->user = (user != NULL && user[0] != '\0') ? user : NULL;
        if (conn->user == NULL)
            g_free (user);

        g_free (conn->password);
        conn->password = (password != NULL && password[0] != '\0') ? password : NULL;
        if (conn->password == NULL)
            g_free (password);

        g_free (conn->path);
        conn->path = (path != NULL && path[0] != '\0') ? path : NULL;
        if (conn->path == NULL)
            g_free (path);

        conn->compressed = compressed;
        return TRUE;
    }

    g_free (label);
    g_free (host);
    g_free (user);
    g_free (password);
    g_free (path);
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
shell_edit_connection (shell_data_t *data)
{
    const GString *current_name;
    shell_connection_t *conn;
    char *old_label;
    char *old_host;
    char *old_user;
    char *old_password;
    char *old_path;
    gboolean old_compressed;

    current_name = data->host->get_current (data->host);
    if (current_name == NULL || current_name->len == 0)
        return MC_PPR_FAILED;

    conn = NULL;
    {
        guint i;

        for (i = 0; i < data->connections->len; i++)
        {
            shell_connection_t *c = (shell_connection_t *) g_ptr_array_index (data->connections, i);

            if (strcmp (c->label, current_name->str) == 0)
            {
                conn = c;
                break;
            }
        }
    }

    if (conn == NULL)
        return MC_PPR_FAILED;

    old_label = g_strdup (conn->label);
    old_host = g_strdup (conn->host);
    old_user = g_strdup (conn->user);
    old_password = g_strdup (conn->password);
    old_path = g_strdup (conn->path);
    old_compressed = conn->compressed;

    if (!show_connection_dialog (conn))
    {
        g_free (old_label);
        g_free (old_host);
        g_free (old_user);
        g_free (old_password);
        g_free (old_path);
        return MC_PPR_FAILED;
    }

    if (conn->label == NULL || conn->label[0] == '\0' || conn->host == NULL
        || conn->host[0] == '\0')
    {
        g_free (conn->label);
        g_free (conn->host);
        g_free (conn->user);
        g_free (conn->password);
        g_free (conn->path);
        conn->label = old_label;
        conn->host = old_host;
        conn->user = old_user;
        conn->password = old_password;
        conn->path = old_path;
        conn->compressed = old_compressed;
        return MC_PPR_FAILED;
    }

    if (shell_label_exists (data, conn->label, conn))
    {
        message (D_ERROR, MSG_ERROR, _ ("Connection with this name already exists"));
        g_free (conn->label);
        g_free (conn->host);
        g_free (conn->user);
        g_free (conn->password);
        g_free (conn->path);
        conn->label = old_label;
        conn->host = old_host;
        conn->user = old_user;
        conn->password = old_password;
        conn->path = old_path;
        conn->compressed = old_compressed;
        return MC_PPR_FAILED;
    }

    g_free (old_label);
    g_free (old_host);
    g_free (old_user);
    g_free (old_password);
    g_free (old_path);

    save_connections (data->connections_file, data->connections);

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */
/* Plugin callbacks */
/* --------------------------------------------------------------------------------------------- */

static void *
shell_open (mc_panel_host_t *host, const char *open_path)
{
    shell_data_t *data;

    (void) open_path;

    data = g_new0 (shell_data_t, 1);
    data->host = host;
    data->title_buf = NULL;
    data->key_edit =
        shell_load_hotkey (SHELL_PANEL_KEY_EDIT, SHELL_PANEL_KEY_EDIT_DEFAULT, SHELL_KEY_F (4));
    data->key_clone =
        shell_load_hotkey (SHELL_PANEL_KEY_CLONE, SHELL_PANEL_KEY_CLONE_DEFAULT, SHELL_KEY_F (15));

    data->connections_file = get_connections_file_path ();
    data->connections = load_connections (data->connections_file);

    return data;
}

/* --------------------------------------------------------------------------------------------- */

static void
shell_close (void *plugin_data)
{
    shell_data_t *data = (shell_data_t *) plugin_data;

    g_ptr_array_free (data->connections, TRUE);

    g_free (data->title_buf);
    g_free (data->connections_file);
    g_free (data);
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
shell_get_items (void *plugin_data, void *list_ptr)
{
    shell_data_t *data = (shell_data_t *) plugin_data;
    guint i;

    for (i = 0; i < data->connections->len; i++)
    {
        const shell_connection_t *conn =
            (const shell_connection_t *) g_ptr_array_index (data->connections, i);

        mc_pp_add_entry (list_ptr, conn->label, S_IFDIR | 0755, 0, time (NULL));
    }

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
shell_enter (void *plugin_data, const char *name, const struct stat *st)
{
    shell_data_t *data = (shell_data_t *) plugin_data;
    const shell_connection_t *conn;
    char *url;

    (void) st;

    conn = find_connection (data, name);
    if (conn == NULL)
        return MC_PPR_FAILED;

    url = shell_build_url (conn);

    data->host->close_plugin (data->host, url);
    g_free (url);

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
shell_delete_items (void *plugin_data, const char **names, int count)
{
    shell_data_t *data = (shell_data_t *) plugin_data;
    int i;

    for (i = 0; i < count; i++)
    {
        guint j;

        for (j = 0; j < data->connections->len; j++)
        {
            const shell_connection_t *conn =
                (const shell_connection_t *) g_ptr_array_index (data->connections, j);

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

/* --------------------------------------------------------------------------------------------- */

static const char *
shell_get_title (void *plugin_data)
{
    shell_data_t *data = (shell_data_t *) plugin_data;

    g_free (data->title_buf);
    data->title_buf = g_strdup ("/");

    return data->title_buf;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
shell_create_item (void *plugin_data)
{
    shell_data_t *data = (shell_data_t *) plugin_data;
    shell_connection_t *conn;

    conn = g_new0 (shell_connection_t, 1);

    if (!show_connection_dialog (conn))
    {
        shell_connection_free (conn);
        return MC_PPR_FAILED;
    }

    if (conn->label == NULL || conn->label[0] == '\0' || conn->host == NULL
        || conn->host[0] == '\0')
    {
        shell_connection_free (conn);
        return MC_PPR_FAILED;
    }

    if (shell_label_exists (data, conn->label, NULL))
    {
        message (D_ERROR, MSG_ERROR, _ ("Connection with this name already exists"));
        shell_connection_free (conn);
        return MC_PPR_FAILED;
    }

    g_ptr_array_add (data->connections, conn);
    save_connections (data->connections_file, data->connections);

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
shell_view_item (void *plugin_data, const char *fname, const struct stat *st, gboolean plain_view)
{
    shell_data_t *data = (shell_data_t *) plugin_data;
    const shell_connection_t *conn;
    GString *ini;
    GError *error = NULL;
    char *tmp_path = NULL;
    int fd;

    (void) st;
    (void) plain_view;

    if (fname == NULL)
        return MC_PPR_FAILED;

    conn = find_connection (data, fname);
    if (conn == NULL)
        return MC_PPR_FAILED;

    ini = g_string_new ("");
    g_string_append_printf (ini, "[%s]\n", conn->label);
    g_string_append_printf (ini, "host=%s\n", conn->host);
    if (conn->user != NULL)
        g_string_append_printf (ini, "user=%s\n", conn->user);
    if (conn->password != NULL)
        g_string_append (ini, "password=***\n");
    if (conn->path != NULL)
        g_string_append_printf (ini, "path=%s\n", conn->path);
    g_string_append_printf (ini, "compressed=%s\n", conn->compressed ? "true" : "false");

    fd = g_file_open_tmp ("mc-shell-view-XXXXXX", &tmp_path, &error);
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
shell_clone_connection (shell_data_t *data)
{
    const GString *current_name;
    const shell_connection_t *src;
    shell_connection_t *conn;
    char *new_label;

    current_name = data->host->get_current (data->host);
    if (current_name == NULL || current_name->len == 0)
        return MC_PPR_FAILED;

    src = find_connection (data, current_name->str);
    if (src == NULL)
        return MC_PPR_FAILED;

    new_label = input_dialog (_ ("Clone Connection"), _ ("New connection name:"),
                              "shell-link-clone", src->label, INPUT_COMPLETE_NONE);
    if (new_label == NULL || new_label[0] == '\0')
    {
        g_free (new_label);
        return MC_PPR_FAILED;
    }

    if (shell_label_exists (data, new_label, NULL))
    {
        message (D_ERROR, MSG_ERROR, _ ("Connection with this name already exists"));
        g_free (new_label);
        return MC_PPR_FAILED;
    }

    conn = g_new0 (shell_connection_t, 1);
    conn->label = new_label;
    conn->host = g_strdup (src->host);
    conn->user = g_strdup (src->user);
    conn->password = g_strdup (src->password);
    conn->path = g_strdup (src->path);
    conn->compressed = src->compressed;

    g_ptr_array_add (data->connections, conn);
    save_connections (data->connections_file, data->connections);

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
shell_handle_key (void *plugin_data, int key)
{
    shell_data_t *data = (shell_data_t *) plugin_data;

    if (key == CK_Edit || (data->key_edit >= 0 && key == data->key_edit))
        return shell_edit_connection (data);

    /* Shift+F5 — clone connection */
    if (data->key_clone >= 0 && key == data->key_clone)
        return shell_clone_connection (data);

    return MC_PPR_NOT_SUPPORTED;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

const mc_panel_plugin_t *mc_panel_plugin_register (void);

const mc_panel_plugin_t *
mc_panel_plugin_register (void)
{
    return &shell_plugin;
}

/* --------------------------------------------------------------------------------------------- */
