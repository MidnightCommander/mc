/* Directory routines
   Copyright (C) 1994 Miguel de Icaza.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <config.h>
#define DIR_H_INCLUDE_HANDLE_DIRENT
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

#include "global.h"
#include "tty.h"
#include "dir.h"
#include "wtools.h"
#include "tree.h"

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

#define MY_ISDIR(x) ( (S_ISDIR (x->st.st_mode) || x->f.link_to_dir) ? 1 : 0)

sort_orders_t sort_orders [SORT_TYPES_TOTAL] = {
    { N_("&Unsorted"),    unsorted },
    { N_("&Name"),        sort_name },
    { N_("&Extension"),   sort_ext },
    { N_("&Modify time"), sort_time },
    { N_("&Access time"), sort_atime },
    { N_("&Change time"), sort_ctime },
    { N_("&Size"),        sort_size },
    { N_("&Inode"),       sort_inode },

    /* New sort orders */
    { N_("&Type"),        sort_type },
    { N_("&Links"),       sort_links },
    { N_("N&GID"),        sort_ngid },
    { N_("N&UID"),        sort_nuid },
    { N_("&Owner"),       sort_owner },
    { N_("&Group"),       sort_group }
};

#ifdef HAVE_STRCOLL
/*
 * g_strcasecmp() doesn't work well in some locales because it relies on
 * the locale-specific toupper().  On the other hand, strcoll() is case
 * sensitive in the "C" and "POSIX" locales, unlike other locales.
 * Solution: always use strcmp() for case sensitive sort.  For case
 * insensitive sort use strcoll() if it's case insensitive for ASCII and
 * g_strcasecmp() otherwise.
 */
typedef enum {
    STRCOLL_NO,
    STRCOLL_YES,
    STRCOLL_TEST	
} strcoll_status;

static int string_sortcomp (char *str1, char *str2)
{
    static strcoll_status use_strcoll = STRCOLL_TEST;

    if (case_sensitive) {
	return strcmp (str1, str2);
    }

    /* Initialize use_strcoll once.  */
    if (use_strcoll == STRCOLL_TEST) {
	/* Only use strcoll() if it considers "B" between "a" and "c".  */
	if (strcoll ("a", "B") * strcoll ("B", "c") > 0) {
	    use_strcoll = STRCOLL_YES;
	} else {
	    use_strcoll = STRCOLL_NO;
	}
    }

    if (use_strcoll == STRCOLL_NO)
	return g_strcasecmp (str1, str2);
    else
	return strcoll (str1, str2);
}
#else
#define string_sortcomp(a,b) (case_sensitive ? strcmp (a,b) : g_strcasecmp (a,b))
#endif

int
unsorted (const file_entry *a, const file_entry *b)
{
    return 0;
}

int
sort_name (const file_entry *a, const file_entry *b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || mix_all_files)
	return string_sortcomp (a->fname, b->fname) * reverse;
    return bd-ad;
}

int
sort_ext (const file_entry *a, const file_entry *b)
{
    char *exta, *extb;
    int r;
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || mix_all_files){
	exta = extension (a->fname);
	extb = extension (b->fname);
	r = string_sortcomp (exta, extb);
	if (r)
	    return r * reverse;
	else
	    return sort_name (a, b);
    } else
	return bd-ad;
}

int
sort_owner (const file_entry *a, const file_entry *b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || mix_all_files)
	return string_sortcomp (get_owner (a->st.st_uid), get_owner (a->st.st_uid)) * reverse;
    return bd-ad;
}

int
sort_group (const file_entry *a, const file_entry *b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || mix_all_files)
	return string_sortcomp (get_group (a->st.st_gid), get_group (a->st.st_gid)) * reverse;
    return bd-ad;
}

int
sort_time (const file_entry *a, const file_entry *b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || mix_all_files)
	return (a->st.st_mtime - b->st.st_mtime) * reverse;
    else
	return bd-ad;
}

int
sort_ctime (const file_entry *a, const file_entry *b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || mix_all_files)
	return (a->st.st_ctime - b->st.st_ctime) * reverse;
    else
	return bd-ad;
}

int
sort_atime (const file_entry *a, const file_entry *b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || mix_all_files)
	return (a->st.st_atime - b->st.st_atime) * reverse;
    else
	return bd-ad;
}

int
sort_inode (const file_entry *a, const file_entry *b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || mix_all_files)
	return (a->st.st_ino - b->st.st_ino) * reverse;
    else
	return bd-ad;
}

int
sort_size (const file_entry *a, const file_entry *b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad != bd && !mix_all_files)
	return bd - ad;

    return (2 * (b->st.st_size > a->st.st_size) - 1) * reverse;
}

