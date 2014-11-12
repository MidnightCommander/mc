#ifndef MC__KEYBIND_DEFAULTS_H
#define MC__KEYBIND_DEFAULTS_H

#include "lib/global.h"
#include "lib/keybind.h"        /* global_keymap_t */
#include "lib/mcconfig.h"       /* mc_config_t */

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct global_keymap_ini_t
{
    const char *key;
    const char *value;
} default_keymap_ini_t;

/*** global variables defined in .c file *********************************************************/

extern GArray *main_keymap;
extern GArray *main_x_keymap;
extern GArray *panel_keymap;
extern GArray *dialog_keymap;
extern GArray *input_keymap;
extern GArray *listbox_keymap;
extern GArray *tree_keymap;
extern GArray *help_keymap;
#ifdef USE_INTERNAL_EDIT
extern GArray *editor_keymap;
extern GArray *editor_x_keymap;
#endif
extern GArray *viewer_keymap;
extern GArray *viewer_hex_keymap;


extern const global_keymap_t *main_map;
extern const global_keymap_t *main_x_map;
extern const global_keymap_t *panel_map;
extern const global_keymap_t *tree_map;
extern const global_keymap_t *help_map;

#ifdef USE_INTERNAL_EDIT
extern const global_keymap_t *editor_map;
extern const global_keymap_t *editor_x_map;
#endif
extern const global_keymap_t *viewer_map;
extern const global_keymap_t *viewer_hex_map;

/*** declarations of public functions ************************************************************/

mc_config_t *create_default_keymap (GError ** error);
void mc_core_keybind_mass_init (const char *keymap_group, const default_keymap_ini_t * k,
                                gboolean isDeleteOld, GError ** error);

/*** inline functions ****************************************************************************/

#endif /* MC__KEYBIND_DEFAULTS_H */
