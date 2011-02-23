#ifndef MC__KEYBIND_DEFAULTS_H
#define MC__KEYBIND_DEFAULTS_H

#include "lib/global.h"
#include "lib/keybind.h"        /* global_keymap_t */

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

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
#ifdef USE_DIFF_VIEW
extern GArray *diff_keymap;
#endif


extern const global_keymap_t *main_map;
extern const global_keymap_t *main_x_map;
extern const global_keymap_t *panel_map;
extern const global_keymap_t *dialog_map;
extern const global_keymap_t *input_map;
extern const global_keymap_t *listbox_map;
extern const global_keymap_t *tree_map;
extern const global_keymap_t *help_map;
#ifdef USE_INTERNAL_EDIT
extern const global_keymap_t *editor_map;
extern const global_keymap_t *editor_x_map;
#endif
extern const global_keymap_t *viewer_map;
extern const global_keymap_t *viewer_hex_map;
#ifdef USE_DIFF_VIEW
extern const global_keymap_t *diff_map;
#endif


/* main.c */
extern const global_keymap_t default_main_keymap[];
extern const global_keymap_t default_main_x_keymap[];
/* screen.c */
extern const global_keymap_t default_panel_keymap[];
/* dialog.c */
extern const global_keymap_t default_dialog_keymap[];
/* widget.c */
extern const global_keymap_t default_input_keymap[];
extern const global_keymap_t default_listbox_keymap[];
/* tree.c */
extern const global_keymap_t default_tree_keymap[];
/* help.c */
extern const global_keymap_t default_help_keymap[];
#ifdef USE_INTERNAL_EDIT
/* ../edit/editkey.c */
extern const global_keymap_t default_editor_keymap[];
extern const global_keymap_t default_editor_x_keymap[];
#endif
/* viewer/actions_cmd.c */
extern const global_keymap_t default_viewer_keymap[];
extern const global_keymap_t default_viewer_hex_keymap[];
#ifdef USE_DIFF_VIEW
/* ydiff.c */
extern const global_keymap_t default_diff_keymap[];
#endif

/*** declarations of public functions ************************************************************/

/*** inline functions ****************************************************************************/

#endif /* MC__KEYBIND_DEFAULTS_H */
