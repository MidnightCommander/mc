/** \file mcviewer.h
 *  \brief Header: internal file viewer
 */

#ifndef MC_VIEWER_H
#define MC_VIEWER_H

/*** typedefs(not structures) and defined constants ********************/

struct mcview_struct;

/*** enums *************************************************************/

/*** structures declarations (and typedefs of structures)***************/

/*** global variables defined in .c file *******************************/

extern int mcview_default_hex_mode;
extern int mcview_default_nroff_flag;
extern int mcview_global_wrap_mode;
extern int mcview_default_magic_flag;

extern int mcview_altered_hex_mode;
extern int mcview_altered_magic_flag;
extern int mcview_altered_nroff_flag;

extern int mcview_remember_file_position;
extern int mcview_max_dirt_limit;

extern int mcview_mouse_move_pages;

/*** declarations of public functions **********************************/


/* Creates a new mcview_t object with the given properties. Caveat: the
 * origin is in y-x order, while the extent is in x-y order. */
extern struct mcview_struct *mcview_new (int y, int x, int lines, int cols, int is_panel);


/* Shows {file} or the output of {command} in the internal viewer,
 * starting in line {start_line}. {move_dir_p} may be NULL or
 * point to a variable that will receive the direction in which the user
 * wants to move (-1 = previous file, 1 = next file, 0 = do nothing).
 */
extern int mcview_viewer (const char *command, const char *file, int *move_dir_p, int start_line);

extern gboolean mcview_load (struct mcview_struct *, const char *, const char *, int);

#endif
