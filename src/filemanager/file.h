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

typedef struct dirsize_status_msg_t dirsize_status_msg_t;

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/* status dialog of directory size computing */
struct dirsize_status_msg_t
{
    status_msg_t status_msg;    /* base class */

    gboolean allow_skip;
    WLabel *dirname;
    WLabel *count_size;
    Widget *abort_button;
    Widget *skip_button;
    const vfs_path_t *dirname_vpath;
    size_t dir_count;
    uintmax_t total_size;
};

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

gboolean file_is_symlink_to_dir (const vfs_path_t * path, struct stat *st, gboolean * stale_link);

FileProgressStatus copy_file_file (file_op_total_context_t * tctx, file_op_context_t * ctx,
                                   const char *src_path, const char *dst_path);
FileProgressStatus move_dir_dir (file_op_total_context_t * tctx, file_op_context_t * ctx,
                                 const char *s, const char *d);
FileProgressStatus copy_dir_dir (file_op_total_context_t * tctx, file_op_context_t * ctx,
                                 const char *s, const char *d,
                                 gboolean toplevel, gboolean move_over, gboolean do_delete,
                                 GSList * parent_dirs);
FileProgressStatus erase_dir (file_op_total_context_t * tctx, file_op_context_t * ctx,
                              const vfs_path_t * vpath);

gboolean panel_operate (void *source_panel, FileOperation op, gboolean force_single);

/* Error reporting routines */

/* Report error with one file */
FileProgressStatus file_error (gboolean allow_retry, const char *format, const char *file);

/* return value is FILE_CONT or FILE_ABORT */
FileProgressStatus compute_dir_size (const vfs_path_t * dirname_vpath, dirsize_status_msg_t * sm,
                                     size_t *ret_dir_count, size_t *ret_marked_count,
                                     uintmax_t * ret_total, gboolean follow_symlinks);

void dirsize_status_init_cb (status_msg_t * sm);
int dirsize_status_update_cb (status_msg_t * sm);
void dirsize_status_deinit_cb (status_msg_t * sm);

/*** inline functions ****************************************************************************/
#endif /* MC__FILE_H */
