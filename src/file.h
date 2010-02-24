
/** \file  file.h
 *  \brief Header: File and directory operation routines
 */

#ifndef MC_FILE_H
#define MC_FILE_H

#include <sys/types.h>		/* off_t */
#include <sys/time.h>

#include "lib/global.h"
#include "dialog.h"		/* Dlg_head */
#include "widget.h"		/* WLabel */
#include "fileopctx.h"

struct link;

FileProgressStatus copy_file_file (FileOpTotalContext *tctx, FileOpContext *ctx,
				   const char *src_path, const char *dst_path);
FileProgressStatus move_dir_dir (FileOpTotalContext *tctx, FileOpContext *ctx,
				 const char *s, const char *d);
FileProgressStatus copy_dir_dir (FileOpTotalContext *tctx, FileOpContext *ctx,
				 const char *s, const char *d, 
				 gboolean toplevel, gboolean move_over, gboolean do_delete,
				 struct link *parent_dirs);
FileProgressStatus erase_dir (FileOpTotalContext *tctx, FileOpContext *ctx,
			      const char *s);

gboolean panel_operate (void *source_panel, FileOperation op, gboolean force_single);

extern int file_op_compute_totals;

/* Error reporting routines */

/* Report error with one file */
FileProgressStatus file_error (const char *format, const char *file);

/* Compute directory size */
/* callback to update status dialog */
typedef FileProgressStatus (*compute_dir_size_callback)(const void *ui, const char *dirname);

/* return value is FILE_CONT or FILE_ABORT */
FileProgressStatus compute_dir_size (const char *dirname, const void *ui,
					compute_dir_size_callback cback,
					off_t *ret_marked, double *ret_total);

/* status dialog of directory size computing */
typedef struct {
    Dlg_head *dlg;
    WLabel *dirname;
} ComputeDirSizeUI;

ComputeDirSizeUI *compute_dir_size_create_ui (void);
void compute_dir_size_destroy_ui (ComputeDirSizeUI *ui);
FileProgressStatus compute_dir_size_update_ui (const void *ui, const char *dirname);

#endif				/* MC_FILE_H */
