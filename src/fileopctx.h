
/** \file fileopctx.h
 *  \brief Header: file operation contexts
 *  \date 1998
 *  \author Federico Mena <federico@nuclecu.unam.mx>
 *  \author Miguel de Icaza <miguel@nuclecu.unam.mx>
 */

/* File operation contexts for the Midnight Commander
 *
 * Copyright (C) 1998 Free Software Foundation, Inc.
 *
 */

#ifndef FILEOPCTX_H
#define FILEOPCTX_H

#include <sys/stat.h>
#include <sys/types.h>

#include "lib/global.h"

struct mc_search_struct;

typedef enum {
	OP_COPY   = 0,
	OP_MOVE   = 1,
	OP_DELETE = 2
} FileOperation;

typedef enum {
    RECURSIVE_YES    = 0,
    RECURSIVE_NO     = 1,
    RECURSIVE_ALWAYS = 2,
    RECURSIVE_NEVER  = 3,
    RECURSIVE_ABORT  = 4
} FileCopyMode;

typedef int (*mc_stat_fn) (const char *filename, struct stat *buf);

/* This structure describes a context for file operations.  It is used to update
 * the progress windows and pass around options.
 */
typedef struct FileOpContext {
	/* Operation type (copy, move, delete) */
	FileOperation operation;

	/* The estimated time of arrival in seconds */
	double eta_secs;

	/* Transferred bytes per second */
	long bps;

	/* Transferred seconds */
	long bps_time;

	/* Whether the panel total has been computed */
	gboolean progress_totals_computed;

	/* Counters for progress indicators */
	off_t progress_count;
	double progress_bytes;

	/* The value of the "preserve Attributes" checkbox in the copy file dialog.
	 * We can't use the value of "ctx->preserve" because it can change in order
	 * to preserve file attributs when moving files across filesystem boundaries
	 * (we want to keep the value of the checkbox between copy operations).
	 */
	gboolean op_preserve;

	/* Result from the recursive query */
	FileCopyMode recursive_result;

	/* Whether to do a reget */
	off_t do_reget;

	/* Controls appending to files */
	gboolean do_append;

	/* Whether to stat or lstat */
	gboolean follow_links;

	/* Pointer to the stat function we will use */
	mc_stat_fn stat_func;

	/* Whether to recompute symlinks */
	gboolean stable_symlinks;

	/* Preserve the original files' owner, group, permissions, and
	 * timestamps (owner, group only as root).
	 */
	int preserve;

	/* If running as root, preserve the original uid/gid (we don't want to
	 * try chown for non root) preserve_uidgid = preserve && uid == 0
	 */
	gboolean preserve_uidgid;

	/* The bits to preserve in created files' modes on file copy */
	int umask_kill;

	/* The mask of files to actually operate on */
	char *dest_mask;

	/* search handler */
	struct mc_search_struct *search_handle;

	/* Whether to dive into subdirectories for recursive operations */
	int dive_into_subdirs;

	/* When moving directories cross filesystem boundaries delete the
	 * successfully copied files when all files below the directory and its
	 * subdirectories were processed.
	 *
	 * If erase_at_end is FALSE files will be deleted immediately after their
	 * successful copy (Note: this behavior is not tested and at the moment
	 * it can't be changed at runtime).
	 */
	gboolean erase_at_end;

	/* PID of the child for background operations */
	pid_t pid;

	/* User interface data goes here */
	void *ui;
} FileOpContext;


FileOpContext *file_op_context_new (FileOperation op);
void file_op_context_destroy (FileOpContext *ctx);


extern const char *op_names [3];

typedef enum {
	FILE_CONT  = 0,
	FILE_RETRY = 1,
	FILE_SKIP  = 2,
	FILE_ABORT = 3
} FileProgressStatus;

/* First argument passed to real functions */
enum OperationMode {
    Foreground,
    Background
};

/* The following functions are implemented separately by each port */

void file_op_context_create_ui (FileOpContext *ctx, gboolean with_eta);
void file_op_context_create_ui_without_init (FileOpContext *ctx, gboolean with_eta);
void file_op_context_destroy_ui (FileOpContext *ctx);

FileProgressStatus file_progress_show (FileOpContext *ctx, off_t done, off_t total);
FileProgressStatus file_progress_show_count (FileOpContext *ctx, off_t done, off_t total);
FileProgressStatus file_progress_show_bytes (FileOpContext *ctx, double done, double total);
FileProgressStatus file_progress_show_source (FileOpContext *ctx, const char *path);
FileProgressStatus file_progress_show_target (FileOpContext *ctx, const char *path);
FileProgressStatus file_progress_show_deleting (FileOpContext *ctx, const char *path);

void file_progress_set_stalled_label (FileOpContext *ctx, const char *stalled_msg);

FileProgressStatus file_progress_real_query_replace (FileOpContext *ctx,
						     enum OperationMode mode,
						     const char *destname,
						     struct stat *_s_stat,
						     struct stat *_d_stat);


#endif
