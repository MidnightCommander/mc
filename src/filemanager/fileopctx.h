/** \file fileopctx.h
 *  \brief Header: file operation contexts
 *  \date 1998
 *  \author Federico Mena <federico@nuclecu.unam.mx>
 *  \author Miguel de Icaza <miguel@nuclecu.unam.mx>
 */

#ifndef MC__FILEOPCTX_H
#define MC__FILEOPCTX_H

#include <sys/stat.h>
#include <sys/types.h>
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

/* ATTENTION: avoid overlapping with B_* values (lib/widget/dialog.h) */
typedef enum
{
    FILE_CONT = 10,
    FILE_RETRY,
    FILE_SKIP,
    FILE_ABORT,
    FILE_SKIPALL,
    FILE_SUSPEND
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
typedef struct
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
    mode_t umask_kill;

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
} file_op_context_t;

typedef struct
{
    size_t progress_count;
    size_t prev_progress_count; /* Used in OP_MOVE between copy and remove directories */
    uintmax_t progress_bytes;
    uintmax_t copied_bytes;
    size_t bps;
    size_t bps_count;
    gint64 transfer_start;
    double eta_secs;

    gboolean ask_overwrite;
} file_op_total_context_t;

/*** global variables defined in .c file *********************************************************/

extern const char *op_names[3];

/*** declarations of public functions ************************************************************/

file_op_context_t *file_op_context_new (FileOperation op);
void file_op_context_destroy (file_op_context_t * ctx);

file_op_total_context_t *file_op_total_context_new (void);
void file_op_total_context_destroy (file_op_total_context_t * tctx);

/* The following functions are implemented separately by each port */
FileProgressStatus file_progress_real_query_replace (file_op_context_t * ctx,
                                                     enum OperationMode mode, const char *src,
                                                     struct stat *src_stat, const char *dst,
                                                     struct stat *dst_stat);

/*** inline functions ****************************************************************************/
#endif /* MC__FILEOPCTX_H */
