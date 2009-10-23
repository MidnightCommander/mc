
#ifndef MC_KEYBIND_H
#define MC_KEYBIND_H

#include "global.h"

typedef struct name_keymap_t {
    const char *name;
    int val;
} name_keymap_t;

typedef struct key_config_t {
    time_t mtime;	/* mtime at the moment we read config file */
    GArray *keymap;
    GArray *ext_keymap;
    gchar *labels[10];
} key_config_t;

/* The global keymaps are of this type */
#define KEYMAP_SHORTCUT_LENGTH		32 /* FIXME: is 32 bytes enough for shortcut? */
typedef struct global_keymap_t {
    long key;
    long command;
    char caption[KEYMAP_SHORTCUT_LENGTH];
} global_keymap_t;

int lookup_action (const char *keyname);
void keybind_cmd_bind (GArray *keymap, const char *keybind, int action);

/* viewer/actions_cmd.c */
extern const global_keymap_t default_viewer_keymap[];
extern const global_keymap_t default_viewer_hex_keymap[];

#ifdef USE_INTERNAL_EDIT
/* ../edit/editkey.c */
extern const global_keymap_t default_editor_keymap[];
extern const global_keymap_t default_editor_x_keymap[];
#endif

/* screen.c */
extern const global_keymap_t default_panel_keymap[];

/* widget.c */
extern const global_keymap_t default_input_keymap[];

/* main.c */
extern const global_keymap_t default_main_map[];
extern const global_keymap_t default_main_x_map[];

extern const global_keymap_t default_input_keymap[];

#endif					/* MC_KEYBIND_H */
