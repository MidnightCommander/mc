#ifndef MC__ARGS_H
#define MC__ARGS_H

#include "lib/global.h"         /* gboolean */
#include "lib/vfs/vfs.h"        /* vfs_path_t */

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    vfs_path_t *file_vpath;
    int line_number;
} mcedit_arg_t;

/*** global variables defined in .c file *********************************************************/

extern gboolean mc_args__force_xterm;
extern gboolean mc_args__nomouse;
extern gboolean mc_args__force_colors;
extern gboolean mc_args__nokeymap;
extern char *mc_args__last_wd_file;
extern char *mc_args__netfs_logfile;
extern char *mc_args__keymap_file;
extern int mc_args__debug_level;

/*** declarations of public functions ************************************************************/

gboolean mc_args_parse (int *argc, char ***argv, const char *translation_domain, GError ** error);
gboolean mc_args_show_info (void);
gboolean mc_setup_by_args (int argc, char **argv, GError ** error);

mcedit_arg_t *mcedit_arg_new (const char *file_name, int line_number);
mcedit_arg_t *mcedit_arg_vpath_new (vfs_path_t * file_vpath, int line_number);
void mcedit_arg_free (mcedit_arg_t * arg);

/*** inline functions ****************************************************************************/

#endif /* MC__ARGS_H */
