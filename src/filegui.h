
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

#endif				/* MC_FILEGUI_H */
