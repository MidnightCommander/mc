
/** \file  filegui.h
 *  \brief Header: file management GUI for the text mode edition
 */

#ifndef MC_FILEGUI_H
#define MC_FILEGUI_H

#include "lib/global.h"
#include "fileopctx.h"

char *file_mask_dialog (FileOpContext *ctx, FileOperation operation,
			gboolean only_one,
			const char *format, const void *text,
			const char *def_text, gboolean *do_background);

FileProgressStatus check_progress_buttons (FileOpContext *ctx);

void file_progress_show (FileOpContext *ctx, off_t done, off_t total);
void file_progress_show_count (FileOpContext *ctx, off_t done, off_t total);
void file_progress_show_bytes (FileOpContext *ctx, double done, double total);
void file_progress_show_total (FileOpTotalContext *tctx, FileOpContext *ctx);
void file_progress_show_source (FileOpContext *ctx, const char *path);
void file_progress_show_target (FileOpContext *ctx, const char *path);
void file_progress_show_deleting (FileOpContext *ctx, const char *path);

#endif				/* MC_FILEGUI_H */
