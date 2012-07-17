/** \file setup.h
 *  \brief Header: setup loading/saving
 */

#ifndef MC__SETUP_H
#define MC__SETUP_H

#include <config.h>

#include "lib/global.h"         /* GError */

#include "filemanager/layout.h" /* panel_view_mode_t */

/*** typedefs(not structures) and defined constants **********************************************/

/* TAB length for editor and viewer */
#define DEFAULT_TAB_SPACING 8

/*** enums ***************************************************************************************/

typedef enum
{
    QSEARCH_CASE_INSENSITIVE = 0,       /* quick search in case insensitive mode */
    QSEARCH_CASE_SENSITIVE = 1, /* quick search in case sensitive mode */
    QSEARCH_PANEL_CASE = 2,     /* quick search get value from panel case_sensitive */
    QSEARCH_NUM
} qsearch_mode_t;

/*** structures declarations (and typedefs of structures)*****************************************/

/* panels ini options; [Panels] section */
typedef struct
{
    gboolean show_mini_info;    /* If true, show the mini-info on the panel */
    gboolean kilobyte_si;       /* If TRUE, SI units (1000 based) will be used for larger units
                                 * (kilobyte, megabyte, ...). If FALSE, binary units (1024 based) will be used */
    gboolean mix_all_files;     /* If FALSE then directories are shown separately from files */
    gboolean show_backups;      /* If TRUE, show files ending in ~ */
    gboolean show_dot_files;    /* If TRUE, show files starting with a dot */
    gboolean fast_reload;       /* If TRUE then use stat() on the cwd to determine directory changes */
    gboolean fast_reload_msg_shown;     /* Have we shown the fast-reload warning in the past? */
    gboolean mark_moves_down;   /* If TRUE, marking a files moves the cursor down */
    gboolean reverse_files_only;        /* If TRUE, only selection of files is inverted */
    gboolean auto_save_setup;
    gboolean navigate_with_arrows;      /* If TRUE: l&r arrows are used to chdir if the input line is empty */
    gboolean scroll_pages;      /* If TRUE, panel is scrolled by half the display when the cursor reaches
                                   the end or the beginning of the panel */
    gboolean mouse_move_pages;  /* Scroll page/item using mouse wheel */
    gboolean filetype_mode;     /* If TRUE then add per file type hilighting */
    gboolean permission_mode;   /* If TRUE, we use permission hilighting */
    qsearch_mode_t qsearch_mode;        /* Quick search mode */
    gboolean torben_fj_mode;    /* If TRUE, use some usability hacks by Torben */
} panels_options_t;

struct WPanel;

/*** global variables defined in .c file *********************************************************/

/* global paremeters */
extern char *profile_name;
extern char *global_profile_name;
extern int confirm_delete;
extern int confirm_directory_hotlist_delete;
extern int confirm_execute;
extern int confirm_exit;
extern int confirm_overwrite;
extern int confirm_view_dir;
extern int safe_delete;
extern int clear_before_exec;
extern int auto_menu;
extern int drop_menus;
extern int verbose;
extern int select_flags;
extern int setup_copymove_persistent_attr;
extern int classic_progressbar;
extern int easy_patterns;
extern int option_tab_spacing;
extern int auto_save_setup;
extern int only_leading_plus_minus;
extern int cd_symlinks;
extern int auto_fill_mkdir_name;
extern int output_starts_shell;
extern int use_file_to_check_type;
extern int file_op_compute_totals;

extern panels_options_t panels_options;

extern panel_view_mode_t startup_left_mode;
extern panel_view_mode_t startup_right_mode;
extern gboolean boot_current_is_left;

/*** declarations of public functions ************************************************************/

char *setup_init (void);
void load_setup (void);
gboolean save_setup (gboolean save_options, gboolean save_panel_options);
void done_setup (void);
void save_config (void);
void setup_save_config_show_error (const char *filename, GError ** error);

void save_layout (void);

void load_key_defs (void);
#ifdef ENABLE_VFS_FTP
char *load_anon_passwd (void);
#endif /* ENABLE_VFS_FTP */

void load_keymap_defs (gboolean load_from_file);
void free_keymap_defs (void);

void panel_load_setup (struct WPanel *panel, const char *section);
void panel_save_setup (struct WPanel *panel, const char *section);

void panels_load_options (void);
void panels_save_options (void);

/*** inline functions ****************************************************************************/

#endif /* MC__SETUP_H */
