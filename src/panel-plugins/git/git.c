/*
   Git status panel plugin (MVP): shows changed files from git status.

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2026.
 */

#include <config.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/logging.h"
#include "lib/mcconfig.h"
#include "lib/panel-plugin.h"
#include "lib/keybind.h"
#include "lib/tty/key.h"
#include "lib/util.h"
#include "lib/vfs/vfs.h"
#include "lib/widget.h"

#include "src/filemanager/dir.h"
#include "src/viewer/mcviewer.h"

/*** file scope type declarations ****************************************************************/

typedef struct
{
    int kind;
    char *repo_path;
    char *old_repo_path;
    char *full_path;
    char *state_text;
    char *state_mark_text;
    char *commit_sha;
    gboolean is_deleted;
    gboolean is_staged;
} git_entry_info_t;

typedef struct
{
    mc_panel_host_t *host;
    char *repo_root;
    char *title;
    char *help_filename;
    char *default_format;
    int key_stage;
    int key_unstage;
    int key_toggle;
    int key_diff;
    int key_diff_alt;
    int key_refresh;
    int key_reset;
    int view;
    char *selected_commit;
    char *current_branch;
    char *selected_branch;
    char *selected_remote;
    char *selected_commit_name;
    char *selected_commit_list_name;
    int selected_branch_scope;
    char *pending_focus;
    GPtrArray *commit_stack; /* stack of git_commit_nav_t* */
    GHashTable *display_to_info; /* key: panel display name, value: git_entry_info_t* */
} git_data_t;

typedef struct
{
    char *commit_sha;
    char *commit_name;
    char *commit_list_name;
} git_commit_nav_t;

/*** forward declarations (file scope functions) *************************************************/

static void *git_open (mc_panel_host_t *host, const char *open_path);
static void git_close (void *plugin_data);
static mc_pp_result_t git_get_items (void *plugin_data, void *list_ptr);
static mc_pp_result_t git_chdir (void *plugin_data, const char *path);
static mc_pp_result_t git_enter (void *plugin_data, const char *fname, const struct stat *st);
static mc_pp_result_t git_view_item (void *plugin_data, const char *fname, const struct stat *st,
                                     gboolean plain_view);
static mc_pp_result_t git_get_help_info (void *plugin_data, const char **filename, const char **node);
static mc_pp_result_t git_get_local_copy (void *plugin_data, const char *fname, char **local_path);
static const char *git_get_title (void *plugin_data);
static mc_pp_result_t git_handle_key (void *plugin_data, int key);
static const mc_panel_column_t *git_get_columns (void *plugin_data, size_t *count);
static const char *git_get_column_value (void *plugin_data, const char *fname, const char *column_id);
static const char *git_get_footer (void *plugin_data);
static const char *git_get_focus_name (void *plugin_data);
static const char *git_get_default_format (void *plugin_data);
static void git_debug_log (const char *fmt, ...) G_GNUC_PRINTF (1, 2);
static const char *git_view_name (int view);

/*** file scope variables ************************************************************************/

static const mc_panel_plugin_t git_plugin = {
    .api_version = MC_PANEL_PLUGIN_API_VERSION,
    .name = "git",
    .display_name = "Git status",
    .proto = "git",
    .prefix = "git:",
    .flags = MC_PPF_NAVIGATE | MC_PPF_GET_FILES | MC_PPF_CUSTOM_TITLE,

    .open = git_open,
    .close = git_close,
    .get_items = git_get_items,

    .chdir = git_chdir,
    .enter = git_enter,
    .view = git_view_item,
    .get_help_info = git_get_help_info,
    .get_local_copy = git_get_local_copy,
    .delete_items = NULL,
    .get_title = git_get_title,
    .handle_key = git_handle_key,
    .create_item = NULL,
    .get_columns = git_get_columns,
    .get_column_value = git_get_column_value,
    .get_footer = git_get_footer,
    .get_focus_name = git_get_focus_name,
    .get_default_format = git_get_default_format,
};

static const mc_panel_column_t git_columns[] = {
    { "type", "", 1, FALSE, J_LEFT_FIT, TRUE },
    { "status", "Status", 11, FALSE, J_LEFT_FIT, TRUE },
};

/*** file scope functions ************************************************************************/

#define GIT_PANEL_CONFIG_FILE        "panels.git.ini"
#define GIT_PANEL_CONFIG_FILE_LEGACY "git-panel.ini"
#define GIT_PANEL_CONFIG_GROUP       "git-panel"
#define GIT_PANEL_FORMAT_KEY         "default_format"
#define GIT_PANEL_FORMAT_DEFAULT     "type name | status | mtime"
#define GIT_PANEL_KEY_STAGE          "hotkey_stage"
#define GIT_PANEL_KEY_UNSTAGE        "hotkey_unstage"
#define GIT_PANEL_KEY_TOGGLE         "hotkey_toggle"
#define GIT_PANEL_KEY_DIFF           "hotkey_diff"
#define GIT_PANEL_KEY_DIFF_ALT       "hotkey_diff_alt"
#define GIT_PANEL_KEY_REFRESH        "hotkey_refresh"
#define GIT_PANEL_KEY_RESET          "hotkey_reset"
#define GIT_PANEL_KEY_STAGE_DEFAULT   "f15"    /* Shift-F5 */
#define GIT_PANEL_KEY_UNSTAGE_DEFAULT "f16"    /* Shift-F6 */
#define GIT_PANEL_KEY_TOGGLE_DEFAULT  "none"
#define GIT_PANEL_KEY_DIFF_DEFAULT    "ctrl-d"
#define GIT_PANEL_KEY_DIFF_ALT_DEFAULT "f13"
#define GIT_PANEL_KEY_REFRESH_DEFAULT "ctrl-r"
#define GIT_PANEL_KEY_RESET_DEFAULT   "f18"    /* Shift-F8 */

/* --------------------------------------------------------------------------------------------- */

typedef enum
{
    GIT_ACTION_TOGGLE = 0,
    GIT_ACTION_STAGE,
    GIT_ACTION_UNSTAGE,
    GIT_ACTION_RESET
} git_action_t;

typedef enum
{
    GIT_VIEW_STATUS = 0,
    GIT_VIEW_BRANCHES,
    GIT_VIEW_BRANCHES_LOCAL,
    GIT_VIEW_BRANCHES_REMOTE_ITEMS,
    GIT_VIEW_COMMITS,
    GIT_VIEW_COMMIT_FILES
} git_view_t;

typedef enum
{
    GIT_ITEM_STATUS_FILE = 0,
    GIT_ITEM_BRANCHES_DIR,
    GIT_ITEM_BRANCHES_LOCAL_DIR,
    GIT_ITEM_REMOTE_DIR_ENTRY,
    GIT_ITEM_BRANCH_ENTRY,
    GIT_ITEM_COMMITS_DIR,
    GIT_ITEM_COMMIT_DIR,
    GIT_ITEM_COMMIT_PARENT_ENTRY,
    GIT_ITEM_COMMIT_FILE
} git_item_kind_t;

#define GIT_COMMITS_DIR_NAME "commits"
#define GIT_BRANCHES_DIR_NAME "branches"
#define GIT_BRANCHES_LOCAL_DIR_NAME "local"
#define GIT_BRANCHES_REMOTE_PREFIX "remote/"

#define GIT_BRANCH_SCOPE_NONE   0
#define GIT_BRANCH_SCOPE_LOCAL  1
#define GIT_BRANCH_SCOPE_REMOTE 2

/* --------------------------------------------------------------------------------------------- */

static void
git_debug_log (const char *fmt, ...)
{
    char *msg;
    va_list ap;

    va_start (ap, fmt);
    msg = g_strdup_vprintf (fmt, ap);
    va_end (ap);

    if (msg == NULL)
        return;

    g_debug ("%s", msg);

    {
        FILE *fp = fopen ("/tmp/mc-git.log", "a");

        if (fp != NULL)
        {
            time_t now = time (NULL);
            struct tm tm_buf;
            struct tm *tm_now = localtime_r (&now, &tm_buf);

            if (tm_now != NULL)
            {
                char ts[32];

                if (strftime (ts, sizeof (ts), "%Y-%m-%d %H:%M:%S", tm_now) > 0)
                    fprintf (fp, "%s %s\n", ts, msg);
                else
                    fprintf (fp, "%s\n", msg);
            }
            else
                fprintf (fp, "%s\n", msg);
            fclose (fp);
        }
    }
    g_free (msg);
}

/* --------------------------------------------------------------------------------------------- */

