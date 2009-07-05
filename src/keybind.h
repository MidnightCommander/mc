
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
