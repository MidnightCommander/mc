/** \file mcviewer.h
 *  \brief Header: internal file viewer
 */

#ifndef MC__VIEWER_H
#define MC__VIEWER_H

#include "lib/global.h"
#include "lib/widget.h"         /* WRect */

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

struct WView;
typedef struct WView WView;

typedef struct
{
    gboolean wrap;              /* Wrap text lines to fit them on the screen */
    gboolean hex;               /* Plainview or hexview */
    gboolean magic;             /* Preprocess the file using external programs */
    gboolean nroff;             /* Nroff-style highlighting */
} mcview_mode_flags_t;

/*** global variables defined in .c file *********************************************************/

extern mcview_mode_flags_t mcview_global_flags;
extern mcview_mode_flags_t mcview_altered_flags;

extern gboolean mcview_remember_file_position;
extern int mcview_max_dirt_limit;

extern gboolean mcview_mouse_move_pages;
extern char *mcview_show_eof;

/*** declarations of public functions ************************************************************/

/* Creates a new WView object with the given properties. */
extern WView *mcview_new (const WRect * r, gboolean is_panel);

/* Shows {file} or the output of {command} in the internal viewer,
 * starting in line {start_line}.
 */
extern gboolean mcview_viewer (const char *command, const vfs_path_t * file_vpath, int start_line,
                               off_t search_start, off_t search_end);

extern gboolean mcview_load (WView * view, const char *command, const char *file, int start_line,
                             off_t search_start, off_t search_end);

extern void mcview_clear_mode_flags (mcview_mode_flags_t * flags);

/*** inline functions ****************************************************************************/
#endif /* MC__VIEWER_H */
