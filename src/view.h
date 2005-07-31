#ifndef MC_VIEW_H
#define MC_VIEW_H

struct WView;			/* opaque structure, can be cast to Widget */
typedef struct WView WView;

/* Creation/initialization of a new view widget */
WView *view_new (int y, int x, int cols, int lines, int is_panel);
int view_load (WView *view, const char *_command, const char *_file,
	       int start_line);

/* View a ''file'' or the output of a ''command'' in the internal viewer,
 * starting in line ''start_line''. ''ret_move_direction'' may be NULL or
 * point to a variable that will receive the direction in which the user
 * wants to move (-1 = previous file, 1 = next file, 0 = do nothing).
 */
int mc_internal_viewer (const char *command, const char *file,
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
