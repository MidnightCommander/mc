/** \file dir.h
 *  \brief Header: directory routines
 */

#ifndef MC__DIR_H
#define MC__DIR_H

#include <sys/stat.h>

#include "lib/global.h"
#include "lib/util.h"
#include "lib/vfs/vfs.h"

/*** typedefs(not structures) and defined constants **********************************************/

#define DIR_LIST_MIN_SIZE 128
#define DIR_LIST_RESIZE_STEP 128

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/**
 * A structure to represent directory content
 */
typedef struct
{
    file_entry_t *list; /**< list of file_entry_t objects */
    int size;           /**< number of allocated elements in list (capacity) */
    int len;            /**< number of used elements in list */
} dir_list;

/**
 * A structure to represent sort options for directory content
 */
typedef struct dir_sort_options_struct
{
    gboolean reverse;           /**< sort is reverse */
    gboolean case_sensitive;    /**< sort is case sensitive */
    gboolean exec_first;        /**< executables are at top of list */
} dir_sort_options_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

gboolean dir_list_grow (dir_list * list, int delta);
gboolean dir_list_append (dir_list * list, const char *fname, const struct stat *st,
                          gboolean link_to_dir, gboolean stale_link);

void dir_list_load (dir_list * list, const vfs_path_t * vpath, GCompareFunc sort,
                    const dir_sort_options_t * sort_op, const char *fltr);
void dir_list_reload (dir_list * list, const vfs_path_t * vpath, GCompareFunc sort,
                      const dir_sort_options_t * sort_op, const char *fltr);
void dir_list_sort (dir_list * list, GCompareFunc sort, const dir_sort_options_t * sort_op);
gboolean dir_list_init (dir_list * list);
void dir_list_clean (dir_list * list);
gboolean handle_path (const char *path, struct stat *buf1, int *link_to_dir, int *stale_link);

/* Sorting functions */
int unsorted (file_entry_t * a, file_entry_t * b);
int sort_name (file_entry_t * a, file_entry_t * b);
int sort_vers (file_entry_t * a, file_entry_t * b);
int sort_ext (file_entry_t * a, file_entry_t * b);
int sort_time (file_entry_t * a, file_entry_t * b);
int sort_atime (file_entry_t * a, file_entry_t * b);
int sort_ctime (file_entry_t * a, file_entry_t * b);
int sort_size (file_entry_t * a, file_entry_t * b);
int sort_inode (file_entry_t * a, file_entry_t * b);

gboolean if_link_is_exe (const vfs_path_t * full_name, const file_entry_t * file);

/*** inline functions ****************************************************************************/

static inline gboolean
link_isdir (const file_entry_t * file)
{
    return (gboolean) file->f.link_to_dir;
}

#endif /* MC__DIR_H */
