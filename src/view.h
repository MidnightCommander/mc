#ifndef MC_VIEW_H
#define MC_VIEW_H

struct WView;			/* opaque structure, can be cast to Widget */
typedef struct WView WView;

/* Creation/initialization of a new view widget */
WView *view_new (int y, int x, int cols, int lines, int is_panel);
int view_init (WView *view, const char *_command, const char *_file,
	       int start_line);

void view_update_bytes_per_line (WView *view);

/* Command: view a _file, if _command != NULL we use popen on _command */
/* move direction should be a pointer that will hold the direction in which */
/* the user wants to move (-1 previous file, 1 next file, 0 do nothing) */
int view (const char *_command, const char *_file, int *move_direction,
	  int start_line);

extern int mouse_move_pages_viewer;
extern int max_dirt_limit;
extern int global_wrap_mode;
extern int have_fast_cpu;
extern int default_hex_mode;
extern int default_magic_flag;
extern int default_nroff_flag;
extern int altered_hex_mode;
extern int altered_magic_flag;
extern int altered_nroff_flag;

#endif
