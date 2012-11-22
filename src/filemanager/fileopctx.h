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

#ifndef MC__FILEOPCTX_H
#define MC__FILEOPCTX_H

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <inttypes.h>           /* uintmax_t */

#include "lib/global.h"
#include "lib/vfs/vfs.h"


/*** typedefs(not structures) and defined constants **********************************************/

typedef int (*mc_stat_fn) (const vfs_path_t * vpath, struct stat * buf);

/*** enums ***************************************************************************************/

typedef enum
{
    FILEGUI_DIALOG_ONE_ITEM,
    FILEGUI_DIALOG_MULTI_ITEM,
    FILEGUI_DIALOG_DELETE_ITEM
} filegui_dialog_type_t;

typedef enum
{
    OP_COPY = 0,
    OP_MOVE = 1,
    OP_DELETE = 2
} FileOperation;

typedef enum
{
    RECURSIVE_YES = 0,
    RECURSIVE_NO = 1,
    RECURSIVE_ALWAYS = 2,
    RECURSIVE_NEVER = 3,
    RECURSIVE_ABORT = 4
} FileCopyMode;

typedef enum
{
    FILE_CONT = 0,
    FILE_RETRY = 1,
    FILE_SKIP = 2,
    FILE_ABORT = 3,
    FILE_SKIPALL = 4,
    FILE_SUSPEND = 5
} FileProgressStatus;

/* First argument passed to real functions */
enum OperationMode
{
    Foreground,
    Background
};

/*** structures declarations (and typedefs of structures)*****************************************/

struct mc_search_struct;

/* This structure describes a context for file operations.  It is used to update
 * the progress windows and pass around options.
 */
typedef struct FileOpContext
{
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
    filegui_dialog_type_t dialog_type;

    /* Counters for progress indicators */
    size_t progress_count;
    uintmax_t progress_bytes;

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
    gboolean preserve;

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
    gboolean dive_into_subdirs;

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

    /* toggle if all errors should be ignored */
    gboolean skip_all;

    /* Whether the file operation is in pause */
    gboolean suspended;

    /* User interface data goes here */
    void *ui;
} FileOpContext;

typedef struct
{
    size_t progress_count;
    uintmax_t progress_bytes;
    uintmax_t copied_bytes;
    size_t bps;
    size_t bps_count;
    struct timeval transfer_start;
    double eta_secs;

    gboolean ask_overwrite;
} FileOpTotalContext;

/*** global variables defined in .c file *********************************************************/

extern const char *op_names[3];

/*** declarations of public functions ************************************************************/

FileOpContext *file_op_context_new (FileOperation op);
void file_op_context_destroy (FileOpContext * ctx);

FileOpTotalContext *file_op_total_context_new (void);
void file_op_total_context_destroy (FileOpTotalContext * tctx);

/* The following functions are implemented separately by each port */
FileProgressStatus file_progress_real_query_replace (FileOpContext * ctx,
                                                     enum OperationMode mode,
                                                     const char *destname,
                                                     struct stat *_s_stat, struct stat *_d_stat);

/*** inline functions ****************************************************************************/
#endif /* MC__FILEOPCTX_H */
