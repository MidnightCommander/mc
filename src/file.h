#ifndef __FILE_H
#define __FILE_H

#include "background.h"

typedef enum {
	OP_COPY,
	OP_MOVE,
	OP_DELETE
} FileOperation;

extern char *op_names [3];

typedef enum {
	FILE_CONT,
	FILE_RETRY,
	FILE_SKIP,
	FILE_ABORT
} FileProgressStatus;

enum {
    RECURSIVE_YES,
    RECURSIVE_NO,
    RECURSIVE_ALWAYS,
    RECURSIVE_NEVER,
    RECURSIVE_ABORT
} FileCopyMode;

extern int know_not_what_am_i_doing;

struct link;

int copy_file_file      (char *s, char *d, int ask_overwrite,
			 long *progres_count, double *progress_bytes, 
                         int is_toplevel_file);
int move_file_file      (char *s, char *d,
			 long *progres_count, double *progress_bytes);
int move_dir_dir        (char *s, char *d,
			 long *progres_count, double *progress_bytes);
int copy_dir_dir        (char *s, char *d, int toplevel, int move_over,
			 int delete, struct link *parent_dirs,
			 long *progres_count, double *progress_bytes);
int erase_dir           (char *s, long *progres_count, double *progress_bytes);
int erase_file          (char *s, long *progress_count, double *progress_bytes, int is_toplevel_file);
int erase_dir_iff_empty (char *s);

/*
 * Manually creating the copy/move/delte dialogs
 */
void  create_op_win      (FileOperation op, int with_eta);
void  destroy_op_win     (void);
void  refresh_op_win     (void);
int   panel_operate      (void *source_panel, FileOperation op,
			  char *thedefault);
int   panel_operate_def  (void *source_panel, FileOperation op,
			  char *thedefault);
void  file_mask_defaults (void);
char *file_mask_dialog   (FileOperation operation, char *text, char *def_text,
			  int only_one, int *do_background);

#ifdef WANT_WIDGETS
char *panel_operate_generate_prompt (char* cmd_buf,
				     WPanel* panel,
				     int operation, int only_one,
				     struct stat* src_stat);
#endif

extern int dive_into_subdirs;
extern int file_op_compute_totals;

/* Error reporting routines */
    /* Skip/Retry/Abort routine */
    int do_file_error (char *error);

    /* Report error with one file */
    int file_error (char *format, char *file);

    /* Report error with two files */
    int files_error (char *format, char *file1, char *file2);

    /* This one just displays buf */
    int do_file_error (char *buf);

/* Query routines */
    /* Replace existing file */
    int query_replace (char *destname, struct stat *_s_stat, struct stat *_d_stat);

    /* Query recursive delete */
    int query_recursive (char *s);

/* Callback routine for background activity */
int background_attention (int fd, void *info);

extern int background_wait;

int is_wildcarded (char *p);
void compute_dir_size (char *dirname, long *ret_marked, double *ret_total);
#endif


