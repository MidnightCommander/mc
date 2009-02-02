#ifndef MC_VIEW_H
#define MC_VIEW_H

typedef struct WView WView;	/* Can be cast to Widget */

/* Creates a new WView object with the given properties. Caveat: the
 * origin is in y-x order, while the extent is in x-y order. */
extern WView *view_new (int y, int x, int cols, int lines, int is_panel);

/* If {command} is not NULL, loads the output of the shell command
 * {command} and ignores {file}. If {command} is NULL, loads the
 * {file}. If the {file} is also NULL, loads nothing. If {start_line}
 * is positive, the output is shown starting in that line. */
extern bool view_load (WView *view, const char *command, const char *file,
	int start_line);

/* Shows {file} or the output of {command} in the internal viewer,
 * starting in line {start_line}. {ret_move_direction} may be NULL or
 * point to a variable that will receive the direction in which the user
 * wants to move (-1 = previous file, 1 = next file, 0 = do nothing).
 */
extern int mc_internal_viewer (const char *command, const char *file,
	int *ret_move_direction, int start_line);

extern int mouse_move_pages_viewer;
extern int max_dirt_limit;
extern int global_wrap_mode;
extern int default_hex_mode;
extern int default_magic_flag;
extern int default_nroff_flag;
extern int altered_hex_mode;
extern int altered_magic_flag;
extern int altered_nroff_flag;
extern int mcview_remember_file_position;

#endif
