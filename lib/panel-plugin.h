/** \file panel-plugin.h
 *  \brief Header: panel plugin API for third-party panel content providers
 */

#ifndef MC__PANEL_PLUGIN_H
#define MC__PANEL_PLUGIN_H

#include "lib/global.h"
#include "lib/strutil.h"

/*
 * Note: get_items callback receives a dir_list* (from src/filemanager/dir.h)
 * cast to void*.  Plugin implementations should include dir.h and cast back.
 */

/*** typedefs(not structures) and defined constants **********************************************/

#define MC_PANEL_PLUGIN_API_VERSION 4
#define MC_PANEL_PLUGIN_ENTRY       "mc_panel_plugin_register"

/*** enums ***************************************************************************************/

typedef enum
{
    MC_PPR_OK = 0,
    MC_PPR_FAILED = -1,
    MC_PPR_NOT_SUPPORTED = -2,
    MC_PPR_CLOSE = -3
} mc_pp_result_t;

typedef enum
{
    MC_PPF_NONE = 0,
    MC_PPF_NAVIGATE = 1 << 0,  /* handles chdir/".." */
    MC_PPF_GET_FILES = 1 << 1, /* can extract files */
    MC_PPF_DELETE = 1 << 2,    /* can delete items */
    MC_PPF_CUSTOM_TITLE = 1 << 3,
    MC_PPF_CREATE = 1 << 4,      /* supports Shift+F4 (create item) */
    MC_PPF_PUT_FILES = 1 << 5,   /* can accept files (put_file/save_file) */
    MC_PPF_SHOW_IN_MENU = 1 << 6 /* add entry to left/right panel menu */
} mc_pp_flags_t;

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct mc_panel_column_t
{
    const char *id;
    const char *title;
    int min_size;
    gboolean expands;
    align_crt_t default_just;
    gboolean use_in_user_format;
} mc_panel_column_t;

/* What mc provides to the plugin */
typedef struct mc_panel_host_t
{
    void (*refresh) (struct mc_panel_host_t *host);
    void (*set_hint) (struct mc_panel_host_t *host, const char *text);
    void (*message) (struct mc_panel_host_t *host, int flags, const char *title, const char *text);
    void (*run_command) (struct mc_panel_host_t *host, const char *command, int flags);
    gboolean (*open_diff) (struct mc_panel_host_t *host, const char *left_path,
                           const char *right_path);
    void (*close_plugin) (struct mc_panel_host_t *host, const char *dir_path);
    int (*get_marked_count) (struct mc_panel_host_t *host);
    const GString *(*get_next_marked) (struct mc_panel_host_t *host, int *current);
    const GString *(*get_current) (struct mc_panel_host_t *host);
    void *host_data; /* opaque, points to WPanel internally */
} mc_panel_host_t;

/* What the plugin provides (callback table) */
typedef struct mc_panel_plugin_t
{
    int api_version;          /* MC_PANEL_PLUGIN_API_VERSION */
    const char *name;         /* "docker", "git-log" */
    const char *display_name; /* "Docker containers" */
    const char *proto; /* protocol prefix for panel title, e.g. "HelloWorld" → "HelloWorld:/path" */
    const char *prefix; /* "docker:" or NULL */
    mc_pp_flags_t flags;

    /* Required */
    void *(*open) (mc_panel_host_t *host, const char *open_path);
    void (*close) (void *plugin_data);
    /* Populate the panel.  @list is a dir_list* (from dir.h).
       The ".." entry at index 0 is already created by the host;
       the plugin must NOT add ".." itself — only real items. */
    mc_pp_result_t (*get_items) (void *plugin_data, void *list /* dir_list* */);

    /* Optional (NULL = not supported) */
    mc_pp_result_t (*chdir) (void *plugin_data, const char *path);
    mc_pp_result_t (*enter) (void *plugin_data, const char *fname, const struct stat *st);
    /* Optional view hook for F3/Shift-F3 flow.
       If returns MC_PPR_OK, core treats view command as handled.
       If returns MC_PPR_NOT_SUPPORTED, core uses default view behavior. */
    mc_pp_result_t (*view) (void *plugin_data, const char *fname, const struct stat *st,
                            gboolean plain_view);
    /* Optional help hook for plugin-specific help nodes/files.
       If returns MC_PPR_OK, core opens help using returned filename/node.
       filename may be NULL to use default help file; node may be NULL for default node. */
    mc_pp_result_t (*get_help_info) (void *plugin_data, const char **filename, const char **node);
    mc_pp_result_t (*get_local_copy) (void *plugin_data, const char *fname, char **local_path);
    mc_pp_result_t (*put_file) (void *plugin_data, const char *local_path, const char *dest_name);
    mc_pp_result_t (*save_file) (void *plugin_data, const char *local_path,
                                 const char *remote_name);
    mc_pp_result_t (*delete_items) (void *plugin_data, const char **names, int count);
    const char *(*get_title) (void *plugin_data);
    mc_pp_result_t (*handle_key) (void *plugin_data, int key);
    mc_pp_result_t (*create_item) (void *plugin_data);
    const mc_panel_column_t *(*get_columns) (void *plugin_data, size_t *count);
    const char *(*get_column_value) (void *plugin_data, const char *fname, const char *column_id);
    /* Optional footer text shown on panel bottom line near free space indicator. */
    const char *(*get_footer) (void *plugin_data);
    /* Optional preferred focus item name after plugin navigation/reload. */
    const char *(*get_focus_name) (void *plugin_data);
    /* Optional plugin-provided default panel format (e.g. "type name | status | size").
       Return NULL or empty string to use generic core fallback from get_columns(). */
    const char *(*get_default_format) (void *plugin_data);
} mc_panel_plugin_t;

typedef const mc_panel_plugin_t *(*mc_panel_plugin_register_fn) (void);

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* Shared helper: build a struct stat and append one entry to the dir_list.
   Plugins that used a local add_entry()/add_fake_entry() can call this instead. */
void mc_pp_add_entry (void *list, const char *name, mode_t mode, off_t size, time_t mtime);

/* Registry */
gboolean mc_panel_plugin_add (const mc_panel_plugin_t *plugin);
const GSList *mc_panel_plugin_list (void);
const mc_panel_plugin_t *mc_panel_plugin_find_by_name (const char *name);
const mc_panel_plugin_t *mc_panel_plugin_find_by_prefix (const char *prefix);

/* Loader */
void mc_panel_plugins_load (void);
void mc_panel_plugins_shutdown (void);

/*** inline functions ****************************************************************************/

#endif /* MC__PANEL_PLUGIN_H */