int
sort_links (const file_entry *a, const file_entry *b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || mix_all_files)
	return (b->st.st_nlink - a->st.st_nlink) * reverse;
    else
	return bd-ad;
}

int
sort_ngid (const file_entry *a, const file_entry *b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || mix_all_files)
	return (b->st.st_gid - a->st.st_gid) * reverse;
    else
	return bd-ad;
}

int
sort_nuid (const file_entry *a, const file_entry *b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || mix_all_files)
	return (b->st.st_uid - a->st.st_uid) * reverse;
    else
	return bd-ad;
}

inline static int
file_type_to_num (const file_entry *fe)
{
    const struct stat *s = &fe->st;

    if (S_ISDIR (s->st_mode))
	return 0;
    if (S_ISLNK (s->st_mode)){
	if (fe->f.link_to_dir)
	    return 1;
	if (fe->f.stale_link)
	    return 2;
	else
	    return 3;
    }
    if (S_ISSOCK (s->st_mode))
	return 4;
    if (S_ISCHR (s->st_mode))
	return 5;
    if (S_ISBLK (s->st_mode))
	return 6;
    if (S_ISFIFO (s->st_mode))
	return 7;
    if (is_exe (s->st_mode))
	return 8;
    return 9;
}

int
sort_type (const file_entry *a, const file_entry *b)
{
    int aa  = file_type_to_num (a);
    int bb  = file_type_to_num (b);

    return bb-aa;
}


void
do_sort (dir_list *list, sortfn *sort, int top, int reverse_f, int case_sensitive_f)
{
    int i;
    int dot_dot_found = 0;
    file_entry tmp_fe;

    for (i = 0; i < top + 1; i++) {             /* put ".." first in list */
	if (!strcmp (list->list [i].fname, "..")) {
	    dot_dot_found = 1;
            if (i > 0) {                        /* swap [i] and [0] */
                memcpy (&tmp_fe, &(list->list [0]), sizeof (file_entry));
                memcpy (&(list->list [0]), &(list->list [i]), sizeof (file_entry));
                memcpy (&(list->list [i]), &tmp_fe, sizeof (file_entry));
            }
            break;
        }
    }

    reverse = reverse_f ? -1 : 1;
    case_sensitive = case_sensitive_f;
    qsort (&(list->list) [dot_dot_found],
	   top + 1 - dot_dot_found, sizeof (file_entry), sort);
}

void
clean_dir (dir_list *list, int count)
{
    int i;

    for (i = 0; i < count; i++){
	g_free (list->list [i].fname);
	list->list [i].fname = 0;
    }
}

static int
add_dotdot_to_list (dir_list *list, int index)
{
    /* Need to grow the *list? */
    if (index == list->size) {
	list->list = g_realloc (list->list, sizeof (file_entry) *
			      (list->size + RESIZE_STEPS));
	if (!list->list)
	    return 0;
	list->size += RESIZE_STEPS;
    }

    memset (&(list->list) [index], 0, sizeof(file_entry));
    (list->list) [index].fnamelen = 2;
    (list->list) [index].fname = g_strdup ("..");
    (list->list) [index].f.link_to_dir = 0;
    (list->list) [index].f.stale_link = 0;
    (list->list) [index].f.dir_size_computed = 0;
    (list->list) [index].f.marked = 0;
    (list->list) [index].st.st_mode = 040755;
    return 1;
}

/* Used to set up a directory list when there is no access to a directory */
int
set_zero_dir (dir_list *list)
{
    return (add_dotdot_to_list (list, 0));
}

/* If you change handle_dirent then check also handle_path. */
/* Return values: -1 = failure, 0 = don't add, 1 = add to the list */
int
handle_dirent (dir_list *list, char *filter, struct dirent *dp,
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
	&& !regexp_match (filter, dp->d_name, match_file))
	return 0;

    /* Need to grow the *list? */
    if (next_free == list->size) {
	list->list =
	    g_realloc (list->list,
		       sizeof (file_entry) * (list->size + RESIZE_STEPS));
	if (!list->list)
	    return -1;
	list->size += RESIZE_STEPS;
    }
    return 1;
}

/* handle_path is a simplified handle_dirent. The difference is that
   handle_path doesn't pay attention to show_dot_files and show_backups.
   Moreover handle_path can't be used with a filemask.
   If you change handle_path then check also handle_dirent. */
/* Return values: -1 = failure, 0 = don't add, 1 = add to the list */
int
handle_path (dir_list *list, char *path,
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
	list->list = g_realloc (list->list, sizeof (file_entry) *
			      (list->size + RESIZE_STEPS));
	if (!list->list)
	    return -1;
	list->size += RESIZE_STEPS;
    }
    return 1;
}

