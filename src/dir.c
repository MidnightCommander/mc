/* Directory routines
   Copyright (C) 1994, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2006, 2007 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/** \file dir.c
 *  \brief Source: directory routines
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/search.h"
#include "lib/vfs/mc-vfs/vfs.h"
#include "lib/fs.h"
#include "lib/strutil.h"

#include "wtools.h"
#include "treestore.h"
#include "dir.h"

/* If true show files starting with a dot */
int show_dot_files = 1;

/* If true show files ending in ~ */
int show_backups = 1;

/* If false then directories are shown separately from files */
int mix_all_files = 0;

/* Reverse flag */
static int reverse = 1;

/* Are the files sorted case sensitively? */
static int case_sensitive = OS_SORT_CASE_SENSITIVE_DEFAULT;

/* Are the exec_bit files top in list*/
static int exec_first = 1;

#define MY_ISDIR(x) ( (is_exe (x->st.st_mode) && !(S_ISDIR (x->st.st_mode) || x->f.link_to_dir) && (exec_first == 1)) ? 1 : ( (S_ISDIR (x->st.st_mode) || x->f.link_to_dir) ? 2 : 0) )
/*
sort_orders_t sort_orders [SORT_TYPES_TOTAL] = {
    { N_("&Unsorted"),    unsorted },
    { N_("&Name"),        sort_name },
    { N_("&Extension"),   sort_ext },
    { N_("&Modify time"), sort_time },
    { N_("&Access time"), sort_atime },
    { N_("C&Hange time"), sort_ctime },
    { N_("&Size"),        sort_size },
    { N_("&Inode"),       sort_inode },
};
*/

int
unsorted (file_entry *a, file_entry *b)
{
    (void) a;
    (void) b;
    return 0;
}

int
sort_name (file_entry *a, file_entry *b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || mix_all_files) {
        /* create key if does not exist, key will be freed after sorting */
        if (a->sort_key == NULL) 
            a->sort_key = str_create_key_for_filename (a->fname, case_sensitive);
        if (b->sort_key == NULL) 
            b->sort_key = str_create_key_for_filename (b->fname, case_sensitive);
        
	return str_key_collate (a->sort_key, b->sort_key, case_sensitive) 
                * reverse;
    }
    return bd - ad;
}

int
sort_ext (file_entry *a, file_entry *b)
{
    int r;
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || mix_all_files){
        if (a->second_sort_key == NULL) 
            a->second_sort_key = str_create_key (extension (a->fname), case_sensitive);
        if (b->second_sort_key == NULL) 
            b->second_sort_key = str_create_key (extension (b->fname), case_sensitive);
	
        r = str_key_collate (a->second_sort_key, b->second_sort_key, case_sensitive);
	if (r)
	    return r * reverse;
	else
	    return sort_name (a, b);
    } else
	return bd-ad;
}

int
sort_time (file_entry *a, file_entry *b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || mix_all_files) {
	int result = a->st.st_mtime < b->st.st_mtime ? -1 :
		     a->st.st_mtime > b->st.st_mtime;
	if (result != 0)
	    return result * reverse;
	else
	    return sort_name (a, b);
    }
    else
	return bd-ad;
}

int
sort_ctime (file_entry *a, file_entry *b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || mix_all_files) {
	int result = a->st.st_ctime < b->st.st_ctime ? -1 :
		     a->st.st_ctime > b->st.st_ctime;
	if (result != 0)
	    return result * reverse;
	else
	    return sort_name (a, b);
    }
    else
	return bd-ad;
}

int
sort_atime (file_entry *a, file_entry *b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || mix_all_files) {
	int result = a->st.st_atime < b->st.st_atime ? -1 :
		     a->st.st_atime > b->st.st_atime;
	if (result != 0)
	    return result * reverse;
	else
	    return sort_name (a, b);
    }
    else
	return bd-ad;
}

int
sort_inode (file_entry *a, file_entry *b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || mix_all_files)
	return (a->st.st_ino - b->st.st_ino) * reverse;
    else
	return bd-ad;
}

int
sort_size (file_entry *a, file_entry *b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);
    int result = 0;

    if (ad != bd && !mix_all_files)
	return bd - ad;

    result = a->st.st_size < b->st.st_size ? -1 :
	     a->st.st_size > b->st.st_size;
    if (result != 0)
	return result * reverse;
    else
	return sort_name (a, b);
}

