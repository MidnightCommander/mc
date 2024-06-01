/** \file  filegui.h
 *  \brief Header: file management GUI for the text mode edition
 */

#ifndef MC__FILEGUI_H
#define MC__FILEGUI_H

#include "lib/global.h"
#include "fileopctx.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

void file_op_context_create_ui (file_op_context_t * ctx, gboolean with_eta,
                                filegui_dialog_type_t dialog_type);
void file_op_context_destroy_ui (file_op_context_t * ctx);

char *file_mask_dialog (file_op_context_t * ctx, gboolean only_one, const char *format,
                        const void *text, const char *def_text, gboolean * do_bg);

FileProgressStatus check_progress_buttons (file_op_context_t * ctx);

void file_progress_show (file_op_context_t * ctx, off_t done, off_t total,
                         const char *stalled_msg, gboolean force_update);
void file_progress_show_count (file_op_context_t * ctx, size_t done, size_t total);
void file_progress_show_total (file_op_total_context_t * tctx, file_op_context_t * ctx,
                               uintmax_t copied_bytes, gboolean show_summary);
void file_progress_show_source (file_op_context_t * ctx, const vfs_path_t * vpath);
void file_progress_show_target (file_op_context_t * ctx, const vfs_path_t * vpath);
gboolean file_progress_show_deleting (file_op_context_t * ctx, const char *path, size_t *count);

/*** inline functions ****************************************************************************/
#endif /* MC__FILEGUI_H */
