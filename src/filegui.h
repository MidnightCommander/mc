#ifndef __FILEGUI_H
#define __FILEGUI_H

void fmd_init_i18n (int force);
char *file_mask_dialog (FileOpContext *ctx, FileOperation operation,
			const char *text, const char *def_text, int only_one,
			int *do_background);

#endif