/* clear keys, should be call after sorting is finished */
static void
clean_sort_keys (dir_list *list, int start, int count)
{
    int i;

    for (i = 0; i < count; i++){
        str_release_key (list->list [i + start].sort_key, case_sensitive);
        list->list [i + start].sort_key = NULL;
        str_release_key (list->list [i + start].second_sort_key, case_sensitive);
        list->list [i + start].second_sort_key = NULL;
    }
}


void
do_sort (dir_list *list, sortfn *sort, int top, int reverse_f, int case_sensitive_f, int exec_first_f)
{
    int dot_dot_found = 0;

    if (top == 0)
	return;

    /* If there is an ".." entry the caller must take care to
       ensure that it occupies the first list element. */
    if (!strcmp (list->list [0].fname, ".."))
	dot_dot_found = 1;

    reverse = reverse_f ? -1 : 1;
    case_sensitive = case_sensitive_f;
    exec_first = exec_first_f;
    qsort (&(list->list) [dot_dot_found],
	   top + 1 - dot_dot_found, sizeof (file_entry), sort);
    
    clean_sort_keys (list, dot_dot_found, top + 1 - dot_dot_found);
}

void
clean_dir (dir_list *list, int count)
{
    int i;

    for (i = 0; i < count; i++){
	g_free (list->list [i].fname);
	list->list [i].fname = NULL;
    }
}

/* Used to set up a directory list when there is no access to a directory */
gboolean
set_zero_dir (dir_list *list)
{
    /* Need to grow the *list? */
    if (list->size == 0) {
	list->list = g_try_realloc (list->list, sizeof (file_entry) *
						(list->size + RESIZE_STEPS));
	if (list->list == NULL)
	    return FALSE;

	list->size += RESIZE_STEPS;
    }

    memset (&(list->list) [0], 0, sizeof(file_entry));
    list->list[0].fnamelen = 2;
    list->list[0].fname = g_strdup ("..");
    list->list[0].f.link_to_dir = 0;
    list->list[0].f.stale_link = 0;
    list->list[0].f.dir_size_computed = 0;
    list->list[0].f.marked = 0;
    list->list[0].st.st_mode = 040755;
    return TRUE;
}

/* If you change handle_dirent then check also handle_path. */
/* Return values: -1 = failure, 0 = don't add, 1 = add to the list */
static int
handle_dirent (dir_list *list, const char *filter, struct dirent *dp,
	       struct stat *buf1, int next_free, int *link_to_dir,
	       int *stale_link)
{
    if (dp->d_name[0] == '.' && dp->d_name[1] == 0)
	return 0;
    if (dp->d_name[0] == '.' && dp->d_name[1] == '.' && dp->d_name[2] == 0)
	return 0;
    if (!show_dot_files && (dp->d_name[0] == '.'))
	return 0;
    if (!show_backups && dp->d_name[NLENGTH (dp) - 1] == '~')
	return 0;
    if (mc_lstat (dp->d_name, buf1) == -1) {
	/*
	 * lstat() fails - such entries should be identified by
	 * buf1->st_mode being 0.
	 * It happens on QNX Neutrino for /fs/cd0 if no CD is inserted.
	 */
	memset (buf1, 0, sizeof (*buf1));
    }

    if (S_ISDIR (buf1->st_mode))
	tree_store_mark_checked (dp->d_name);

    /* A link to a file or a directory? */
    *link_to_dir = 0;
    *stale_link = 0;
    if (S_ISLNK (buf1->st_mode)) {
	struct stat buf2;
	if (!mc_stat (dp->d_name, &buf2))
	    *link_to_dir = S_ISDIR (buf2.st_mode) != 0;
	else
	    *stale_link = 1;
    }
    if (!(S_ISDIR (buf1->st_mode) || *link_to_dir) && filter
	&& !mc_search(filter, dp->d_name, MC_SEARCH_T_GLOB) )
	    return 0;

    /* Need to grow the *list? */
    if (next_free == list->size) {
	list->list = g_try_realloc (list->list, sizeof (file_entry) *
						(list->size + RESIZE_STEPS));
	if (list->list == NULL)
	    return -1;
	list->size += RESIZE_STEPS;
    }
    return 1;
}

