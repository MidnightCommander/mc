#ifndef MC_CONFIG_H
#define MC_CONFIG_H

/*** typedefs(not structures) and defined constants ********************/

#define CONFIG_APP_SECTION "Midnight-Commander"

/*** enums *************************************************************/

/*** structures declarations (and typedefs of structures)***************/

typedef struct mc_config_struct
{
    GKeyFile *handle;
    gchar *ini_path;
} mc_config_t;

/*** global variables defined in .c file *******************************/

extern mc_config_t *mc_main_config;
extern mc_config_t *mc_panels_config;

/*** declarations of public functions **********************************/


/* mcconfig/common.c: */

mc_config_t *mc_config_init (const gchar *);
void mc_config_deinit (mc_config_t *);

gboolean mc_config_del_param (mc_config_t *, const char *, const gchar *);
gboolean mc_config_del_group (mc_config_t *, const char *);

gboolean mc_config_has_param (mc_config_t *, const char *, const gchar *);
gboolean mc_config_has_group (mc_config_t *, const char *);

gboolean mc_config_read_file (mc_config_t *, const gchar *);

gboolean mc_config_save_file (mc_config_t *);

gboolean mc_config_save_to_file (mc_config_t *, const gchar *);

/* mcconfig/get.c: */

gchar **mc_config_get_groups (mc_config_t *, gsize *);

gchar **mc_config_get_keys (mc_config_t *, const gchar *, gsize *);

gchar *mc_config_get_string (mc_config_t *, const gchar *, const gchar *,
			     const gchar *);

gchar *mc_config_get_string_raw (mc_config_t *, const gchar *, const gchar *,
			     const gchar *);

gboolean mc_config_get_bool (mc_config_t *, const gchar *, const gchar *,
			     gboolean);

int mc_config_get_int (mc_config_t *, const gchar *, const gchar *, int);


gchar **mc_config_get_string_list (mc_config_t *, const gchar *,
				   const gchar *, gsize *);

gboolean *mc_config_get_bool_list (mc_config_t *, const gchar *,
				   const gchar *, gsize *);

int *mc_config_get_int_list (mc_config_t *, const gchar *,
			     const gchar *, gsize *);


/* mcconfig/set.c: */

void
mc_config_set_string (mc_config_t *, const gchar *,
		      const gchar *, const gchar *);

void
mc_config_set_bool (mc_config_t *, const gchar *, const gchar *, gboolean);

void mc_config_set_int (mc_config_t *, const gchar *, const gchar *, int);

void
mc_config_set_string_list (mc_config_t *, const gchar *,
			   const gchar *, const gchar * const[], gsize);

void
mc_config_set_bool_list (mc_config_t *, const gchar *,
			 const gchar *, gboolean[], gsize);

void
mc_config_set_int_list (mc_config_t *, const gchar *,
			const gchar *, int[], gsize);


/* mcconfig/dialog.c: */

void mc_config_show_dialog (void);


#endif
