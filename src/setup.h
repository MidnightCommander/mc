
/** \file setup.h
 *  \brief Header: setup loading/saving
 */

#ifndef MC_SETUP_H
#define MC_SETUP_H

#include <config.h>

#include "lib/global.h"         /* GError */

#include "panel.h"              /* WPanel, panel_view_mode_t */

/* global paremeters */
extern char *profile_name;
extern char *global_profile_name;
extern char *setup_color_string;
extern char *term_color_string;
extern char *color_terminal_string;
extern int verbose;
extern int mouse_close_dialog;
extern int reverse_files_only;
extern int select_flags;
extern int setup_copymove_persistent_attr;
extern int num_history_items_recorded;

char *setup_init (void);
void load_setup (void);
gboolean save_setup (void);
void done_setup (void);
void save_config (void);
void setup_save_config_show_error (const char *filename, GError **error);

void save_layout (void);

void load_key_defs (void);
#if defined(ENABLE_VFS) && defined (USE_NETCODE)
char *load_anon_passwd (void);
#endif /* ENABLE_VFS && defined USE_NETCODE */

void load_keymap_defs (void);
void free_keymap_defs (void);

/* panel setup */
extern panel_view_mode_t startup_left_mode;
extern panel_view_mode_t startup_right_mode;

void panel_load_setup (struct WPanel *panel, const char *section);
void panel_save_setup (struct WPanel *panel, const char *section);
void save_panel_types (void);

#endif                          /* MC_SETUP_H */
