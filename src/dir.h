
/** \file dir.h
 *  \brief Header: directory routines
 */

#ifndef MC_DIR_H
#define MC_DIR_H

#define MIN_FILES 128
#define RESIZE_STEPS 128

#include <sys/stat.h>

typedef struct {

    /* File attributes */

    int  fnamelen;
    char *fname;
    struct stat st;

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
		 int case_sensitive, int exec_ff, const char *filter);
void do_sort (dir_list * list, sortfn * sort, int top, int reverse,
	      int case_sensitive, int exec_ff);
int do_reload_dir (const char *path, dir_list * list, sortfn * sort, int count,
		   int reverse, int case_sensitive, int exec_ff, const char *filter);
void clean_dir (dir_list * list, int count);
int set_zero_dir (dir_list * list);
int handle_path (dir_list *list, const char *path, struct stat *buf1,
		 int next_free, int *link_to_dir, int *stale_link);

/* Sorting functions */
int unsorted   (const file_entry *a, const file_entry *b);
int sort_name  (const file_entry *a, const file_entry *b);
int sort_ext   (const file_entry *a, const file_entry *b);
int sort_time  (const file_entry *a, const file_entry *b);
int sort_atime (const file_entry *a, const file_entry *b);
int sort_ctime (const file_entry *a, const file_entry *b);
int sort_size  (const file_entry *a, const file_entry *b);
int sort_inode (const file_entry *a, const file_entry *b);

/* SORT_TYPES is used to build the nice dialog box entries */
#define SORT_TYPES 8

/* This is the number of sort types not available in that dialog box */
#define SORT_TYPES_EXTRA 0

/* The total nnumber of sort types */
#define SORT_TYPES_TOTAL (SORT_TYPES + SORT_TYPES_EXTRA)

typedef struct {
    const char    *sort_name;
    int     (*sort_fn)(const file_entry *, const file_entry *);
} sort_orders_t;

extern sort_orders_t sort_orders [SORT_TYPES_TOTAL];

int link_isdir (const file_entry *);
int if_link_is_exe (const char *full_name, const file_entry *file);

extern int show_backups;
extern int show_dot_files;
extern int mix_all_files;

#endif
