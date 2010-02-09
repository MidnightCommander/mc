
/** \file dir.h
 *  \brief Header: directory routines
 */

#ifndef MC_DIR_H
#define MC_DIR_H

#include <sys/stat.h>

#include "lib/global.h"

#define MIN_FILES 128
#define RESIZE_STEPS 128

/* keys are set only during sorting */
typedef struct {
    /* File attributes */
    size_t fnamelen;
    char *fname;
    struct stat st;
    /* key used for comparing names */
    char *sort_key;
    /* key used for comparing extensions */
    char *second_sort_key;

    /* Flags */
    struct {
	unsigned int marked:1;		/* File marked in pane window */
	unsigned int link_to_dir:1;	/* If this is a link, does it point to directory? */
	unsigned int stale_link:1;    /* If this is a symlink and points to Charon's land */
	unsigned int dir_size_computed:1; /* Size of directory was computed with dirsizes_cmd */
    } f;
} file_entry;

typedef struct {
    file_entry *list;
    int         size;
} dir_list;

typedef int sortfn (const void *, const void *);

int do_load_dir (const char *path, dir_list * list, sortfn * sort, int reverse,
		 int case_sensitive, int exec_ff, const char *fltr);
void do_sort (dir_list * list, sortfn * sort, int top, int reverse,
	      int case_sensitive, int exec_ff);
int do_reload_dir (const char *path, dir_list * list, sortfn * sort, int count,
		   int reverse, int case_sensitive, int exec_ff, const char *fltr);
void clean_dir (dir_list * list, int count);
gboolean set_zero_dir (dir_list *list);
int handle_path (dir_list *list, const char *path, struct stat *buf1,
		 int next_free, int *link_to_dir, int *stale_link);

/* Sorting functions */
int unsorted   (file_entry *a, file_entry *b);
int sort_name  (file_entry *a, file_entry *b);
int sort_vers  (file_entry *a, file_entry *b);
int sort_ext   (file_entry *a, file_entry *b);
int sort_time  (file_entry *a, file_entry *b);
int sort_atime (file_entry *a, file_entry *b);
int sort_ctime (file_entry *a, file_entry *b);
int sort_size  (file_entry *a, file_entry *b);
int sort_inode (file_entry *a, file_entry *b);


int link_isdir (const file_entry *);
int if_link_is_exe (const char *full_name, const file_entry *file);

extern int show_backups;
extern int show_dot_files;
extern int mix_all_files;
extern int kilobyte_si;

#endif
