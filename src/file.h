#ifndef MC_FILE_H
#define MC_FILE_H

#include "fileopctx.h"

extern int safe_delete;

struct link;

int copy_file_file (FileOpContext *ctx, const char *s, const char *d,
		    int ask_overwrite, off_t *progress_count,
		    double *progress_bytes, int is_toplevel_file);
int move_dir_dir (FileOpContext *ctx, const char *s, const char *d,
		  off_t *progress_count, double *progress_bytes);
int copy_dir_dir (FileOpContext *ctx, const char *s, const char *d, int toplevel,
		  int move_over, int delete, struct link *parent_dirs,
		  off_t *progress_count, double *progress_bytes);
int erase_dir (FileOpContext *ctx, const char *s, off_t *progress_count,
	       double *progress_bytes);

int panel_operate (void *source_panel, FileOperation op, int force_single);

extern int file_op_compute_totals;

/* Error reporting routines */

/* Report error with one file */
int file_error (const char *format, const char *file);

/* Query routines */

void compute_dir_size (const char *dirname, off_t *ret_marked,
		       double *ret_total);

#endif