/* get info about ".." */
static gboolean
get_dotdot_dir_stat (const char *path, struct stat *st)
{
    gboolean ret = FALSE;

    if ((path != NULL) && (path[0] != '\0') && (st != NULL)) {
	char *dotdot_dir;
	struct stat s;

	dotdot_dir = g_strdup_printf ("%s/../", path);
	canonicalize_pathname (dotdot_dir);
	ret = mc_stat (dotdot_dir, &s) == 0;
	g_free (dotdot_dir);
	*st = s;
    }

    return ret;
}

/* handle_path is a simplified handle_dirent. The difference is that
   handle_path doesn't pay attention to show_dot_files and show_backups.
   Moreover handle_path can't be used with a filemask.
   If you change handle_path then check also handle_dirent. */
/* Return values: -1 = failure, 0 = don't add, 1 = add to the list */
int
handle_path (dir_list *list, const char *path,
	     struct stat *buf1, int next_free, int *link_to_dir,
	     int *stale_link)
{
    if (path [0] == '.' && path [1] == 0)
	return 0;
    if (path [0] == '.' && path [1] == '.' && path [2] == 0)
	return 0;
    if (mc_lstat (path, buf1) == -1)
        return 0;

    if (S_ISDIR (buf1->st_mode))
	tree_store_mark_checked (path);

    /* A link to a file or a directory? */
    *link_to_dir = 0;
    *stale_link = 0;
    if (S_ISLNK(buf1->st_mode)){
	struct stat buf2;
	if (!mc_stat (path, &buf2))
	    *link_to_dir = S_ISDIR(buf2.st_mode) != 0;
	else
	    *stale_link = 1;
    }

    /* Need to grow the *list? */
    if (next_free == list->size){
	list->list = g_try_realloc (list->list, sizeof (file_entry) *
						(list->size + RESIZE_STEPS));
	if (list->list == NULL)
	    return -1;
	list->size += RESIZE_STEPS;
    }
    return 1;
}

int
do_load_dir (const char *path, dir_list *list, sortfn *sort, int lc_reverse,
	     int lc_case_sensitive, int exec_ff, const char *filter)
{
    DIR *dirp;
    struct dirent *dp;
    int status, link_to_dir, stale_link;
    int next_free = 0;
    struct stat st;

    /* ".." (if any) must be the first entry in the list */
    if (!set_zero_dir (list))
	return next_free;

    if (get_dotdot_dir_stat (path, &st))
	list->list[next_free].st = st;
    next_free++;

    dirp = mc_opendir (path);
    if (!dirp) {
	message (D_ERROR, MSG_ERROR, _("Cannot read directory contents"));
	return next_free;
    }

    tree_store_start_check (path);

    /* Do not add a ".." entry to the root directory */
    if ((path[0] == PATH_SEP) && (path[1] == '\0'))
	next_free--;

    while ((dp = mc_readdir (dirp))) {
	status =
	    handle_dirent (list, filter, dp, &st, next_free, &link_to_dir,
			   &stale_link);
	if (status == 0)
	    continue;
	if (status == -1) {
	    tree_store_end_check ();
	    mc_closedir (dirp);
	    return next_free;
	}
	list->list[next_free].fnamelen = NLENGTH (dp);
	list->list[next_free].fname = g_strdup (dp->d_name);
	list->list[next_free].f.marked = 0;
	list->list[next_free].f.link_to_dir = link_to_dir;
	list->list[next_free].f.stale_link = stale_link;
	list->list[next_free].f.dir_size_computed = 0;
	list->list[next_free].st = st;
        list->list[next_free].sort_key = NULL;
        list->list[next_free].second_sort_key = NULL;
	next_free++;

	if ((next_free & 31) == 0)
	    rotate_dash ();
    }

    if (next_free != 0)
	do_sort (list, sort, next_free - 1, lc_reverse, lc_case_sensitive, exec_ff);

    mc_closedir (dirp);
    tree_store_end_check ();
    return next_free;
}

int
link_isdir (const file_entry *file)
{
    if (file->f.link_to_dir)
	return 1;
    else
	return 0;
}

int
if_link_is_exe (const char *full_name, const file_entry *file)
{
    struct stat b;

    if (S_ISLNK (file->st.st_mode) && !mc_stat (full_name, &b)) {
	return is_exe (b.st_mode);
    } else
	return 1;
}

static dir_list dir_copy = { 0, 0 };

