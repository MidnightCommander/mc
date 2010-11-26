/** \file main.h
 *  \brief Header: this is a main module header
 */

#ifndef MC__MAIN_H
#define MC__MAIN_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/* run mode and params */
typedef enum
{
    MC_RUN_FULL = 0,
    MC_RUN_EDITOR,
    MC_RUN_VIEWER,
    MC_RUN_DIFFVIEWER
} mc_run_mode_t;

enum cd_enum
{
    cd_parse_command,
    cd_exact
};


/*** structures declarations (and typedefs of structures)*****************************************/

struct mc_fhl_struct;

/*** global variables defined in .c file *********************************************************/

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

extern int quit;
/* Set to TRUE to suppress printing the last directory */
extern gboolean print_last_revert;
/* If set, then print to the given file the last directory we were at */
extern char *last_wd_string;

extern struct mc_fhl_struct *mc_filehighlight;

extern int use_internal_view;
extern int use_internal_edit;

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

extern int utf8_display;

extern int midnight_shutdown;

extern char *shell;
extern const char *mc_prompt;

extern char *mc_home;
extern char *mc_home_alt;

extern const char *home_dir;

/*** declarations of public functions ************************************************************/

#ifdef HAVE_SUBSHELL_SUPPORT
int load_prompt (int fd, void *unused);
#endif

int do_cd (const char *new_dir, enum cd_enum cd_type);
void update_xterm_title_path (void);

/*** inline functions ****************************************************************************/

#endif /* MC__MAIN_H */
