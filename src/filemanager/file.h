/** \file  file.h
 *  \brief Header: File and directory operation routines
 */

#ifndef MC__FILE_H
#define MC__FILE_H

#include <inttypes.h>           /* off_t, uintmax_t */

#include "lib/global.h"
#include "lib/widget.h"

#include "fileopctx.h"

/*** typedefs(not structures) and defined constants **********************************************/

/* Compute directory size */
/* callback to update status dialog */
typedef FileProgressStatus (*compute_dir_size_callback) (void *ui, const vfs_path_t * dirname_vpath,
                                                         size_t dir_count, uintmax_t total_size);

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/* status dialog of directory size computing */
typedef struct
{
    WDialog *dlg;
    WLabel *dirname;
} ComputeDirSizeUI;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

FileProgressStatus copy_file_file (FileOpTotalContext * tctx, FileOpContext * ctx,
                                   const char *src_path, const char *dst_path);
FileProgressStatus move_dir_dir (FileOpTotalContext * tctx, FileOpContext * ctx,
                                 const char *s, const char *d);
FileProgressStatus copy_dir_dir (FileOpTotalContext * tctx, FileOpContext * ctx,
                                 const char *s, const char *d,
                                 gboolean toplevel, gboolean move_over, gboolean do_delete,
                                 GSList * parent_dirs);
FileProgressStatus erase_dir (FileOpTotalContext * tctx, FileOpContext * ctx,
                              const vfs_path_t * vpath);

gboolean panel_operate (void *source_panel, FileOperation op, gboolean force_single);

/* Error reporting routines */

/* Report error with one file */
FileProgressStatus file_error (const char *format, const char *file);

/* return value is FILE_CONT or FILE_ABORT */
FileProgressStatus compute_dir_size (const vfs_path_t * dirname_vpath, void *ui,
                                     compute_dir_size_callback cback,
                                     size_t * ret_marked, uintmax_t * ret_total,
                                     gboolean compute_symlinks);

ComputeDirSizeUI *compute_dir_size_create_ui (gboolean allow_skip);
void compute_dir_size_destroy_ui (ComputeDirSizeUI * ui);
FileProgressStatus compute_dir_size_update_ui (void *ui, const vfs_path_t * dirname_vpath,
                                               size_t dir_count, uintmax_t total_size);

/*** inline functions ****************************************************************************/
#endif /* MC__FILE_H */
