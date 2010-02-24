
/** \file  filegui.h
 *  \brief Header: file management GUI for the text mode edition
 */

#ifndef MC_FILEGUI_H
#define MC_FILEGUI_H

#include "lib/global.h"
#include "fileopctx.h"

typedef enum {
    FILEGUI_DIALOG_ONE_ITEM,
    FILEGUI_DIALOG_MULTI_ITEM,
    FILEGUI_DIALOG_DELETE_ITEM
} filegui_dialog_type_t;

void file_op_context_create_ui (FileOpContext *ctx, gboolean with_eta, filegui_dialog_type_t dialog_type);
void file_op_context_create_ui_without_init (FileOpContext *ctx, gboolean with_eta, filegui_dialog_type_t dialog_type);
void file_op_context_destroy_ui (FileOpContext *ctx);


char *file_mask_dialog (FileOpContext *ctx, FileOperation operation,
			gboolean only_one,
			const char *format, const void *text,
			const char *def_text, gboolean *do_background);

FileProgressStatus check_progress_buttons (FileOpContext *ctx);

void file_progress_show (FileOpContext *ctx, off_t done, off_t total,
			 const char *stalled_msg, gboolean force_update);
void file_progress_show_count (FileOpContext *ctx, off_t done, off_t total);
void file_progress_show_total (FileOpTotalContext *tctx, FileOpContext *ctx,
			       double copyed_bytes, gboolean need_show_total_summary);
void file_progress_show_source (FileOpContext *ctx, const char *path);
void file_progress_show_target (FileOpContext *ctx, const char *path);
void file_progress_show_deleting (FileOpContext *ctx, const char *path);

#endif				/* MC_FILEGUI_H */