static const char *
git_view_name (int view)
{
    switch ((git_view_t) view)
    {
    case GIT_VIEW_STATUS:
        return "status";
    case GIT_VIEW_BRANCHES:
        return "branches";
    case GIT_VIEW_BRANCHES_LOCAL:
        return "branches_local";
    case GIT_VIEW_BRANCHES_REMOTE_ITEMS:
        return "branches_remote_items";
    case GIT_VIEW_COMMITS:
        return "commits";
    case GIT_VIEW_COMMIT_FILES:
        return "commit_files";
    default:
        return "unknown";
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
git_set_pending_focus (git_data_t *data, const char *name)
{
    g_free (data->pending_focus);
    data->pending_focus = g_strdup (name);
}

/* --------------------------------------------------------------------------------------------- */

static char *
git_read_config_string_from_file (const char *path, const char *key)
{
    char *value;
    mc_config_t *cfg;

    if (path == NULL || !exist_file (path))
        return NULL;

    cfg = mc_config_init (path, TRUE);
    value = mc_config_get_string (cfg, GIT_PANEL_CONFIG_GROUP, key, NULL);
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
git_save_defaults_to_user_file (const char *path)
{
    mc_config_t *cfg;

    if (path == NULL || exist_file (path))
        return;

    cfg = mc_config_init (path, FALSE);
    mc_config_set_string (cfg, GIT_PANEL_CONFIG_GROUP, GIT_PANEL_FORMAT_KEY, GIT_PANEL_FORMAT_DEFAULT);
    mc_config_set_string (cfg, GIT_PANEL_CONFIG_GROUP, GIT_PANEL_KEY_STAGE, GIT_PANEL_KEY_STAGE_DEFAULT);
    mc_config_set_string (cfg, GIT_PANEL_CONFIG_GROUP, GIT_PANEL_KEY_UNSTAGE,
                          GIT_PANEL_KEY_UNSTAGE_DEFAULT);
    mc_config_set_string (cfg, GIT_PANEL_CONFIG_GROUP, GIT_PANEL_KEY_TOGGLE,
                          GIT_PANEL_KEY_TOGGLE_DEFAULT);
    mc_config_set_string (cfg, GIT_PANEL_CONFIG_GROUP, GIT_PANEL_KEY_DIFF, GIT_PANEL_KEY_DIFF_DEFAULT);
    mc_config_set_string (cfg, GIT_PANEL_CONFIG_GROUP, GIT_PANEL_KEY_DIFF_ALT,
                          GIT_PANEL_KEY_DIFF_ALT_DEFAULT);
    mc_config_set_string (cfg, GIT_PANEL_CONFIG_GROUP, GIT_PANEL_KEY_REFRESH,
                          GIT_PANEL_KEY_REFRESH_DEFAULT);
    mc_config_set_string (cfg, GIT_PANEL_CONFIG_GROUP, GIT_PANEL_KEY_RESET, GIT_PANEL_KEY_RESET_DEFAULT);
    mc_config_save_file (cfg, NULL);
    mc_config_deinit (cfg);
}

/* --------------------------------------------------------------------------------------------- */

static char *
git_load_config_value (const char *key, const char *fallback)
{
    char *value = NULL;
    char *user_path = NULL;
    char *user_path_legacy = NULL;
    char *sys_path = NULL;
    char *sys_path_legacy = NULL;

    user_path = g_build_filename (mc_config_get_path (), GIT_PANEL_CONFIG_FILE, (char *) NULL);
    user_path_legacy =
        g_build_filename (mc_config_get_path (), GIT_PANEL_CONFIG_FILE_LEGACY, (char *) NULL);

    value = git_read_config_string_from_file (user_path, key);
    if (value == NULL)
        value = git_read_config_string_from_file (user_path_legacy, key);

    if (value == NULL && mc_global.sysconfig_dir != NULL)
    {
        sys_path = g_build_filename (mc_global.sysconfig_dir, GIT_PANEL_CONFIG_FILE, (char *) NULL);
        sys_path_legacy =
            g_build_filename (mc_global.sysconfig_dir, GIT_PANEL_CONFIG_FILE_LEGACY, (char *) NULL);

        value = git_read_config_string_from_file (sys_path, key);
        if (value == NULL)
            value = git_read_config_string_from_file (sys_path_legacy, key);
    }

    git_save_defaults_to_user_file (user_path);

    g_free (sys_path_legacy);
    g_free (sys_path);
    g_free (user_path_legacy);
    g_free (user_path);
    return (value != NULL) ? value : g_strdup (fallback);
}

/* --------------------------------------------------------------------------------------------- */

static int
git_parse_hotkey (const char *value, int fallback)
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
    else if (strcmp (s, "shift-f3") == 0)
        ret = KEY_F (13);
    else if (strcmp (s, "insert") == 0)
        ret = KEY_IC;
    else if ((strncmp (s, "shift-f", 7) == 0 || strncmp (s, "s-f", 3) == 0)
             && g_ascii_isdigit ((guchar) s[strncmp (s, "shift-f", 7) == 0 ? 7 : 3]) != 0)
    {
        int base = atoi (s + (strncmp (s, "shift-f", 7) == 0 ? 7 : 3));
        if (base >= 1 && base <= 12)
            ret = KEY_F (base + 10);
    }
    else if (s[0] == 'f' && g_ascii_isdigit ((guchar) s[1]) != 0)
    {
        int n = atoi (s + 1);
        if (n >= 1 && n <= 24)
            ret = KEY_F (n);
    }

    g_free (s);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static int
git_load_hotkey (const char *key, const char *fallback_text, int fallback_key)
{
    char *value;
    int hotkey;

    value = git_load_config_value (key, fallback_text);
    hotkey = git_parse_hotkey (value, fallback_key);
    g_free (value);
    return hotkey;
}

/* --------------------------------------------------------------------------------------------- */

static char *
git_load_default_format (void)
{
    return git_load_config_value (GIT_PANEL_FORMAT_KEY, GIT_PANEL_FORMAT_DEFAULT);
}

static void
git_entry_info_free (gpointer ptr)
{
    git_entry_info_t *info = (git_entry_info_t *) ptr;

    if (info == NULL)
        return;

    g_free (info->repo_path);
    g_free (info->old_repo_path);
    g_free (info->full_path);
    g_free (info->state_text);
    g_free (info->state_mark_text);
    g_free (info->commit_sha);
    g_free (info);
}

/* --------------------------------------------------------------------------------------------- */

static void
git_commit_nav_free (gpointer ptr)
{
    git_commit_nav_t *it = (git_commit_nav_t *) ptr;

    if (it == NULL)
        return;

    g_free (it->commit_sha);
    g_free (it->commit_name);
    g_free (it->commit_list_name);
    g_free (it);
}

/* --------------------------------------------------------------------------------------------- */

static void
git_commit_stack_clear (git_data_t *data)
{
    if (data->commit_stack != NULL)
        g_ptr_array_set_size (data->commit_stack, 0);
}

/* --------------------------------------------------------------------------------------------- */

static void
git_commit_stack_push_current (git_data_t *data)
{
    git_commit_nav_t *it;

    if (data->commit_stack == NULL || data->selected_commit == NULL)
        return;

    it = g_new0 (git_commit_nav_t, 1);
    it->commit_sha = g_strdup (data->selected_commit);
    it->commit_name = g_strdup (data->selected_commit_name);
    it->commit_list_name = g_strdup (data->selected_commit_list_name);
    g_ptr_array_add (data->commit_stack, it);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
git_commit_stack_pop_to_current (git_data_t *data)
{
    git_commit_nav_t *it;

    if (data->commit_stack == NULL || data->commit_stack->len == 0)
        return FALSE;

    it = (git_commit_nav_t *) g_ptr_array_steal_index (data->commit_stack, data->commit_stack->len - 1);
    if (it == NULL)
        return FALSE;

    g_free (data->selected_commit);
    data->selected_commit = g_strdup (it->commit_sha);
    g_free (data->selected_commit_name);
    data->selected_commit_name = g_strdup (it->commit_name);
    g_free (data->selected_commit_list_name);
    data->selected_commit_list_name = g_strdup (it->commit_list_name);

    git_commit_nav_free (it);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
git_run_capture (char **argv, char **out, char **err)
{
    GError *error = NULL;
    int status = 0;
    char *std_out = NULL;
    char *std_err = NULL;
    gboolean ok;

    ok = g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, &std_out, &std_err,
                       &status, &error);
    if (!ok)
    {
        if (err != NULL)
        {
            if (error != NULL && error->message != NULL)
                *err = g_strdup (error->message);
            else
                *err = g_strdup ("Failed to start git command");
        }

        if (out != NULL)
            *out = NULL;

        if (error != NULL)
            g_error_free (error);

        g_free (std_out);
        g_free (std_err);
        return FALSE;
    }

#if GLIB_CHECK_VERSION(2, 70, 0)
    if (!g_spawn_check_wait_status (status, NULL))
#else
    if (!g_spawn_check_exit_status (status, NULL))
#endif
    {
        if (err != NULL)
            *err = g_strdup (std_err != NULL ? std_err : "git command failed");

        if (out != NULL)
            *out = std_out;
        else
            g_free (std_out);

        g_free (std_err);
        return FALSE;
    }

    if (out != NULL)
        *out = std_out;
    else
        g_free (std_out);

    if (err != NULL)
        *err = std_err;
    else
        g_free (std_err);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
git_run_stdout (char **argv, char **out)
{
    return git_run_capture (argv, out, NULL);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
git_run_simple (char **argv, char **err)
{
    return git_run_capture (argv, NULL, err);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
git_make_temp_copy (const char *src_path, char **tmp_path)
{
    GError *error = NULL;
    gchar *content = NULL;
    gsize content_len = 0;
    int fd;
    gboolean ok;

    if (!g_file_get_contents (src_path, &content, &content_len, &error))
    {
        if (error != NULL)
            g_error_free (error);
        return FALSE;
    }

    fd = g_file_open_tmp ("mc-git-edit-XXXXXX", tmp_path, &error);
    if (fd == -1)
    {
        if (error != NULL)
            g_error_free (error);
        g_free (content);
        return FALSE;
    }
    close (fd);

    ok = g_file_set_contents (*tmp_path, content, (gssize) content_len, &error);
    g_free (content);

    if (!ok)
    {
        if (error != NULL)
            g_error_free (error);
        unlink (*tmp_path);
        g_free (*tmp_path);
        *tmp_path = NULL;
        return FALSE;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static char *
git_detect_repo_root (const char *start_path)
{
    char *argv[] = { (char *) "git", (char *) "-C", (char *) start_path, (char *) "rev-parse",
                     (char *) "--show-toplevel", NULL };
    char *out = NULL;
    char *trimmed;

    if (!git_run_stdout (argv, &out))
        return NULL;

    g_strchomp (out);
    if (out[0] == '\0')
    {
        g_free (out);
        return NULL;
    }

    trimmed = g_strdup (out);
    g_free (out);
    return trimmed;
}

/* --------------------------------------------------------------------------------------------- */

static char *
git_detect_upstream_ref (git_data_t *data)
{
    char *argv[] = { (char *) "git", (char *) "-C", data->repo_root, (char *) "rev-parse",
                     (char *) "--abbrev-ref", (char *) "--symbolic-full-name",
                     (char *) "@{upstream}", NULL };
    char *out = NULL;
    char *trimmed;

    if (!git_run_stdout (argv, &out))
        return NULL;

    g_strchomp (out);
    if (out[0] == '\0')
    {
        g_free (out);
        return NULL;
    }

    trimmed = g_strdup (out);
    g_free (out);
    return trimmed;
}

/* --------------------------------------------------------------------------------------------- */

static char *
git_detect_current_branch (git_data_t *data)
{
    char *argv[] = { (char *) "git", (char *) "-C", data->repo_root, (char *) "symbolic-ref",
                     (char *) "--quiet", (char *) "--short", (char *) "HEAD", NULL };
    char *out = NULL;

    if (git_run_stdout (argv, &out))
    {
        g_strchomp (out);
        if (out[0] != '\0')
            return out;
        g_free (out);
    }

    {
        char *argv2[] = { (char *) "git", (char *) "-C", data->repo_root, (char *) "rev-parse",
                          (char *) "--short", (char *) "HEAD", NULL };

        if (git_run_stdout (argv2, &out))
        {
            char *detached;

            g_strchomp (out);
            detached = g_strdup_printf ("detached@%s", out[0] != '\0' ? out : "?");
            g_free (out);
            return detached;
        }
    }

    return g_strdup ("?");
}

/* --------------------------------------------------------------------------------------------- */

static void
git_update_title (git_data_t *data)
{
    g_free (data->title);

    switch ((git_view_t) data->view)
    {
    case GIT_VIEW_STATUS:
        data->title = g_strdup (data->repo_root);
        break;
    case GIT_VIEW_BRANCHES:
        data->title = g_strdup ("/branches");
        break;
    case GIT_VIEW_BRANCHES_LOCAL:
        data->title = g_strdup ("/branches/local");
        break;
    case GIT_VIEW_BRANCHES_REMOTE_ITEMS:
        data->title = g_strdup_printf ("/branches/remote/%s",
                                       data->selected_remote != NULL ? data->selected_remote : "");
        break;
    case GIT_VIEW_COMMITS:
        if (data->selected_branch != NULL)
            data->title = g_strdup_printf ("/commits/%s", data->selected_branch);
        else
            data->title = g_strdup ("/commits");
        break;
    case GIT_VIEW_COMMIT_FILES:
        if (data->selected_branch != NULL)
            data->title = g_strdup_printf ("/commits/%s/%s", data->selected_branch,
                                           data->selected_commit_name != NULL
                                               ? data->selected_commit_name
                                               : (data->selected_commit != NULL ? data->selected_commit : ""));
        else
            data->title = g_strdup_printf ("/commits/%s",
                                           data->selected_commit_name != NULL
                                               ? data->selected_commit_name
                                               : (data->selected_commit != NULL ? data->selected_commit : ""));
        break;
    default:
        data->title = g_strdup (data->repo_root);
        break;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
git_add_virtual_dir (git_data_t *data, dir_list *list, const char *name, git_item_kind_t kind,
                     const char *commit_sha, const char *status_text, time_t mtime)
{
    struct stat st;
    git_entry_info_t *info;

    memset (&st, 0, sizeof (st));
    st.st_mode = S_IFDIR | 0755;
    st.st_size = 0;
    st.st_mtime = mtime;
    st.st_uid = getuid ();
    st.st_gid = getgid ();
    st.st_nlink = 1;

    (void) dir_list_append (list, g_strdup (name), &st, TRUE, FALSE);

    info = g_new0 (git_entry_info_t, 1);
    info->kind = kind;
    info->state_mark_text = g_strdup ("/");
    info->state_text = g_strdup (status_text);
    info->commit_sha = g_strdup (commit_sha);
    g_hash_table_insert (data->display_to_info, g_strdup (name), info);
}

/* --------------------------------------------------------------------------------------------- */

static void
git_add_branch_entry (git_data_t *data, dir_list *list, const char *display_name,
                      const char *branch_ref, const char *status_text, time_t mtime)
{
    struct stat st;
    git_entry_info_t *info;

    memset (&st, 0, sizeof (st));
    st.st_mode = S_IFDIR | 0755;
    st.st_size = 0;
    st.st_mtime = mtime;
    st.st_uid = getuid ();
    st.st_gid = getgid ();
    st.st_nlink = 1;

    (void) dir_list_append (list, g_strdup (display_name), &st, TRUE, FALSE);

    info = g_new0 (git_entry_info_t, 1);
    info->kind = GIT_ITEM_BRANCH_ENTRY;
    info->repo_path = g_strdup (branch_ref != NULL ? branch_ref : display_name);
    info->state_mark_text = g_strdup ("/");
    info->state_text = g_strdup (status_text);
    g_hash_table_insert (data->display_to_info, g_strdup (display_name), info);
}

/* --------------------------------------------------------------------------------------------- */

static void
git_add_entry (git_data_t *data, dir_list *list, char state_mark, const char *state_prefix,
               const char *repo_path, gboolean is_deleted, gboolean is_staged,
               git_item_kind_t kind, const char *commit_sha, const char *old_repo_path)
{
    struct stat st;
    git_entry_info_t *info;
    char *name;

    info = g_new0 (git_entry_info_t, 1);
    info->kind = kind;
    info->repo_path = g_strdup (repo_path);
    info->old_repo_path = g_strdup (old_repo_path);
    info->full_path = g_build_filename (data->repo_root, repo_path, NULL);
    info->state_text = g_strdup (state_prefix);
    info->state_mark_text = g_strdup_printf ("%c", state_mark);
    info->commit_sha = g_strdup (commit_sha);
    info->is_deleted = is_deleted;
    info->is_staged = is_staged;

    name = g_strdup (repo_path);

    if (stat (info->full_path, &st) == -1)
    {
        memset (&st, 0, sizeof (st));
        st.st_mode = S_IFREG | 0644;
        st.st_size = 0;
        st.st_mtime = time (NULL);
        st.st_uid = getuid ();
        st.st_gid = getgid ();
        st.st_nlink = 1;
    }

    (void) dir_list_append (list, name, &st, S_ISDIR (st.st_mode), FALSE);

    g_hash_table_insert (data->display_to_info, g_strdup (name), info);

    g_free (name);
}

/* --------------------------------------------------------------------------------------------- */

static void
git_parse_branches_and_fill (git_data_t *data, dir_list *list, const char *out,
                             const char *remote_prefix)
{
    char **lines;
    int i;
    gboolean is_remote = (remote_prefix != NULL);

    lines = g_strsplit (out, "\n", -1);
    for (i = 0; lines[i] != NULL; i++)
    {
        char **parts;
        const char *branch_name;
        const char *display_name;
        const char *short_hash;
        const char *head_mark;
        char *status;
        time_t commit_time;

        if (lines[i][0] == '\0')
            continue;

        parts = g_strsplit (lines[i], "\t", 4);
        if (parts[0] == NULL || parts[1] == NULL || parts[2] == NULL)
        {
            g_strfreev (parts);
            continue;
        }

        branch_name = parts[0];
        display_name = branch_name;
        if (is_remote)
        {
            size_t prefix_len;

            prefix_len = strlen (remote_prefix);
            if (!g_str_has_prefix (branch_name, remote_prefix)
                || branch_name[prefix_len] != '/')
            {
                g_strfreev (parts);
                continue;
            }

            display_name = branch_name + prefix_len + 1;
            if (strcmp (display_name, "HEAD") == 0)
            {
                g_strfreev (parts);
                continue;
            }
        }

        short_hash = parts[1];
        commit_time = (time_t) g_ascii_strtoll (parts[2], NULL, 10);
        head_mark = parts[3] != NULL ? g_strstrip (parts[3]) : "";

        if (!is_remote && strcmp (head_mark, "*") == 0)
            status = g_strdup_printf ("* %s", short_hash);
        else
            status = g_strdup (short_hash);

        git_add_branch_entry (data, list, display_name, branch_name, status, commit_time);
        g_free (status);
        g_strfreev (parts);
    }

    g_strfreev (lines);
}

/* --------------------------------------------------------------------------------------------- */

static void
git_parse_remotes_and_fill (git_data_t *data, dir_list *list, const char *out)
{
    char **lines;
    int i;

    lines = g_strsplit (out, "\n", -1);
    for (i = 0; lines[i] != NULL; i++)
    {
        const char *remote_name = g_strstrip (lines[i]);
        char *name;

        if (remote_name == NULL || *remote_name == '\0')
            continue;

        name = g_strdup_printf ("remote/%s", remote_name);
        git_add_virtual_dir (data, list, name, GIT_ITEM_REMOTE_DIR_ENTRY, NULL, "REMOTE",
                             time (NULL));
        g_free (name);
    }

    g_strfreev (lines);
}

/* --------------------------------------------------------------------------------------------- */

static const char *
git_stage_status_from_status (const char *status)
{
    const char x = status[0];

    if (x == '?' && status[1] == '?')
        return "UNTRACKED";

    if (x != ' ')
        return "STAGED";

    return "UNSTAGED";
}

/* --------------------------------------------------------------------------------------------- */

static char
git_type_mark_from_status (const char *status)
{
    const char x = status[0];
    const char y = status[1];
    char t;

    if (x == '?' && y == '?')
        return '+';

    /* Type reflects what changed with file, independent from staged/unstaged bucket. */
    t = (y != ' ') ? y : x;

    if (t == 'R')
        return '=';

    if (t == 'D')
        return '-';

    if (t == 'A')
        return '+';

    return '*';
}

/* --------------------------------------------------------------------------------------------- */

static void
git_parse_status_and_fill (git_data_t *data, dir_list *list, const char *out)
{
    char **lines;
    int i;

    lines = g_strsplit (out, "\n", -1);
    for (i = 0; lines[i] != NULL; i++)
    {
        const char *line = lines[i];
        const char *status;
        const char *stage_status;
        char state_mark;
        const char *path;
        const char *arrow;
        gboolean is_deleted;
        gboolean is_staged;

        if (line[0] == '\0')
            continue;

        if (line[0] == '?' && line[1] == '?')
        {
            status = "??";
            path = line + 3;
        }
        else
        {
            static char st_code[3];

            if (strlen (line) < 4)
                continue;

            st_code[0] = line[0];
            st_code[1] = line[1];
            st_code[2] = '\0';
            status = st_code;
            path = line + 3;
        }

        if (path[0] == '\0')
            continue;

        arrow = strstr (path, " -> ");
        if (arrow != NULL)
            path = arrow + 4;

        stage_status = git_stage_status_from_status (status);
        state_mark = git_type_mark_from_status (status);
        is_deleted = (status[0] == 'D' || status[1] == 'D');
        is_staged = (status[0] != ' ' && !(status[0] == '?' && status[1] == '?'));
        git_add_entry (data, list, state_mark, stage_status, path, is_deleted, is_staged,
                       GIT_ITEM_STATUS_FILE, NULL, NULL);
    }

    g_strfreev (lines);
}

/* --------------------------------------------------------------------------------------------- */

static void
git_parse_log_and_fill (git_data_t *data, dir_list *list, const char *out)
{
    char **lines;
    int i;

    lines = g_strsplit (out, "\n", -1);
    for (i = 0; lines[i] != NULL; i++)
    {
        char **parts;
        char *name;
        time_t commit_time;

        if (lines[i][0] == '\0')
            continue;

        parts = g_strsplit (lines[i], "\t", 4);
        if (parts[0] == NULL || parts[1] == NULL || parts[2] == NULL)
        {
            g_strfreev (parts);
            continue;
        }

        commit_time = (time_t) g_ascii_strtoll (parts[2], NULL, 10);
        name = g_strdup (parts[3] != NULL ? parts[3] : "");
        git_add_virtual_dir (data, list, name, GIT_ITEM_COMMIT_DIR, parts[0], parts[1], commit_time);
        g_free (name);
        g_strfreev (parts);
    }

    g_strfreev (lines);
}

/* --------------------------------------------------------------------------------------------- */

static char
git_type_mark_from_name_status (const char *status_token)
{
    char c;

    if (status_token == NULL || status_token[0] == '\0')
        return '*';

    c = status_token[0];
    if (c == 'A')
        return '+';
    if (c == 'D')
        return '-';
    if (c == 'R')
        return '=';
    return '*';
}

/* --------------------------------------------------------------------------------------------- */

static int
git_parse_commit_files_and_fill (git_data_t *data, dir_list *list, const char *out)
{
    char **lines;
    int i;
    int added = 0;

    lines = g_strsplit (out, "\n", -1);
    for (i = 0; lines[i] != NULL; i++)
    {
        char **parts;
        const char *status;
        const char *path;
        const char *old_path = NULL;
        gboolean is_deleted;

        if (lines[i][0] == '\0')
            continue;

        parts = g_strsplit (lines[i], "\t", 4);
        if (parts[0] == NULL || parts[1] == NULL)
        {
            g_strfreev (parts);
            continue;
        }

        status = parts[0];
        path = parts[1];
        if (status[0] == 'R' && parts[2] != NULL)
        {
            old_path = parts[1];
            path = parts[2];
        }

        is_deleted = (status[0] == 'D');
        git_add_entry (data, list, git_type_mark_from_name_status (status), "COMMIT", path,
                       is_deleted, FALSE, GIT_ITEM_COMMIT_FILE, data->selected_commit, old_path);
        added++;

        g_strfreev (parts);
    }

    g_strfreev (lines);
    return added;
}

/* --------------------------------------------------------------------------------------------- */

static void
git_add_commit_parent_entry (git_data_t *data, dir_list *list, const char *parent_sha)
{
    struct stat st;
    git_entry_info_t *info;
    char short_sha[10];

    if (parent_sha == NULL || *parent_sha == '\0')
        return;

    memset (&st, 0, sizeof (st));
    st.st_mode = S_IFDIR | 0755;
    st.st_size = 0;
    st.st_mtime = time (NULL);
    st.st_uid = getuid ();
    st.st_gid = getgid ();
    st.st_nlink = 1;

    g_strlcpy (short_sha, parent_sha, sizeof (short_sha));
    (void) dir_list_append (list, g_strdup (short_sha), &st, TRUE, FALSE);

    info = g_new0 (git_entry_info_t, 1);
    info->kind = GIT_ITEM_COMMIT_PARENT_ENTRY;
    info->commit_sha = g_strdup (parent_sha);
    info->state_mark_text = g_strdup ("/");
    info->state_text = g_strdup ("PARENT");
    g_hash_table_insert (data->display_to_info, g_strdup (short_sha), info);
}

/* --------------------------------------------------------------------------------------------- */

static int
git_parse_commit_parents_and_fill (git_data_t *data, dir_list *list, const char *commit_sha)
{
    char *argv[] = { (char *) "git", (char *) "-C", data->repo_root, (char *) "rev-list",
                     (char *) "--parents", (char *) "-n", (char *) "1", (char *) commit_sha, NULL };
    char *out = NULL;
    char **parts;
    int i;
    int added = 0;

    if (!git_run_stdout (argv, &out))
        return 0;

    g_strchomp (out);
    if (out[0] == '\0')
    {
        g_free (out);
        return 0;
    }

    parts = g_strsplit (out, " ", -1);
    for (i = 1; parts[i] != NULL; i++)
    {
        if (parts[i][0] == '\0')
            continue;
        git_add_commit_parent_entry (data, list, parts[i]);
        added++;
    }

    g_strfreev (parts);
    g_free (out);
    return added;
}

/* --------------------------------------------------------------------------------------------- */

static void
git_show_error (git_data_t *data, const char *title, const char *text)
{
    if (data->host != NULL && data->host->message != NULL)
        data->host->message (data->host, 0, title, text);
}

/* --------------------------------------------------------------------------------------------- */

static const git_entry_info_t *
git_lookup_info (const git_data_t *data, const char *name)
{
    const git_entry_info_t *info;

    if (data == NULL || name == NULL || *name == '\0')
        return NULL;

    info = (const git_entry_info_t *) g_hash_table_lookup (data->display_to_info, name);
    if (info != NULL)
        return info;

    if (name[0] == '/')
        return (const git_entry_info_t *) g_hash_table_lookup (data->display_to_info, name + 1);

    {
        char *slash_name = g_strconcat ("/", name, NULL);
        info = (const git_entry_info_t *) g_hash_table_lookup (data->display_to_info, slash_name);
        g_free (slash_name);
    }

    return info;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
git_apply_one (git_data_t *data, const char *display_name, git_action_t action)
{
    const git_entry_info_t *info;
    char *err = NULL;
    gboolean ok;

    info = git_lookup_info (data, display_name);
    if (info == NULL || info->repo_path == NULL)
        return FALSE;

    git_debug_log ("git: action=%d file=%s staged=%d status=%s", (int) action, info->repo_path,
                   info->is_staged ? 1 : 0, info->state_text != NULL ? info->state_text : "?");

    if (action == GIT_ACTION_STAGE && info->is_staged)
        return TRUE;

    if (action == GIT_ACTION_UNSTAGE && !info->is_staged)
        return TRUE;

    if (action == GIT_ACTION_RESET)
    {
        if (info->state_text != NULL && strcmp (info->state_text, "UNTRACKED") == 0)
        {
            char *argv[] = { (char *) "git",   (char *) "-C", data->repo_root, (char *) "clean",
                             (char *) "-f",    (char *) "--", (char *) info->repo_path, NULL };

            git_debug_log ("git: run clean -f -- %s", info->repo_path);
            ok = git_run_simple (argv, &err);
        }
        else
        {
            char *argv[] = { (char *) "git",      (char *) "-C",       data->repo_root,
                             (char *) "restore",  (char *) "--source=HEAD",
                             (char *) "--staged", (char *) "--worktree",
                             (char *) "--",       (char *) info->repo_path, NULL };

            git_debug_log ("git: run restore --source=HEAD --staged --worktree -- %s", info->repo_path);
            ok = git_run_simple (argv, &err);
        }
    }
    else if (action == GIT_ACTION_UNSTAGE || (action == GIT_ACTION_TOGGLE && info->is_staged))
    {
        char *argv[] = { (char *) "git",     (char *) "-C",       data->repo_root,
                         (char *) "restore", (char *) "--staged", (char *) "--",
                         (char *) info->repo_path, NULL };

        git_debug_log ("git: run restore --staged -- %s", info->repo_path);
        ok = git_run_simple (argv, &err);
    }
    else
    {
        char *argv[] = { (char *) "git", (char *) "-C", data->repo_root, (char *) "add",
                         (char *) "-A",  (char *) "--", (char *) info->repo_path, NULL };

        git_debug_log ("git: run add -A -- %s", info->repo_path);
        ok = git_run_simple (argv, &err);
    }

    if (!ok)
    {
        git_debug_log ("git: command failed: %s", err != NULL ? err : "(empty)");
        if (err == NULL || err[0] == '\0')
            git_show_error (data, "Git panel", "Git command failed.");
        else
            git_show_error (data, "Git panel", err);

        g_free (err);
        return FALSE;
    }

    g_free (err);
    git_debug_log ("git: command success");
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
git_apply_selected (git_data_t *data, git_action_t action)
{
    int marked_count;
    int current = 0;
    const GString *name = NULL;
    gboolean any_processed = FALSE;
    gboolean had_errors = FALSE;

    marked_count = (data->host != NULL && data->host->get_marked_count != NULL)
        ? data->host->get_marked_count (data->host)
        : 0;

    if (data->host == NULL)
        return FALSE;

    if (action == GIT_ACTION_RESET
        && query_dialog (_ ("Git panel"),
                         _ ("This operation is irreversible.\n"
                            "Selected files will be reset to HEAD.\n"
                            "Untracked files will be deleted.\n\n"
                            "Do you want to continue?"),
                         D_NORMAL, 2, _ ("&Reset"), _ ("&Cancel")) != 0)
    {
        git_debug_log ("git: reset canceled by user");
        return FALSE;
    }

    if (marked_count > 0 && data->host->get_next_marked != NULL)
    {
        while ((name = data->host->get_next_marked (data->host, &current)) != NULL)
        {
            if (name->str == NULL || strcmp (name->str, "..") == 0)
                continue;

            any_processed = TRUE;
            git_debug_log ("git: selected(marked)='%s' action=%d", name->str, (int) action);
            if (!git_apply_one (data, name->str, action))
                had_errors = TRUE;

            current++;
        }

        if (!any_processed)
        {
            git_show_error (data, "Git panel", "No marked files selected for operation.");
            git_debug_log ("git: no marked files for action=%d", (int) action);
            return FALSE;
        }

        if (had_errors)
            git_show_error (data, "Git panel",
                            "Some marked files failed to process. Check mc log.");
        return !had_errors;
    }

    if (data->host->get_current != NULL)
        name = data->host->get_current (data->host);
    if (name == NULL || name->str == NULL || strcmp (name->str, "..") == 0)
    {
        git_show_error (data, "Git panel", "No file selected for operation.");
        git_debug_log ("git: no selected file for action=%d", (int) action);
        return FALSE;
    }

    git_debug_log ("git: selected(current)='%s' action=%d", name->str, (int) action);
    return git_apply_one (data, name->str, action);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
git_write_temp_contents (const char *contents, gssize len, char **tmp_path)
{
    GError *error = NULL;
    int fd;
    gboolean ok;

    fd = g_file_open_tmp ("mc-git-diff-XXXXXX", tmp_path, &error);
    if (fd == -1)
    {
        if (error != NULL)
            g_error_free (error);
        return FALSE;
    }
    close (fd);

    ok = g_file_set_contents (*tmp_path, contents, len, &error);
    if (!ok)
    {
        if (error != NULL)
            g_error_free (error);
        unlink (*tmp_path);
        g_free (*tmp_path);
        *tmp_path = NULL;
        return FALSE;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
git_write_temp_from_git_show (git_data_t *data, const char *object_spec, char **tmp_path)
{
    char *out = NULL;
    char *err = NULL;
    gboolean ok;
    char *argv[] = { (char *) "git",  (char *) "-C", data->repo_root, (char *) "show",
                     (char *) object_spec, NULL };

    ok = git_run_capture (argv, &out, &err);
    if (!ok)
    {
        g_free (out);
        g_free (err);
        return FALSE;
    }

    ok = git_write_temp_contents (out, (gssize) strlen (out), tmp_path);
    g_free (out);
    g_free (err);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
git_show_diff_selected (git_data_t *data)
{
    const GString *name = NULL;
    const git_entry_info_t *info;
    int current = 0;
    char *left_path = NULL;
    char *right_path = NULL;
    gboolean left_is_temp = FALSE;
    gboolean right_is_temp = FALSE;
    gboolean ok = FALSE;

    if (data->host == NULL || data->host->open_diff == NULL)
        return FALSE;

    if (data->host->get_next_marked != NULL)
        name = data->host->get_next_marked (data->host, &current);
    if ((name == NULL || name->str == NULL || strcmp (name->str, "..") == 0)
        && data->host->get_current != NULL)
        name = data->host->get_current (data->host);
    if (name == NULL || name->str == NULL || strcmp (name->str, "..") == 0)
    {
        git_show_error (data, "Git panel", "No file selected for diff.");
        return FALSE;
    }

    info = git_lookup_info (data, name->str);
    if (info == NULL || info->repo_path == NULL)
    {
        git_show_error (data, "Git panel", "Cannot resolve selected file for diff.");
        return FALSE;
    }

    if (info->kind == GIT_ITEM_COMMIT_FILE)
    {
        char *object_old = NULL;
        char *object_new = NULL;
        const char *old_path = (info->old_repo_path != NULL) ? info->old_repo_path : info->repo_path;
        const gboolean is_added = (info->state_mark_text != NULL && info->state_mark_text[0] == '+');

        if (is_added)
        {
            if (!git_write_temp_contents ("", 0, &left_path))
                goto fail;
            left_is_temp = TRUE;
        }
        else
        {
            object_old = g_strdup_printf ("%s^:%s", info->commit_sha, old_path);
            ok = git_write_temp_from_git_show (data, object_old, &left_path);
            if (!ok && !git_write_temp_contents ("", 0, &left_path))
            {
                g_free (object_old);
                goto fail;
            }
            left_is_temp = TRUE;
            g_free (object_old);
        }

        if (info->is_deleted)
        {
            if (!git_write_temp_contents ("", 0, &right_path))
                goto fail;
            right_is_temp = TRUE;
        }
        else
        {
            object_new = g_strdup_printf ("%s:%s", info->commit_sha, info->repo_path);
            ok = git_write_temp_from_git_show (data, object_new, &right_path);
            g_free (object_new);
            if (!ok)
                goto fail;
            right_is_temp = TRUE;
        }
    }
    else if (info->is_deleted)
    {
        char *head_spec;

        head_spec = g_strdup_printf ("HEAD:%s", info->repo_path);
        ok = git_write_temp_from_git_show (data, head_spec, &left_path);
        g_free (head_spec);
        if (!ok)
            goto fail;
        left_is_temp = TRUE;

        if (!git_write_temp_contents ("", 0, &right_path))
            goto fail;
        right_is_temp = TRUE;
    }
    else
    {
        char *head_spec;

        if (info->full_path == NULL || access (info->full_path, R_OK) != 0)
            goto fail;
        right_path = g_strdup (info->full_path);

        head_spec = g_strdup_printf ("HEAD:%s", info->repo_path);
        ok = git_write_temp_from_git_show (data, head_spec, &left_path);
        g_free (head_spec);
        if (!ok)
        {
            if (!git_write_temp_contents ("", 0, &left_path))
                goto fail;
        }
        left_is_temp = TRUE;
    }

    if (!data->host->open_diff (data->host, left_path, right_path))
    {
        git_show_error (data, "Git panel", "Cannot open internal diff viewer.");
        goto fail;
    }

    if (left_is_temp)
        unlink (left_path);
    if (right_is_temp)
        unlink (right_path);
    g_free (left_path);
    g_free (right_path);
    return TRUE;

fail:
    if (left_path != NULL)
    {
        if (left_is_temp)
            unlink (left_path);
        g_free (left_path);
    }
    if (right_path != NULL)
    {
        if (right_is_temp)
            unlink (right_path);
        g_free (right_path);
    }
    git_show_error (data, "Git panel",
                    "Cannot prepare git diff view. Check file state and repository history.");
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
git_show_commit_description_by_sha (git_data_t *data, const char *sha, const char *name_for_log)
{
    char *out = NULL;
    char *err = NULL;
    char *tmp_path = NULL;
    gboolean ok;
    char *argv[] = { (char *) "git", (char *) "-C", data->repo_root, (char *) "show",
                     (char *) "--no-color", (char *) "-s", (char *) "--format=fuller", NULL, NULL };

    if (sha == NULL || *sha == '\0')
    {
        git_debug_log ("git: F3 empty sha name='%s'", name_for_log != NULL ? name_for_log : "(null)");
        return FALSE;
    }

    git_debug_log ("git: F3 commit description name='%s' sha=%s view=%s",
                   name_for_log != NULL ? name_for_log : "(null)", sha, git_view_name (data->view));

    argv[7] = (char *) sha;
    ok = git_run_capture (argv, &out, &err);
    if (!ok)
    {
        if (err != NULL && *err != '\0')
            git_show_error (data, "Git panel", err);
        g_free (out);
        g_free (err);
        return FALSE;
    }

    if (!git_write_temp_contents (out != NULL ? out : "", out != NULL ? (gssize) strlen (out) : 0, &tmp_path))
    {
        git_debug_log ("git: F3 failed to write temp file for sha=%s", sha);
        g_free (out);
        g_free (err);
        return FALSE;
    }
    git_debug_log ("git: F3 temp file=%s", tmp_path);

    {
        vfs_path_t *tmp_vpath;

        tmp_vpath = vfs_path_from_str (tmp_path);
        (void) mcview_viewer (NULL, tmp_vpath, 0, 0, 0);
        vfs_path_free (tmp_vpath, TRUE);
    }

    unlink (tmp_path);
    g_free (tmp_path);
    g_free (out);
    g_free (err);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/* Plugin callbacks */
/* --------------------------------------------------------------------------------------------- */

static void *
git_open (mc_panel_host_t *host, const char *open_path)
{
    git_data_t *data;
    char *repo_root;
    const char *start_path;

    start_path = (open_path != NULL && *open_path != '\0') ? open_path : ".";
    repo_root = git_detect_repo_root (start_path);
    if (repo_root == NULL)
    {
        if (host != NULL && host->message != NULL)
            host->message (host, 0, "Git panel", "Current path is not a Git repository.");
        return NULL;
    }

    data = g_new0 (git_data_t, 1);
    data->host = host;
    data->repo_root = repo_root;
    data->title = g_strdup (repo_root);
    data->help_filename = g_build_filename (mc_global.share_data_dir, "help", "git-panel.hlp", (char *) NULL);
    data->display_to_info =
        g_hash_table_new_full (g_str_hash, g_str_equal, g_free, git_entry_info_free);
    data->default_format = git_load_default_format ();
    data->key_stage = git_load_hotkey (GIT_PANEL_KEY_STAGE, GIT_PANEL_KEY_STAGE_DEFAULT, KEY_F (15));
    data->key_unstage =
        git_load_hotkey (GIT_PANEL_KEY_UNSTAGE, GIT_PANEL_KEY_UNSTAGE_DEFAULT, KEY_F (16));
    data->key_toggle = git_load_hotkey (GIT_PANEL_KEY_TOGGLE, GIT_PANEL_KEY_TOGGLE_DEFAULT, -1);
    data->key_diff = git_load_hotkey (GIT_PANEL_KEY_DIFF, GIT_PANEL_KEY_DIFF_DEFAULT, XCTRL ('d'));
    data->key_diff_alt =
        git_load_hotkey (GIT_PANEL_KEY_DIFF_ALT, GIT_PANEL_KEY_DIFF_ALT_DEFAULT, KEY_F (13));
    data->key_refresh =
        git_load_hotkey (GIT_PANEL_KEY_REFRESH, GIT_PANEL_KEY_REFRESH_DEFAULT, XCTRL ('r'));
    data->key_reset = git_load_hotkey (GIT_PANEL_KEY_RESET, GIT_PANEL_KEY_RESET_DEFAULT, KEY_F (18));
    data->view = GIT_VIEW_STATUS;
    data->selected_commit = NULL;
    data->current_branch = git_detect_current_branch (data);
    data->selected_branch = NULL;
    data->selected_remote = NULL;
    data->selected_commit_name = NULL;
    data->selected_commit_list_name = NULL;
    data->selected_branch_scope = GIT_BRANCH_SCOPE_NONE;
    data->pending_focus = NULL;
    data->commit_stack = g_ptr_array_new_with_free_func (git_commit_nav_free);
    git_update_title (data);

    git_debug_log ("git: keys stage=%d unstage=%d toggle=%d diff=%d diff_alt=%d refresh=%d reset=%d",
                   data->key_stage, data->key_unstage, data->key_toggle, data->key_diff,
                   data->key_diff_alt, data->key_refresh, data->key_reset);

    return data;
}

/* --------------------------------------------------------------------------------------------- */

static void
git_close (void *plugin_data)
{
    git_data_t *data = (git_data_t *) plugin_data;

    g_hash_table_destroy (data->display_to_info);
    g_free (data->title);
    g_free (data->help_filename);
    g_free (data->repo_root);
    g_free (data->default_format);
    g_free (data->selected_commit);
    g_free (data->current_branch);
    g_free (data->selected_branch);
    g_free (data->selected_remote);
    g_free (data->selected_commit_name);
    g_free (data->selected_commit_list_name);
    g_free (data->pending_focus);
    g_ptr_array_free (data->commit_stack, TRUE);
    g_free (data);
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
git_get_items (void *plugin_data, void *list_ptr)
{
    git_data_t *data = (git_data_t *) plugin_data;
    dir_list *list = (dir_list *) list_ptr;
    char *out = NULL;
    gboolean ok = FALSE;

    g_hash_table_remove_all (data->display_to_info);

    if ((git_view_t) data->view == GIT_VIEW_STATUS)
    {
        char *argv[] = { (char *) "git",   (char *) "-C",     data->repo_root,
                         (char *) "status", (char *) "--porcelain=v1", NULL };

        g_free (data->current_branch);
        data->current_branch = git_detect_current_branch (data);
        git_update_title (data);
        git_add_virtual_dir (data, list, GIT_BRANCHES_DIR_NAME, GIT_ITEM_BRANCHES_DIR, NULL, "",
                             time (NULL));
        git_add_virtual_dir (data, list, GIT_COMMITS_DIR_NAME, GIT_ITEM_COMMITS_DIR, NULL, "", time (NULL));
        ok = git_run_stdout (argv, &out);
        if (!ok)
            return MC_PPR_FAILED;
        git_parse_status_and_fill (data, list, out);
    }
    else if ((git_view_t) data->view == GIT_VIEW_BRANCHES)
    {
        git_add_virtual_dir (data, list, GIT_BRANCHES_LOCAL_DIR_NAME, GIT_ITEM_BRANCHES_LOCAL_DIR,
                             NULL, "", time (NULL));

        {
            char *argv[] = { (char *) "git", (char *) "-C", data->repo_root, (char *) "remote", NULL };

            ok = git_run_stdout (argv, &out);
            if (!ok)
                return MC_PPR_FAILED;
            git_parse_remotes_and_fill (data, list, out);
        }
    }
    else if ((git_view_t) data->view == GIT_VIEW_BRANCHES_LOCAL)
    {
        char *argv[] = { (char *) "git", (char *) "-C", data->repo_root, (char *) "for-each-ref",
                         (char *) "--sort=-committerdate",
                         (char *) "--format=%(refname:short)\t%(objectname:short)\t%(committerdate:unix)\t%(HEAD)",
                         (char *) "refs/heads", NULL };

        ok = git_run_stdout (argv, &out);
        if (!ok)
            return MC_PPR_FAILED;
        git_parse_branches_and_fill (data, list, out, NULL);
    }
    else if ((git_view_t) data->view == GIT_VIEW_BRANCHES_REMOTE_ITEMS
             && data->selected_remote != NULL)
    {
        char *remote_ref = NULL;
        char *argv[] = { (char *) "git", (char *) "-C", data->repo_root, (char *) "for-each-ref",
                         (char *) "--sort=-committerdate",
                         (char *) "--format=%(refname:short)\t%(objectname:short)\t%(committerdate:unix)",
                         NULL, NULL };

        remote_ref = g_strdup_printf ("refs/remotes/%s", data->selected_remote);
        argv[6] = remote_ref;

        ok = git_run_stdout (argv, &out);
        g_free (remote_ref);
        if (!ok)
            return MC_PPR_FAILED;
        git_parse_branches_and_fill (data, list, out, data->selected_remote);
    }
    else if ((git_view_t) data->view == GIT_VIEW_COMMITS)
    {
        char *upstream_ref;
        char *range = NULL;
        char *out2 = NULL;
        gboolean ok2;

        if (data->selected_branch != NULL)
        {
            char *argv[] = { (char *) "git", (char *) "-C", data->repo_root, (char *) "log",
                             (char *) "--no-color", (char *) "--decorate=no",
                             (char *) "--first-parent",
                             (char *) "--pretty=format:%H%x09%h%x09%ct%x09%s",
                             (char *) "-n", (char *) "200", data->selected_branch, NULL };
            ok2 = git_run_stdout (argv, &out2);
        }
        else
        {
            upstream_ref = git_detect_upstream_ref (data);
            if (upstream_ref != NULL)
            {
                char *argv[] = { (char *) "git", (char *) "-C", data->repo_root, (char *) "log",
                                 (char *) "--no-color", (char *) "--decorate=no",
                                 (char *) "--first-parent",
                                 (char *) "--pretty=format:%H%x09%h%x09%ct%x09%s",
                                 (char *) "-n", (char *) "200", NULL, NULL };

                range = g_strdup_printf ("%s..HEAD", upstream_ref);
                argv[10] = range;
                ok2 = git_run_stdout (argv, &out2);
            }
            else
            {
                char *argv[] = { (char *) "git", (char *) "-C", data->repo_root, (char *) "log",
                                 (char *) "--no-color", (char *) "--decorate=no",
                                 (char *) "--first-parent",
                                 (char *) "--pretty=format:%H%x09%h%x09%ct%x09%s",
                                 (char *) "-n", (char *) "200", (char *) "HEAD", NULL };

                ok2 = git_run_stdout (argv, &out2);
            }
            g_free (range);
            g_free (upstream_ref);
        }

        if (!ok2)
            return MC_PPR_FAILED;

        git_parse_log_and_fill (data, list, out2);
        g_free (out2);
        return MC_PPR_OK;
    }
    else if ((git_view_t) data->view == GIT_VIEW_COMMIT_FILES && data->selected_commit != NULL)
    {
        int files_added;
        char *argv[] = { (char *) "git",      (char *) "-C", data->repo_root,
                         (char *) "show",     (char *) "--name-status",
                         (char *) "--format=", (char *) "--find-renames",
                         data->selected_commit, NULL };

        ok = git_run_stdout (argv, &out);
        if (!ok)
            return MC_PPR_FAILED;
        files_added = git_parse_commit_files_and_fill (data, list, out);
        if (files_added == 0)
            (void) git_parse_commit_parents_and_fill (data, list, data->selected_commit);
    }
    else
        return MC_PPR_FAILED;

    g_free (out);
    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
git_get_local_copy (void *plugin_data, const char *fname, char **local_path)
{
    git_data_t *data = (git_data_t *) plugin_data;
    const git_entry_info_t *info;

    if (fname == NULL || local_path == NULL)
        return MC_PPR_FAILED;

    info = git_lookup_info (data, fname);
    if (info == NULL)
        return MC_PPR_FAILED;

    if (info->kind == GIT_ITEM_STATUS_FILE)
    {
        if (info->full_path == NULL || info->is_deleted || access (info->full_path, R_OK) != 0)
            return MC_PPR_FAILED;

        if (!git_make_temp_copy (info->full_path, local_path))
            return MC_PPR_FAILED;
    }
    else if (info->kind == GIT_ITEM_COMMIT_FILE)
    {
        char *object_spec;
        gboolean ok;

        if (info->is_deleted || info->commit_sha == NULL || info->repo_path == NULL)
            return MC_PPR_FAILED;

        object_spec = g_strdup_printf ("%s:%s", info->commit_sha, info->repo_path);
        ok = git_write_temp_from_git_show (data, object_spec, local_path);
        g_free (object_spec);
        if (!ok)
            return MC_PPR_FAILED;
    }
    else
        return MC_PPR_FAILED;

    return (*local_path != NULL) ? MC_PPR_OK : MC_PPR_FAILED;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
git_chdir (void *plugin_data, const char *path)
{
    git_data_t *data = (git_data_t *) plugin_data;
    const git_entry_info_t *info;

    if (data == NULL || path == NULL)
        return MC_PPR_FAILED;

    git_set_pending_focus (data, NULL);
    git_debug_log ("git: chdir path='%s' view=%s", path, git_view_name (data->view));

    if (strcmp (path, "..") == 0)
    {
        if ((git_view_t) data->view == GIT_VIEW_STATUS)
            return MC_PPR_NOT_SUPPORTED;

        if ((git_view_t) data->view == GIT_VIEW_BRANCHES)
        {
            git_set_pending_focus (data, GIT_BRANCHES_DIR_NAME);
            data->view = GIT_VIEW_STATUS;
            git_update_title (data);
            return MC_PPR_OK;
        }

        if ((git_view_t) data->view == GIT_VIEW_BRANCHES_LOCAL
            || (git_view_t) data->view == GIT_VIEW_BRANCHES_REMOTE_ITEMS)
        {
            char *focus = NULL;

            if ((git_view_t) data->view == GIT_VIEW_BRANCHES_LOCAL)
                focus = g_strdup (GIT_BRANCHES_LOCAL_DIR_NAME);
            else
                focus = g_strdup_printf ("remote/%s",
                                         data->selected_remote != NULL ? data->selected_remote : "");
            git_set_pending_focus (data,
                                   focus);
            g_free (focus);
            data->view = GIT_VIEW_BRANCHES;
            git_update_title (data);
            return MC_PPR_OK;
        }

        if ((git_view_t) data->view == GIT_VIEW_COMMIT_FILES)
        {
            char *return_focus = g_strdup (data->selected_commit_name);

            if (git_commit_stack_pop_to_current (data))
            {
                git_set_pending_focus (data, return_focus);
                git_update_title (data);
                g_free (return_focus);
                return MC_PPR_OK;
            }
            g_free (return_focus);

            git_set_pending_focus (data,
                                   data->selected_commit_list_name != NULL
                                       ? data->selected_commit_list_name
                                       : (data->selected_commit_name != NULL ? data->selected_commit_name
                                                                            : data->selected_commit));
            data->view = GIT_VIEW_COMMITS;
            git_update_title (data);
            return MC_PPR_OK;
        }

        if ((git_view_t) data->view == GIT_VIEW_COMMITS
            && data->selected_branch_scope != GIT_BRANCH_SCOPE_NONE)
        {
            if (data->selected_branch_scope == GIT_BRANCH_SCOPE_LOCAL)
            {
                git_set_pending_focus (data, data->selected_branch);
                data->view = GIT_VIEW_BRANCHES_LOCAL;
            }
            else
            {
                const char *focus = data->selected_branch;

                if (focus != NULL && data->selected_remote != NULL)
                {
                    size_t plen = strlen (data->selected_remote);

                    if (g_str_has_prefix (focus, data->selected_remote) && focus[plen] == '/')
                        focus += plen + 1;
                }
                git_set_pending_focus (data, focus);
                data->view = GIT_VIEW_BRANCHES_REMOTE_ITEMS;
            }
            g_free (data->selected_branch);
            data->selected_branch = NULL;
            data->selected_branch_scope = GIT_BRANCH_SCOPE_NONE;
            g_free (data->selected_commit);
            data->selected_commit = NULL;
            g_free (data->selected_commit_name);
            data->selected_commit_name = NULL;
            g_free (data->selected_commit_list_name);
            data->selected_commit_list_name = NULL;
            git_commit_stack_clear (data);
            git_update_title (data);
            return MC_PPR_OK;
        }

        git_set_pending_focus (data, GIT_COMMITS_DIR_NAME);
        data->view = GIT_VIEW_STATUS;
        g_free (data->selected_branch);
        data->selected_branch = NULL;
        data->selected_branch_scope = GIT_BRANCH_SCOPE_NONE;
        g_free (data->selected_commit);
        data->selected_commit = NULL;
        g_free (data->selected_commit_name);
        data->selected_commit_name = NULL;
        g_free (data->selected_commit_list_name);
        data->selected_commit_list_name = NULL;
        git_commit_stack_clear (data);
        g_free (data->selected_remote);
        data->selected_remote = NULL;
        git_update_title (data);
        return MC_PPR_OK;
    }

    if ((git_view_t) data->view == GIT_VIEW_STATUS && strcmp (path, GIT_BRANCHES_DIR_NAME) == 0)
    {
        data->view = GIT_VIEW_BRANCHES;
        git_set_pending_focus (data, GIT_BRANCHES_DIR_NAME);
        git_update_title (data);
        return MC_PPR_OK;
    }

    if ((git_view_t) data->view == GIT_VIEW_STATUS && strcmp (path, GIT_COMMITS_DIR_NAME) == 0)
    {
        g_free (data->selected_branch);
        data->selected_branch = NULL;
        data->selected_branch_scope = GIT_BRANCH_SCOPE_NONE;
        g_free (data->selected_commit_name);
        data->selected_commit_name = NULL;
        g_free (data->selected_commit_list_name);
        data->selected_commit_list_name = NULL;
        git_commit_stack_clear (data);
        git_set_pending_focus (data, GIT_COMMITS_DIR_NAME);
        data->view = GIT_VIEW_COMMITS;
        git_update_title (data);
        return MC_PPR_OK;
    }

    if ((git_view_t) data->view == GIT_VIEW_BRANCHES
        && strcmp (path, GIT_BRANCHES_LOCAL_DIR_NAME) == 0)
    {
        data->view = GIT_VIEW_BRANCHES_LOCAL;
        git_update_title (data);
        return MC_PPR_OK;
    }

    if ((git_view_t) data->view == GIT_VIEW_BRANCHES && g_str_has_prefix (path, GIT_BRANCHES_REMOTE_PREFIX))
    {
        info = git_lookup_info (data, path);
        if (info == NULL || info->kind != GIT_ITEM_REMOTE_DIR_ENTRY)
            return MC_PPR_FAILED;

        g_free (data->selected_remote);
        data->selected_remote = g_strdup (path + strlen (GIT_BRANCHES_REMOTE_PREFIX));
        git_set_pending_focus (data, path);
        data->view = GIT_VIEW_BRANCHES_REMOTE_ITEMS;
        git_update_title (data);
        return MC_PPR_OK;
    }

    if ((git_view_t) data->view == GIT_VIEW_BRANCHES_LOCAL
        || (git_view_t) data->view == GIT_VIEW_BRANCHES_REMOTE_ITEMS)
    {
        info = git_lookup_info (data, path);
        if (info == NULL || info->kind != GIT_ITEM_BRANCH_ENTRY || info->repo_path == NULL)
            return MC_PPR_FAILED;

        g_free (data->selected_branch);
        data->selected_branch = g_strdup (info->repo_path);
        data->selected_branch_scope = ((git_view_t) data->view == GIT_VIEW_BRANCHES_LOCAL)
            ? GIT_BRANCH_SCOPE_LOCAL
            : GIT_BRANCH_SCOPE_REMOTE;
        git_set_pending_focus (data, info->repo_path);

        data->view = GIT_VIEW_COMMITS;
        git_commit_stack_clear (data);
        git_update_title (data);
        return MC_PPR_OK;
    }

    if ((git_view_t) data->view == GIT_VIEW_COMMIT_FILES)
    {
        info = git_lookup_info (data, path);
        if (info == NULL || info->kind != GIT_ITEM_COMMIT_PARENT_ENTRY || info->commit_sha == NULL)
            return MC_PPR_FAILED;

        git_commit_stack_push_current (data);
        g_free (data->selected_commit);
        data->selected_commit = g_strdup (info->commit_sha);
        g_free (data->selected_commit_name);
        data->selected_commit_name = g_strdup (path);
        g_free (data->selected_commit_list_name);
        data->selected_commit_list_name = g_strdup (path);
        git_set_pending_focus (data, path);
        git_update_title (data);
        return MC_PPR_OK;
    }

    if ((git_view_t) data->view == GIT_VIEW_COMMITS)
    {
        info = git_lookup_info (data, path);
        if (info == NULL || info->kind != GIT_ITEM_COMMIT_DIR || info->commit_sha == NULL)
            return MC_PPR_FAILED;

        data->view = GIT_VIEW_COMMIT_FILES;
        g_free (data->selected_commit);
        data->selected_commit = g_strdup (info->commit_sha);
        g_free (data->selected_commit_name);
        data->selected_commit_name =
            g_strdup (info->state_text != NULL ? info->state_text : path);
        g_free (data->selected_commit_list_name);
        data->selected_commit_list_name = g_strdup (path);
        git_set_pending_focus (data, path);
        git_update_title (data);
        return MC_PPR_OK;
    }

    return MC_PPR_FAILED;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
git_enter (void *plugin_data, const char *fname, const struct stat *st)
{
    git_data_t *data = (git_data_t *) plugin_data;
    const git_entry_info_t *info;

    git_set_pending_focus (data, NULL);

    if (fname == NULL || st == NULL)
        return MC_PPR_FAILED;

    info = git_lookup_info (data, fname);
    git_debug_log ("git: enter name='%s' mode=0%o isdir=%d view=%s kind=%d", fname, (unsigned) st->st_mode,
                   S_ISDIR (st->st_mode) ? 1 : 0, git_view_name (data->view), info != NULL ? info->kind : -1);

    if (info != NULL)
    {
        switch (info->kind)
        {
        case GIT_ITEM_BRANCHES_DIR:
        case GIT_ITEM_BRANCHES_LOCAL_DIR:
        case GIT_ITEM_REMOTE_DIR_ENTRY:
        case GIT_ITEM_BRANCH_ENTRY:
        case GIT_ITEM_COMMITS_DIR:
        case GIT_ITEM_COMMIT_DIR:
        case GIT_ITEM_COMMIT_PARENT_ENTRY:
            return git_chdir (plugin_data, fname);
        default:
            break;
        }
    }

    if (S_ISDIR (st->st_mode))
        return git_chdir (plugin_data, fname);

    return MC_PPR_NOT_SUPPORTED;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
git_view_item (void *plugin_data, const char *fname, const struct stat *st, gboolean plain_view)
{
    git_data_t *data = (git_data_t *) plugin_data;
    const git_entry_info_t *info;

    (void) st;
    (void) plain_view;

    if (data == NULL || fname == NULL)
        return MC_PPR_FAILED;

    info = git_lookup_info (data, fname);
    if (info == NULL)
        return MC_PPR_NOT_SUPPORTED;

    if ((git_view_t) data->view != GIT_VIEW_COMMITS && (git_view_t) data->view != GIT_VIEW_COMMIT_FILES)
        return MC_PPR_NOT_SUPPORTED;

    if (info->kind != GIT_ITEM_COMMIT_DIR && info->kind != GIT_ITEM_COMMIT_PARENT_ENTRY)
        return MC_PPR_NOT_SUPPORTED;

    if (git_show_commit_description_by_sha (data, info->commit_sha, fname))
        return MC_PPR_OK;

    return MC_PPR_FAILED;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
git_get_help_info (void *plugin_data, const char **filename, const char **node)
{
    git_data_t *data = (git_data_t *) plugin_data;

    if (filename != NULL && data != NULL && data->help_filename != NULL
        && exist_file (data->help_filename))
        *filename = data->help_filename;
    else if (filename != NULL)
        *filename = NULL;
    if (node != NULL)
        *node = "[Git panel plugin]";

    if (filename != NULL && *filename == NULL)
        return MC_PPR_NOT_SUPPORTED;

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
git_get_title (void *plugin_data)
{
    git_data_t *data = (git_data_t *) plugin_data;
    return data->title;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
git_handle_key (void *plugin_data, int key)
{
    git_data_t *data = (git_data_t *) plugin_data;
    const GString *current = NULL;

    if (data->host != NULL && data->host->get_current != NULL)
        current = data->host->get_current (data->host);

    git_debug_log ("git: key=%d view=%s current='%s'", key, git_view_name (data->view),
                   current != NULL && current->str != NULL ? current->str : "(null)");

    if ((data->key_diff >= 0 && key == data->key_diff)
        || (data->key_diff_alt >= 0 && key == data->key_diff_alt))
    {
        if ((git_view_t) data->view != GIT_VIEW_COMMITS && git_show_diff_selected (data))
            return MC_PPR_OK;
        return MC_PPR_FAILED;
    }

    if (data->key_refresh >= 0 && key == data->key_refresh)
        return MC_PPR_OK;

    if ((git_view_t) data->view != GIT_VIEW_STATUS)
        return MC_PPR_FAILED;

    if (data->key_stage >= 0 && key == data->key_stage)
    {
        if (git_apply_selected (data, GIT_ACTION_STAGE))
            return MC_PPR_OK;
        return MC_PPR_FAILED;
    }

    if (data->key_unstage >= 0 && key == data->key_unstage)
    {
        if (git_apply_selected (data, GIT_ACTION_UNSTAGE))
            return MC_PPR_OK;
        return MC_PPR_FAILED;
    }

    if (data->key_toggle >= 0 && key == data->key_toggle)
    {
        if (git_apply_selected (data, GIT_ACTION_TOGGLE))
            return MC_PPR_OK;
        return MC_PPR_FAILED;
    }

    if (data->key_reset >= 0 && key == data->key_reset)
    {
        if (git_apply_selected (data, GIT_ACTION_RESET))
            return MC_PPR_OK;
        return MC_PPR_FAILED;
    }

    return MC_PPR_FAILED;
}

/* --------------------------------------------------------------------------------------------- */

static const mc_panel_column_t *
git_get_columns (void *plugin_data, size_t *count)
{
    (void) plugin_data;

    if (count != NULL)
        *count = G_N_ELEMENTS (git_columns);

    return git_columns;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
git_get_column_value (void *plugin_data, const char *fname, const char *column_id)
{
    git_data_t *data = (git_data_t *) plugin_data;
    const git_entry_info_t *info;

    if (fname == NULL || column_id == NULL)
        return NULL;

    info = (const git_entry_info_t *) g_hash_table_lookup (data->display_to_info, fname);
    if (info == NULL)
        return NULL;

    if (strcmp (column_id, "type") == 0)
        return info->state_mark_text;

    if (strcmp (column_id, "status") == 0 || strcmp (column_id, "git_status") == 0
        || strcmp (column_id, "git_state") == 0 || strcmp (column_id, "git_state_text") == 0)
        return info->state_text;

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
git_get_footer (void *plugin_data)
{
    git_data_t *data = (git_data_t *) plugin_data;

    return data->current_branch;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
git_get_focus_name (void *plugin_data)
{
    git_data_t *data = (git_data_t *) plugin_data;

    return data->pending_focus;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
git_get_default_format (void *plugin_data)
{
    git_data_t *data = (git_data_t *) plugin_data;

    return data->default_format;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

const mc_panel_plugin_t *mc_panel_plugin_register (void);

const mc_panel_plugin_t *
mc_panel_plugin_register (void)
{
    return &git_plugin;
}