static void
alloc_dir_copy (int size)
{
    if (dir_copy.size < size) {
	if (dir_copy.list) {
	    int i;
	    for (i = 0; i < dir_copy.size; i++)
		g_free (dir_copy.list [i].fname);
	    g_free (dir_copy.list);
	}

	dir_copy.list = g_new0 (file_entry, size);
	dir_copy.size = size;
    }
}

/* If filter is null, then it is a match */
int
do_reload_dir (const char *path, dir_list *list, sortfn *sort, int count,
	       int rev, int lc_case_sensitive, int exec_ff, const char *filter)
{
    DIR *dirp;
    struct dirent *dp;
    int next_free = 0;
    int i, status, link_to_dir, stale_link;
    struct stat st;
    int marked_cnt;
    GHashTable *marked_files;

    dirp = mc_opendir (path);
    if (!dirp) {
	message (D_ERROR, MSG_ERROR, _("Cannot read directory contents"));
	clean_dir (list, count);
	return set_zero_dir (list) ? 1 : 0;
    }

    tree_store_start_check (path);
    marked_files = g_hash_table_new (g_str_hash, g_str_equal);
    alloc_dir_copy (list->size);
    for (marked_cnt = i = 0; i < count; i++) {
	dir_copy.list[i].fnamelen = list->list[i].fnamelen;
	dir_copy.list[i].fname = list->list[i].fname;
	dir_copy.list[i].f.marked = list->list[i].f.marked;
	dir_copy.list[i].f.dir_size_computed =
	    list->list[i].f.dir_size_computed;
	dir_copy.list[i].f.link_to_dir = list->list[i].f.link_to_dir;
	dir_copy.list[i].f.stale_link = list->list[i].f.stale_link;
        dir_copy.list[i].sort_key = NULL;
        dir_copy.list[i].second_sort_key = NULL;
	if (list->list[i].f.marked) {
	    g_hash_table_insert (marked_files, dir_copy.list[i].fname,
				 &dir_copy.list[i]);
	    marked_cnt++;
	}
    }

    /* Add ".." except to the root directory. The ".." entry
       (if any) must be the first in the list. */
    if (!((path[0] == PATH_SEP) && (path[1] == '\0'))) {
	if (!set_zero_dir (list)) {
	    clean_dir (list, count);
	    clean_dir (&dir_copy, count);
	    return next_free;
	}

	if (get_dotdot_dir_stat (path, &st))
	    list->list[next_free].st = st;

	next_free++;
    }

    while ((dp = mc_readdir (dirp))) {
	status =
	    handle_dirent (list, filter, dp, &st, next_free, &link_to_dir,
			   &stale_link);
	if (status == 0)
	    continue;
	if (status == -1) {
	    mc_closedir (dirp);
	    /* Norbert (Feb 12, 1997):
	       Just in case someone finds this memory leak:
	       -1 means big trouble (at the moment no memory left),
	       I don't bother with further cleanup because if one gets to
	       this point he will have more problems than a few memory
	       leaks and because one 'clean_dir' would not be enough (and
	       because I don't want to spent the time to make it working,
	       IMHO it's not worthwhile).
	       clean_dir (&dir_copy, count);
	     */
	    tree_store_end_check ();
	    g_hash_table_destroy (marked_files);
	    return next_free;
	}

	list->list[next_free].f.marked = 0;

	/*
	 * If we have marked files in the copy, scan through the copy
	 * to find matching file.  Decrease number of remaining marks if
	 * we copied one.
	 */
	if (marked_cnt > 0) {
	    if ((g_hash_table_lookup (marked_files, dp->d_name))) {
		list->list[next_free].f.marked = 1;
		marked_cnt--;
	    }
	}

	list->list[next_free].fnamelen = NLENGTH (dp);
	list->list[next_free].fname = g_strdup (dp->d_name);
	list->list[next_free].f.link_to_dir = link_to_dir;
	list->list[next_free].f.stale_link = stale_link;
	list->list[next_free].f.dir_size_computed = 0;
	list->list[next_free].st = st;
	list->list[next_free].sort_key = NULL;
	list->list[next_free].second_sort_key = NULL;
	next_free++;
	if (!(next_free % 16))
	    rotate_dash ();
    }
    mc_closedir (dirp);
    tree_store_end_check ();
    g_hash_table_destroy (marked_files);
    if (next_free) {
	do_sort (list, sort, next_free - 1, rev, lc_case_sensitive, exec_ff);
    }
    clean_dir (&dir_copy, count);
    return next_free;
}

