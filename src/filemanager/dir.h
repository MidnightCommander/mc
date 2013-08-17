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

#define MIN_FILES 128
#define RESIZE_STEPS 128

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    file_entry *list;
    int size;
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

int do_load_dir (const vfs_path_t * vpath, dir_list * list, GCompareFunc sort,
                 const dir_sort_options_t * sort_op, const char *fltr);
void do_sort (dir_list * list, GCompareFunc sort, int top, const dir_sort_options_t * sort_op);
int do_reload_dir (const vfs_path_t * vpath, dir_list * list, GCompareFunc sort, int count,
                   const dir_sort_options_t * sort_op, const char *fltr);
void clean_dir (dir_list * list, int count);
gboolean set_zero_dir (dir_list * list);
int handle_path (dir_list * list, const char *path, struct stat *buf1,
                 int next_free, int *link_to_dir, int *stale_link);

/* Sorting functions */
int unsorted (file_entry * a, file_entry * b);
int sort_name (file_entry * a, file_entry * b);
int sort_vers (file_entry * a, file_entry * b);
int sort_ext (file_entry * a, file_entry * b);
int sort_time (file_entry * a, file_entry * b);
int sort_atime (file_entry * a, file_entry * b);
int sort_ctime (file_entry * a, file_entry * b);
int sort_size (file_entry * a, file_entry * b);
int sort_inode (file_entry * a, file_entry * b);

gboolean if_link_is_exe (const vfs_path_t * full_name, const file_entry * file);

/*** inline functions ****************************************************************************/

static inline gboolean
link_isdir (const file_entry * file)
{
    return (gboolean) file->f.link_to_dir;
}

#endif /* MC__DIR_H */