int
do_load_dir (char *path, dir_list *list, sortfn *sort, int reverse,
	     int case_sensitive, char *filter)
{
    DIR *dirp;
    struct dirent *dp;
    int status, link_to_dir, stale_link;
    int next_free = 0;
    struct stat st;

    tree_store_start_check_cwd ();

    dirp = mc_opendir (".");
    if (!dirp) {
	message (1, MSG_ERROR, _("Cannot read directory contents"));
	tree_store_end_check ();
	return set_zero_dir (list);
    }
    for (dp = mc_readdir (dirp); dp; dp = mc_readdir (dirp)) {
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
	next_free++;
	if (!(next_free % 32))
	    rotate_dash ();
    }

    if (next_free) {
	/* Add ".." except the root directory */
	if (strcmp (path, "/") != 0)
	    add_dotdot_to_list (list, next_free++);
	do_sort (list, sort, next_free - 1, reverse, case_sensitive);
    } else {
	tree_store_end_check ();
	mc_closedir (dirp);
	return set_zero_dir (list);
    }

    mc_closedir (dirp);
    tree_store_end_check ();
    return next_free;
}

int
link_isdir (file_entry *file)
{
    if (file->f.link_to_dir)
	return 1;
    else
	return 0;
}

int
if_link_is_exe (char *full_name, file_entry *file)
{
    struct stat b;

    if (S_ISLNK (file->st.st_mode)) {
	mc_stat (full_name, &b);
	return is_exe (b.st_mode);
    } else
	return 1;
}

static dir_list dir_copy = { 0, 0 };

static void
alloc_dir_copy (int size)
{
    int i;

    if (dir_copy.size < size){
	if (dir_copy.list){

	    for (i = 0; i < dir_copy.size; i++) {
		if (dir_copy.list [i].fname)
		    g_free (dir_copy.list [i].fname);
	    }
	    g_free (dir_copy.list);
	    dir_copy.list = 0;
	}

	dir_copy.list = g_new (file_entry, size);
	for (i = 0; i < size; i++)
	    dir_copy.list [i].fname = 0;

	dir_copy.size = size;
    }
}

/* If filter is null, then it is a match */
int
do_reload_dir (char *path, dir_list *list, sortfn *sort, int count,
	       int rev, int case_sensitive, char *filter)
{
    DIR *dirp;
    struct dirent *dp;
    int next_free = 0;
    int i, status, link_to_dir, stale_link;
    struct stat st;
    int marked_cnt;
    GHashTable *marked_files = g_hash_table_new (g_str_hash, g_str_equal);

    tree_store_start_check_cwd ();
    dirp = mc_opendir (".");
    if (!dirp) {
	message (1, MSG_ERROR, _("Cannot read directory contents"));
	clean_dir (list, count);
	tree_store_end_check ();
	return set_zero_dir (list);
    }

    alloc_dir_copy (list->size);
    for (marked_cnt = i = 0; i < count; i++) {
	dir_copy.list[i].fnamelen = list->list[i].fnamelen;
	dir_copy.list[i].fname = list->list[i].fname;
	dir_copy.list[i].f.marked = list->list[i].f.marked;
	dir_copy.list[i].f.dir_size_computed =
	    list->list[i].f.dir_size_computed;
	dir_copy.list[i].f.link_to_dir = list->list[i].f.link_to_dir;
	dir_copy.list[i].f.stale_link = list->list[i].f.stale_link;
	if (list->list[i].f.marked) {
	    g_hash_table_insert (marked_files, dir_copy.list[i].fname,
				 &dir_copy.list[i]);
	    marked_cnt++;
	}
    }

    for (dp = mc_readdir (dirp); dp; dp = mc_readdir (dirp)) {
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
	    return next_free;
	}

	list->list[next_free].f.marked = 0;

	/*
	 * If we have marked files in the copy, scan through the copy
	 * to find matching file.  Decrease number of remaining marks if
	 * we copied one.
	 */
	if (marked_cnt > 0) {
	    file_entry *p;
	    if (NULL !=
		(p = g_hash_table_lookup (marked_files, dp->d_name))) {
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
	next_free++;
	if (!(next_free % 16))
	    rotate_dash ();
    }
    mc_closedir (dirp);
    tree_store_end_check ();
    g_hash_table_destroy (marked_files);
    if (next_free) {
	/* Add ".." except the root directory */
	if (strcmp (path, "/") != 0)
	    add_dotdot_to_list (list, next_free++);
	do_sort (list, sort, next_free - 1, rev, case_sensitive);
    } else
	next_free = set_zero_dir (list);
    clean_dir (&dir_copy, count);
    return next_free;
}

