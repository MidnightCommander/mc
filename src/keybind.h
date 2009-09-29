
#ifndef MC_KEYBIND_H
#define MC_KEYBIND_H

#include "global.h"

#define GLOBAL_KEYMAP_FILE	"mc.keymap"

typedef struct name_key_map_t {
    const char *name;
    int val;
} name_key_map_t;

typedef struct key_config_t {
    time_t mtime;	/* mtime at the moment we read config file */
    GArray *keymap;
    GArray *ext_keymap;
    gchar *labels[10];
} key_config_t;

/* The global keymaps are of this type */
typedef struct global_key_map_t {
    long key;
    long command;
} global_key_map_t;

int lookup_action (char *keyname);
void keybind_cmd_bind(GArray *keymap, char *keybind, int action);

#endif					/* MC_KEYBIND_H */

/* viewer/actions_cmd.c */
extern const global_key_map_t default_viewer_keymap[];
extern const global_key_map_t default_viewer_hex_keymap[];

/* ../edit/editkey.c */
extern const global_key_map_t default_editor_keymap[];
extern const global_key_map_t default_editor_x_keymap[];

/* screen.c */
extern const global_key_map_t default_panel_keymap[];

/* widget.c */
extern const global_key_map_t default_input_keymap[];

/* main.c */
extern const global_key_map_t default_main_map[];
extern const global_key_map_t default_main_x_map[];

extern const global_key_map_t default_input_keymap[];
