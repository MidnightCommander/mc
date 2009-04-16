
/** \file  filegui.h
 *  \brief Header: file management GUI for the text mode edition
 */

#ifndef MC_FILEGUI_H
#define MC_FILEGUI_H

#include "fileopctx.h"

void fmd_init_i18n (int force);
char *file_mask_dialog (FileOpContext *ctx, FileOperation operation,
			const char *text, const char *def_text, int only_one,
			int *do_background);

#endif
