
/** \file main.h
 *  \brief Header: this is a main module header
 */

#ifndef MC_MAIN_H
#define MC_MAIN_H

#include "global.h"
#include "keybind.h"

/* Toggling functions */
void toggle_fast_reload (void);
void toggle_mix_all_files (void);
void toggle_show_backup (void);
void toggle_show_hidden (void);
void toggle_kilobyte_si (void);

extern int quote;
extern volatile int quit;

/* If true, after executing a command, wait for a keystroke */
enum { pause_never, pause_on_dumb_terminals, pause_always };

void subshell_chdir (const char *command);

struct WButtonBar;

void midnight_set_buttonbar (struct WButtonBar *b);

/* See main.c for details on these variables */
extern int mark_moves_down;
extern int auto_menu;
extern int pause_after_run;
extern int auto_save_setup;
extern int use_internal_view;
extern int use_internal_edit;
extern int fast_reload_w;
extern int clear_before_exec;
extern char *other_dir;
extern int mouse_move_pages;

extern int option_tab_spacing;

#ifdef HAVE_CHARSET
extern int source_codepage;
extern int display_codepage;
#else
extern int eight_bit_clean;
extern int full_eight_bits;
#endif /* !HAVE_CHARSET */

extern int utf8_display;

extern int confirm_view_dir;
extern int fast_refresh;
extern int navigate_with_arrows;
extern int drop_menus;
extern int cd_symlinks;
extern int show_all_if_ambiguous;
extern int update_prompt;	/* To comunicate with subshell */
extern int safe_delete;
extern int confirm_delete;
extern int confirm_directory_hotlist_delete;
extern int confirm_execute;
extern int confirm_exit;
extern int confirm_overwrite;
extern int boot_current_is_left;
extern int use_file_to_check_type;
extern int vfs_use_limit;
extern int only_leading_plus_minus;
extern int output_starts_shell;
extern int midnight_shutdown;
extern char *shell;
extern int auto_fill_mkdir_name;
extern int skip_check_codeset;
/* Ugly hack in order to distinguish between left and right panel in menubar */
extern int is_right;		/* If the selected menu was the right */
#define MENU_PANEL (is_right ? right_panel : left_panel)
#define MENU_PANEL_IDX  (is_right ? 1 : 0)
#define SELECTED_IS_PANEL (get_display_type (is_right ? 1 : 0) == view_listing)

#ifdef USE_INTERNAL_EDIT
extern GArray *editor_keymap;
extern GArray *editor_x_keymap;
#endif
extern GArray *viewer_keymap;
extern GArray *viewer_hex_keymap;
extern GArray *main_keymap;
extern GArray *main_x_keymap;
extern GArray *panel_keymap;
extern GArray *input_keymap;
extern GArray *tree_keymap;
extern GArray *help_keymap;

extern const global_keymap_t *panel_map;
extern const global_keymap_t *input_map;
extern const global_keymap_t *tree_map;
extern const global_keymap_t *help_map;

#ifdef HAVE_SUBSHELL_SUPPORT
void do_update_prompt (void);
#endif

enum cd_enum {
    cd_parse_command,
    cd_exact
};

int do_cd           (const char *new_dir, enum cd_enum cd_type); /* For find.c */
void sort_cmd (void);
void change_panel   (void);
int load_prompt     (int, void *);
void save_cwds_stat (void);
void quiet_quit_cmd (void);	/* For cmd.c and command.c */

void touch_bar      (void);
void update_xterm_title_path (void);
void load_hint      (int force);

void print_vfs_message(const char *msg, ...)
    __attribute__ ((format (__printf__, 1, 2)));

extern const char *prompt;
extern const char *edit_one_file;
extern char *mc_home;
extern char *mc_home_alt;

struct mc_fhl_struct;
extern struct mc_fhl_struct *mc_filehighlight;

char *get_mc_lib_dir (void);

void done_menu (void);
void init_menu (void);

char *remove_encoding_from_path (const char *);

struct WPanel;

void directory_history_add (struct WPanel *panel, const char *dir);
int do_panel_cd (struct WPanel *panel, const char *new_dir, enum cd_enum cd_type);

#endif					/* MC_MAIN_H */
