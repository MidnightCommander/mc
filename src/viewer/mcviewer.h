/** \file mcviewer.h
 *  \brief Header: internal file viewer
 */

#ifndef MC__VIEWER_H
#define MC__VIEWER_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

struct mcview_struct;

typedef enum
{
    MCVIEW_EXIT_FAILURE = -1,
    MCVIEW_EXIT_OK = 0,
    MCVIEW_WANT_NEXT,
    MCVIEW_WANT_PREV
} mcview_ret_t;

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

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
extern char *mcview_show_eof;

/*** declarations of public functions ************************************************************/

/* Creates a new mcview_t object with the given properties. Caveat: the
 * origin is in y-x order, while the extent is in x-y order. */
extern struct mcview_struct *mcview_new (int y, int x, int lines, int cols, gboolean is_panel);


/* Shows {file} or the output of {command} in the internal viewer,
 * starting in line {start_line}.
 */
extern mcview_ret_t mcview_viewer (const char *command, const vfs_path_t * file_vpath,
                                   int start_line);

extern gboolean mcview_load (struct mcview_struct *, const char *, const char *, int);

/*** inline functions ****************************************************************************/
#endif /* MC__VIEWER_H */
