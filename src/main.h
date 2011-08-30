/** \file main.h
 *  \brief Header: this is a main module header
 */

#ifndef MC__MAIN_H
#define MC__MAIN_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

#define MAX_MACRO_LENGTH 1024

/*** enums ***************************************************************************************/

/* run mode and params */

enum cd_enum
{
    cd_parse_command,
    cd_exact
};

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct macro_action_t
{
    unsigned long action;
    int ch;
} macro_action_t;

typedef struct macros_t
{
    int hotkey;
    GArray *macro;
} macros_t;

struct mc_fhl_struct;

/*** global variables defined in .c file *********************************************************/

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
extern int default_source_codepage;
extern char *autodetect_codeset;
extern gboolean is_autodetect_codeset_enabled;
#endif /* !HAVE_CHARSET */

extern char *shell;
extern const char *mc_prompt;

/* index to record_macro_buf[], -1 if not recording a macro */
extern int macro_index;

/* macro stuff */
extern struct macro_action_t record_macro_buf[MAX_MACRO_LENGTH];

extern GArray *macros_list;

/*** declarations of public functions ************************************************************/

#ifdef HAVE_SUBSHELL_SUPPORT
int load_prompt (int fd, void *unused);
#endif

int do_cd (const char *new_dir, enum cd_enum cd_type);
void update_xterm_title_path (void);

/*** inline functions ****************************************************************************/

#endif /* MC__MAIN_H */
