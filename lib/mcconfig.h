#ifndef MC_CONFIG_H
#define MC_CONFIG_H

#include "lib/vfs/vfs.h"        /* vfs_path_t */

/*** typedefs(not structures) and defined constants **********************************************/

#define CONFIG_APP_SECTION "Midnight-Commander"
#define CONFIG_PANELS_SECTION "Panels"
#define CONFIG_LAYOUT_SECTION "Layout"
#define CONFIG_MISC_SECTION "Misc"
#define CONFIG_EXT_EDITOR_VIEWER_SECTION "External editor or viewer parameters"

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct mc_config_t
{
    GKeyFile *handle;
    gchar *ini_path;
} mc_config_t;

/*** global variables defined in .c file *********************************************************/

extern mc_config_t *mc_main_config;
extern mc_config_t *mc_panels_config;

/*** declarations of public functions ************************************************************/

/* mcconfig/common.c: */

mc_config_t *mc_config_init (const gchar * ini_path, gboolean read_only);
void mc_config_deinit (mc_config_t * mc_config);

gboolean mc_config_del_key (mc_config_t *, const char *, const gchar *);
gboolean mc_config_del_group (mc_config_t *, const char *);

gboolean mc_config_has_param (const mc_config_t *, const char *, const gchar *);
gboolean mc_config_has_group (mc_config_t *, const char *);

gboolean mc_config_read_file (mc_config_t * mc_config, const gchar * ini_path, gboolean read_only,
                              gboolean remove_empty);

gboolean mc_config_save_file (mc_config_t * config, GError ** error);

gboolean mc_config_save_to_file (mc_config_t * config, const gchar * filename, GError ** error);


/* mcconfig/get.c: */

gchar **mc_config_get_groups (const mc_config_t *, gsize *);

gchar **mc_config_get_keys (const mc_config_t *, const gchar *, gsize *);

gchar *mc_config_get_string (mc_config_t *, const gchar *, const gchar *, const gchar *);

gchar *mc_config_get_string_raw (const mc_config_t *, const gchar *, const gchar *, const gchar *);

gboolean mc_config_get_bool (mc_config_t *, const gchar *, const gchar *, gboolean);

int mc_config_get_int (mc_config_t *, const gchar *, const gchar *, int);


gchar **mc_config_get_string_list (mc_config_t *, const gchar *, const gchar *, gsize *);

gboolean *mc_config_get_bool_list (mc_config_t *, const gchar *, const gchar *, gsize *);

int *mc_config_get_int_list (mc_config_t *, const gchar *, const gchar *, gsize *);


/* mcconfig/set.c: */

void mc_config_set_string_raw (mc_config_t *, const gchar *, const gchar *, const gchar *);

void mc_config_set_string_raw_value (mc_config_t *, const gchar *, const gchar *, const gchar *);

void mc_config_set_string (const mc_config_t *, const gchar *, const gchar *, const gchar *);

void mc_config_set_bool (mc_config_t *, const gchar *, const gchar *, gboolean);

void mc_config_set_int (mc_config_t *, const gchar *, const gchar *, int);

void
mc_config_set_string_list (mc_config_t *, const gchar *,
                           const gchar *, const gchar * const[], gsize);

void mc_config_set_bool_list (mc_config_t *, const gchar *, const gchar *, gboolean[], gsize);

void mc_config_set_int_list (mc_config_t *, const gchar *, const gchar *, int[], gsize);


/* mcconfig/dialog.c: */

void mc_config_show_dialog (void);


/* mcconfig/paths.c: */

void mc_config_init_config_paths (GError ** error);

void mc_config_deinit_config_paths (void);

gboolean mc_config_migrate_from_old_place (GError ** error, char **msg);

const char *mc_config_get_data_path (void);

const char *mc_config_get_cache_path (void);

const char *mc_config_get_path (void);

const char *mc_config_get_home_dir (void);

char *mc_config_get_full_path (const char *config_name);

vfs_path_t *mc_config_get_full_vpath (const char *config_name);

/*** inline functions ****************************************************************************/

#endif
