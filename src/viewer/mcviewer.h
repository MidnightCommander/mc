/** \file mcviewer.h
 *  \brief Header: internal file viewer
 */

#ifndef MC__VIEWER_H
#define MC__VIEWER_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

struct WView;
typedef struct WView WView;

/*** global variables defined in .c file *********************************************************/

extern int mcview_default_hex_mode;
extern int mcview_default_nroff_flag;
extern gboolean mcview_global_wrap_mode;
extern int mcview_default_magic_flag;

extern int mcview_altered_hex_mode;
extern int mcview_altered_magic_flag;
extern int mcview_altered_nroff_flag;

extern gboolean mcview_remember_file_position;
extern int mcview_max_dirt_limit;

extern gboolean mcview_mouse_move_pages;
extern char *mcview_show_eof;

/*** declarations of public functions ************************************************************/

/* Creates a new WView object with the given properties. Caveat: the
 * origin is in y-x order, while the extent is in x-y order. */
extern WView *mcview_new (int y, int x, int lines, int cols, gboolean is_panel);


/* Shows {file} or the output of {command} in the internal viewer,
 * starting in line {start_line}.
 */
extern gboolean mcview_viewer (const char *command, const vfs_path_t * file_vpath, int start_line,
                               off_t search_start, off_t search_end);

extern gboolean mcview_load (WView * view, const char *command, const char *file, int start_line,
                             off_t search_start, off_t search_end);

/*** inline functions ****************************************************************************/
#endif /* MC__VIEWER_H */
