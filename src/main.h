
/** \file main.h
 *  \brief Header: this is a main module header
 */

#ifndef MC_MAIN_H
#define MC_MAIN_H

#include "lib/global.h"
#include "keybind.h"

/* run mode and params */
typedef enum
{
    MC_RUN_FULL = 0,
    MC_RUN_EDITOR,
    MC_RUN_VIEWER,
    MC_RUN_DIFFVIEWER
} mc_run_mode_t;

extern mc_run_mode_t mc_run_mode;
/*
 * MC_RUN_FULL: dir for left panel
 * MC_RUN_EDITOR: file to edit
 * MC_RUN_VIEWER: file to view
 * MC_RUN_DIFFVIEWER: first file to compare
 */
extern char *mc_run_param0;
/*
 * MC_RUN_FULL: dir for right panel
 * MC_RUN_EDITOR: unused
 * MC_RUN_VIEWER: unused
 * MC_RUN_DIFFVIEWER: second file to compare
 */
extern char *mc_run_param1;

void toggle_show_hidden (void);

extern int quote;
extern volatile int quit;

/* If true, after executing a command, wait for a keystroke */
enum { pause_never, pause_on_dumb_terminals, pause_always };

void subshell_chdir (const char *command);

struct WButtonBar;

void midnight_set_buttonbar (struct WButtonBar *b);

/* See main.c for details on these variables */
extern int auto_menu;
extern int pause_after_run;
extern int auto_save_setup;
extern int use_internal_view;
extern int use_internal_edit;
extern int clear_before_exec;

#ifdef HAVE_CHARSET
extern int source_codepage;
extern int default_source_codepage;
extern int display_codepage;
extern char *autodetect_codeset;
extern gboolean is_autodetect_codeset_enabled;
#else
extern int eight_bit_clean;
extern int full_eight_bits;
#endif /* !HAVE_CHARSET */

extern char *clipboard_store_path;
extern char *clipboard_paste_path;

extern int utf8_display;

extern int fast_refresh;
extern int drop_menus;
extern int cd_symlinks;
extern int show_all_if_ambiguous;
extern int update_prompt;       /* To comunicate with subshell */
extern int safe_delete;
extern int confirm_delete;
extern int confirm_directory_hotlist_delete;
extern int confirm_execute;
extern int confirm_exit;
extern int confirm_overwrite;
extern int confirm_history_cleanup;
extern int confirm_view_dir;
extern int boot_current_is_left;
extern int use_file_to_check_type;
extern int vfs_use_limit;
extern int only_leading_plus_minus;
extern int output_starts_shell;
extern int midnight_shutdown;
extern char *shell;
extern int auto_fill_mkdir_name;
/* Ugly hack in order to distinguish between left and right panel in menubar */
extern int is_right;            /* If the selected menu was the right */
#define MENU_PANEL (is_right ? right_panel : left_panel)
#define MENU_PANEL_IDX  (is_right ? 1 : 0)
#define SELECTED_IS_PANEL (get_display_type (is_right ? 1 : 0) == view_listing)

#ifdef HAVE_SUBSHELL_SUPPORT
void do_update_prompt (void);
int load_prompt (int fd, void *unused);
#endif

enum cd_enum
{
    cd_parse_command,
    cd_exact
};

int do_cd (const char *new_dir, enum cd_enum cd_type);
void sort_cmd (void);
void change_panel (void);
void save_cwds_stat (void);
gboolean quiet_quit_cmd (void);     /* For cmd.c and command.c */

void touch_bar (void);
void update_xterm_title_path (void);
void load_hint (int force);

void print_vfs_message (const char *msg, ...) __attribute__ ((format (__printf__, 1, 2)));

extern const char *mc_prompt;
extern char *mc_home;
extern char *mc_home_alt;

struct mc_fhl_struct;
extern struct mc_fhl_struct *mc_filehighlight;

char *get_mc_lib_dir (void);

void done_menu (void);
void init_menu (void);

char *remove_encoding_from_path (const char *);

#endif /* MC_MAIN_H */
