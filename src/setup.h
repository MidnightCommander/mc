
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
extern int select_flags;
extern int setup_copymove_persistent_attr;
extern int num_history_items_recorded;
extern int classic_progressbar;

/* TAB length for editor and viewer */
#define DEFAULT_TAB_SPACING 8
extern int option_tab_spacing;

char *setup_init (void);
void load_setup (void);
gboolean save_setup (void);
void done_setup (void);
void save_config (void);
void setup_save_config_show_error (const char *filename, GError **error);

void save_layout (void);

void load_key_defs (void);
#ifdef ENABLE_VFS_FTP
char *load_anon_passwd (void);
#endif /* ENABLE_VFS_FTP */

void load_keymap_defs (void);
void free_keymap_defs (void);

typedef enum
{
    QSEARCH_CASE_INSENSITIVE = 0,       /* quick search in case insensitive mode */
    QSEARCH_CASE_SENSITIVE   = 1,       /* quick search in case sensitive mode */
    QSEARCH_PANEL_CASE       = 2,       /* quick search get value from panel case_sensitive */
    QSEARCH_NUM
} qsearch_mode_t;

/* panels ini options; [Panels] section */
typedef struct
{
    gboolean kilobyte_si;               /* If TRUE, SI units (1000 based) will be used for larger units
                                         * (kilobyte, megabyte, ...). If FALSE, binary units (1024 based) will be used */
    gboolean mix_all_files;             /* If FALSE then directories are shown separately from files */
    gboolean show_backups;              /* If TRUE, show files ending in ~ */
    gboolean show_dot_files;            /* If TRUE, show files starting with a dot */
    gboolean fast_reload;               /* If TRUE then use stat() on the cwd to determine directory changes */
    gboolean fast_reload_msg_shown;     /* Have we shown the fast-reload warning in the past? */
    gboolean mark_moves_down;           /* If TRUE, marking a files moves the cursor down */
    gboolean reverse_files_only;        /* If TRUE, only selection of files is inverted */
    gboolean auto_save_setup;
    gboolean navigate_with_arrows;      /* If TRUE: l&r arrows are used to chdir if the input line is empty */
    gboolean scroll_pages;              /* If TRUE, panel is scrolled by half the display when the cursor reaches
                                           the end or the beginning of the panel */
    gboolean mouse_move_pages;          /* Scroll page/item using mouse wheel */
    gboolean filetype_mode;             /* If TRUE then add per file type hilighting */
    gboolean permission_mode;           /* If TRUE, we use permission hilighting */
    qsearch_mode_t qsearch_mode;        /* Quick search mode */
} panels_options_t;

extern panels_options_t panels_options;

extern panel_view_mode_t startup_left_mode;
extern panel_view_mode_t startup_right_mode;

void panel_load_setup (struct WPanel *panel, const char *section);
void panel_save_setup (struct WPanel *panel, const char *section);
void save_panel_types (void);

void panels_load_options (void);
void panels_save_options (void);

#endif                          /* MC_SETUP_H */
