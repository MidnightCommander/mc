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

void file_op_context_create_ui (FileOpContext * ctx, gboolean with_eta,
                                filegui_dialog_type_t dialog_type);
void file_op_context_destroy_ui (FileOpContext * ctx);

char *file_mask_dialog (FileOpContext * ctx, FileOperation operation,
                        gboolean only_one,
                        const char *format, const void *text,
                        const char *def_text, gboolean * do_bg);

FileProgressStatus check_progress_buttons (FileOpContext * ctx);

void file_progress_show (FileOpContext * ctx, off_t done, off_t total,
                         const char *stalled_msg, gboolean force_update);
void file_progress_show_count (FileOpContext * ctx, size_t done, size_t total);
void file_progress_show_total (FileOpTotalContext * tctx, FileOpContext * ctx,
                               uintmax_t copied_bytes, gboolean show_summary);
void file_progress_show_source (FileOpContext * ctx, const vfs_path_t * s_vpath);
void file_progress_show_target (FileOpContext * ctx, const vfs_path_t * path);
void file_progress_show_deleting (FileOpContext * ctx, const char *path);

/*** inline functions ****************************************************************************/
#endif /* MC__FILEGUI_H */
