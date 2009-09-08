#ifndef MC__SKIN_INTERNAL_H
#define MC__SKIN_INTERNAL_H

/*** typedefs(not structures) and defined constants **********************************************/

#define MC_SKIN_COLOR_CACHE_COUNT 5

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct mc_skin_color_struct{
    gchar *fgcolor;
    gchar *bgcolor;
    int pair_index;
} mc_skin_color_t;

/*** global variables defined in .c file *********************************************************/

extern mc_skin_t mc_skin__default;

/*** declarations of public functions ************************************************************/


gboolean mc_skin_ini_file_load(mc_skin_t *);
gboolean mc_skin_ini_file_parce(mc_skin_t *);
void mc_skin_set_hardcoded_skin(mc_skin_t *);

gboolean mc_skin_ini_file_parce_colors(mc_skin_t *);


void mc_skin_hardcoded_ugly_lines(mc_skin_t *);
void mc_skin_hardcoded_space_lines(mc_skin_t *);
void mc_skin_hardcoded_blackwhite_colors(mc_skin_t *);

#endif
