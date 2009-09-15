#ifndef MC__ARGS_H
#define MC__ARGS_H

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

extern gboolean mc_args__force_xterm;
extern gboolean mc_args__nomouse;
extern gboolean mc_args__slow_terminal;
extern gboolean mc_args__ugly_line_drawing;
extern gboolean mc_args__disable_colors;
extern gboolean mc_args__force_colors;
extern char *mc_args__skin;
extern gboolean mc_args__version;
extern char *mc_args__last_wd_file;
extern char *mc_args__netfs_logfile;
extern char *mc_args__keymap_file;
extern int mc_args__debug_level;

/*** declarations of public functions ************************************************************/

gboolean mc_args_handle(int *, char ***, const gchar *);

#endif
