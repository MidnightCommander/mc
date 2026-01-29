#ifndef MC_SKIN_H
#define MC_SKIN_H

#include "lib/global.h"

#include "lib/mcconfig.h"

#include "lib/tty/color.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct mc_skin_struct
{
    gchar *name;
    gchar *description;
    mc_config_t *config;
    GHashTable *colors;
    gboolean have_256_colors;
    gboolean have_true_colors;
} mc_skin_t;

/*** global variables defined in .c file *********************************************************/

extern mc_skin_t mc_skin__default;

/*** declarations of public functions ************************************************************/

gboolean mc_skin_init (const gchar *skin_override, GError **error);
void mc_skin_deinit (void);

int mc_skin_color_get (const gchar *group, const gchar *name);

void mc_skin_lines_parse_ini_file (mc_skin_t *mc_skin);

gchar *mc_skin_get (const gchar *group, const gchar *key, const gchar *default_value);

GPtrArray *mc_skin_list (void);

#endif
