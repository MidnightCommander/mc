
/** \file main.h
 *  \brief Header: this is a main module header
 */

#ifndef MC_MAIN_H
#define MC_MAIN_H

#include "menu.h"
#include "panel.h"
#include "widget.h"

#ifdef USE_INTERNAL_EDIT
#include "../edit/edit.h"
#endif

/* Toggling functions */
void toggle_fast_reload (void);
void toggle_mix_all_files (void);
void toggle_show_backup (void);
void toggle_show_hidden (void);

#define UP_OPTIMIZE 0
#define UP_RELOAD   1
#define UP_ONLY_CURRENT  2

#define UP_KEEPSEL ((char *) -1)

extern int quote;
extern volatile int quit;

/* If true, after executing a command, wait for a keystroke */
enum { pause_never, pause_on_dumb_terminals, pause_always };

void subshell_chdir (const char *command);

/* See main.c for details on these variables */
extern int mark_moves_down;
extern int auto_menu;
extern int pause_after_run;
extern int auto_save_setup;
extern int use_internal_view;
extern int use_internal_edit;
extern int fast_reload_w;
extern int clear_before_exec;
extern int mou_auto_repeat;
extern char *other_dir;
extern int mouse_move_pages;

#ifdef USE_INTERNAL_EDIT
extern int edit_stack_iterator;
struct edit_stack_type edit_history_moveto[MAX_HISTORY_MOVETO];
#endif

#ifdef HAVE_CHARSET
extern int source_codepage;
extern int display_codepage;
extern int utf8_display;
#else
extern int eight_bit_clean;
extern int full_eight_bits;
#endif /* !HAVE_CHARSET */

extern int confirm_view_dir;
extern int fast_refresh;
extern int navigate_with_arrows;
extern int force_ugly_line_drawing;
extern int drop_menus;
extern int cd_symlinks;
extern int show_all_if_ambiguous;
extern int slow_terminal;
extern int update_prompt;	/* To comunicate with subshell */
extern int safe_delete;
extern int confirm_delete;
extern int confirm_directory_hotlist_delete;
extern int confirm_execute;
extern int confirm_exit;
extern int confirm_overwrite;
extern int force_colors;
extern int boot_current_is_left;
extern int use_file_to_check_type;
extern int vfs_use_limit;
extern int alternate_plus_minus;
extern int only_leading_plus_minus;
extern int output_starts_shell;
extern int midnight_shutdown;
extern char cmd_buf [512];
extern const char *shell;
extern int auto_fill_mkdir_name;

/* Ugly hack in order to distinguish between left and right panel in menubar */
extern int is_right;		/* If the selected menu was the right */
#define MENU_PANEL (is_right ? right_panel : left_panel)
#define MENU_PANEL_IDX  (is_right ? 1 : 0)
#define SELECTED_IS_PANEL (get_display_type (is_right ? 1 : 0) == view_listing)

typedef void (*key_callback) (void);

/* The keymaps are of this type */
typedef struct {
    int   key_code;
    key_callback fn;
} key_map;

void update_panels (int force_update, const char *current_file);
void repaint_screen (void);
void do_update_prompt (void);

enum cd_enum {
    cd_parse_command,
    cd_exact
};

int do_cd           (const char *new_dir, enum cd_enum cd_type); /* For find.c */
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
char *get_mc_lib_dir (void);

int maybe_cd (int move_up_dir);
void do_possible_cd (const char *dir);

void done_menu (void);
void init_menu (void);

char *remove_encoding_from_path (const char *);

#define MC_BASE "/.mc/"

void directory_history_add   (struct WPanel *panel, const char *dir);
int  do_panel_cd             (struct WPanel *panel, const char *new_dir, enum cd_enum cd_type);

#endif
