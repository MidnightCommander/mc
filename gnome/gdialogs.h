/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* New dialogs... */
#include "../vfs/vfs.h"
#include "file.h"
#include "panel.h"

FileProgressStatus file_progress_show_source (char *path);
FileProgressStatus file_progress_show_target (char *path);
FileProgressStatus file_progress_show_deleting (char *path);
FileProgressStatus file_progress_show (long done, long total);
FileProgressStatus file_progress_show_count (long done, long total);
FileProgressStatus file_progress_show_bytes (long done, long total);
FileProgressStatus file_progress_real_query_replace (enum OperationMode mode, char *destname, struct stat *_s_stat, struct stat *_d_stat);
void file_progress_set_stalled_label (char *stalled_msg);
char *file_mask_dialog (FileOperation operation, char *text, char *def_text, int only_one, int *do_background);
void create_op_win (FileOperation op, int with_eta);
void destroy_op_win (void);
void fmd_init_i18n();
