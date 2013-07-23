/*
   Directory routines

   Copyright (C) 1994, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2006, 2007, 2011, 2013
   The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2013

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file src/filemanager/dir.c
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
#include "lib/vfs/vfs.h"
#include "lib/fs.h"
#include "lib/strutil.h"
#include "lib/util.h"
#include "lib/widget.h"         /* message() */

#include "src/setup.h"          /* panels_options */

#include "treestore.h"
#include "dir.h"
#include "layout.h"             /* rotate_dash() */

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define MY_ISDIR(x) (\
    (is_exe (x->st.st_mode) && !(S_ISDIR (x->st.st_mode) || x->f.link_to_dir) && exec_first) \
        ? 1 \
        : ( (S_ISDIR (x->st.st_mode) || x->f.link_to_dir) ? 2 : 0) )

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* Reverse flag */
static int reverse = 1;

/* Are the files sorted case sensitively? */
static int case_sensitive = OS_SORT_CASE_SENSITIVE_DEFAULT;

/* Are the exec_bit files top in list */
static gboolean exec_first = TRUE;

static dir_list dir_copy = { 0, 0 };

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

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

static inline int
key_collate (const char *t1, const char *t2)
{
    int dotdot = 0;
    int ret;

    dotdot = (t1[0] == '.' ? 1 : 0) | ((t2[0] == '.' ? 1 : 0) << 1);

    switch (dotdot)
    {
    case 0:
    case 3:
        ret = str_key_collate (t1, t2, case_sensitive) * reverse;
        break;
    case 1:
        ret = -1;               /* t1 < t2 */
        break;
    case 2:
        ret = 1;                /* t1 > t2 */
        break;
    default:
        ret = 0;                /* it must not happen */
    }

    return ret;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * clear keys, should be call after sorting is finished.
 */

static void
clean_sort_keys (dir_list * list, int start, int count)
{
    int i;

    for (i = 0; i < count; i++)
    {
        str_release_key (list->list[i + start].sort_key, case_sensitive);
        list->list[i + start].sort_key = NULL;
        str_release_key (list->list[i + start].second_sort_key, case_sensitive);
        list->list[i + start].second_sort_key = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Increase directory list by RESIZE_STEPS
 *
 * @param list directory list
 * @return FALSE on failure, TRUE on success
 */

static gboolean
grow_list (dir_list * list)
{
    if (list == NULL)
        return FALSE;

    list->list = g_try_realloc (list->list, sizeof (file_entry) * (list->size + RESIZE_STEPS));

    if (list->list == NULL)
        return FALSE;

    list->size += RESIZE_STEPS;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * If you change handle_dirent then check also handle_path.
 * @return -1 = failure, 0 = don't add, 1 = add to the list
 */

static int
handle_dirent (dir_list * list, const char *fltr, struct dirent *dp,
               struct stat *buf1, int next_free, int *link_to_dir, int *stale_link)
{
    vfs_path_t *vpath;

    if (DIR_IS_DOT (dp->d_name) || DIR_IS_DOTDOT (dp->d_name))
        return 0;
    if (!panels_options.show_dot_files && (dp->d_name[0] == '.'))
        return 0;
    if (!panels_options.show_backups && dp->d_name[NLENGTH (dp) - 1] == '~')
        return 0;

    vpath = vfs_path_from_str (dp->d_name);
    if (mc_lstat (vpath, buf1) == -1)
    {
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
    if (S_ISLNK (buf1->st_mode))
    {
        struct stat buf2;
        if (mc_stat (vpath, &buf2) == 0)
            *link_to_dir = S_ISDIR (buf2.st_mode) != 0;
        else
            *stale_link = 1;
    }
    vfs_path_free (vpath);
    if (!(S_ISDIR (buf1->st_mode) || *link_to_dir) && (fltr != NULL)
        && !mc_search (fltr, dp->d_name, MC_SEARCH_T_GLOB))
        return 0;

    /* Need to grow the *list? */
    if (next_free == list->size && !grow_list (list))
        return -1;

    return 1;
}

/* --------------------------------------------------------------------------------------------- */
/** get info about ".." */

static gboolean
get_dotdot_dir_stat (const vfs_path_t * vpath, struct stat *st)
{
    gboolean ret = FALSE;

    if ((vpath != NULL) && (st != NULL))
    {
        const char *path;

        path = vfs_path_get_by_index (vpath, 0)->path;
        if (path != NULL && *path != '\0')
        {
            vfs_path_t *tmp_vpath;
            struct stat s;

            tmp_vpath = vfs_path_append_new (vpath, "..", NULL);
            ret = mc_stat (tmp_vpath, &s) == 0;
            vfs_path_free (tmp_vpath);
            *st = s;
        }
    }

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static void
alloc_dir_copy (int size)
{
    if (dir_copy.size < size)
    {
        if (dir_copy.list)
        {
            int i;
            for (i = 0; i < dir_copy.size; i++)
                g_free (dir_copy.list[i].fname);
            g_free (dir_copy.list);
        }

        dir_copy.list = g_new0 (file_entry, size);
        dir_copy.size = size;
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

int
unsorted (file_entry * a, file_entry * b)
{
    (void) a;
    (void) b;
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

int
sort_name (file_entry * a, file_entry * b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || panels_options.mix_all_files)
    {
        /* create key if does not exist, key will be freed after sorting */
        if (a->sort_key == NULL)
            a->sort_key = str_create_key_for_filename (a->fname, case_sensitive);
        if (b->sort_key == NULL)
            b->sort_key = str_create_key_for_filename (b->fname, case_sensitive);

        return key_collate (a->sort_key, b->sort_key);
    }
    return bd - ad;
}

/* --------------------------------------------------------------------------------------------- */

int
sort_vers (file_entry * a, file_entry * b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || panels_options.mix_all_files)
    {
        return str_verscmp (a->fname, b->fname) * reverse;
    }
    else
    {
        return bd - ad;
    }
}

/* --------------------------------------------------------------------------------------------- */

int
sort_ext (file_entry * a, file_entry * b)
{
    int r;
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || panels_options.mix_all_files)
    {
        if (a->second_sort_key == NULL)
            a->second_sort_key = str_create_key (extension (a->fname), case_sensitive);
        if (b->second_sort_key == NULL)
            b->second_sort_key = str_create_key (extension (b->fname), case_sensitive);

        r = str_key_collate (a->second_sort_key, b->second_sort_key, case_sensitive);
        if (r)
            return r * reverse;
        else
            return sort_name (a, b);
    }
    else
        return bd - ad;
}

/* --------------------------------------------------------------------------------------------- */

int
sort_time (file_entry * a, file_entry * b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || panels_options.mix_all_files)
    {
        int result = a->st.st_mtime < b->st.st_mtime ? -1 : a->st.st_mtime > b->st.st_mtime;
        if (result != 0)
            return result * reverse;
        else
            return sort_name (a, b);
    }
    else
        return bd - ad;
}

/* --------------------------------------------------------------------------------------------- */

int
sort_ctime (file_entry * a, file_entry * b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || panels_options.mix_all_files)
    {
        int result = a->st.st_ctime < b->st.st_ctime ? -1 : a->st.st_ctime > b->st.st_ctime;
        if (result != 0)
            return result * reverse;
        else
            return sort_name (a, b);
    }
    else
        return bd - ad;
}

/* --------------------------------------------------------------------------------------------- */

int
sort_atime (file_entry * a, file_entry * b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || panels_options.mix_all_files)
    {
        int result = a->st.st_atime < b->st.st_atime ? -1 : a->st.st_atime > b->st.st_atime;
        if (result != 0)
            return result * reverse;
        else
            return sort_name (a, b);
    }
    else
        return bd - ad;
}

/* --------------------------------------------------------------------------------------------- */

int
sort_inode (file_entry * a, file_entry * b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || panels_options.mix_all_files)
        return (a->st.st_ino - b->st.st_ino) * reverse;
    else
        return bd - ad;
}

/* --------------------------------------------------------------------------------------------- */

int
sort_size (file_entry * a, file_entry * b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);
    int result = 0;

    if (ad != bd && !panels_options.mix_all_files)
        return bd - ad;

    result = a->st.st_size < b->st.st_size ? -1 : a->st.st_size > b->st.st_size;
    if (result != 0)
        return result * reverse;
    else
        return sort_name (a, b);
}

/* --------------------------------------------------------------------------------------------- */

void
do_sort (dir_list * list, sortfn * sort, int top, gboolean reverse_f, gboolean case_sensitive_f,
         gboolean exec_first_f)
{
    int dot_dot_found = 0;

    if (top == 0)
        return;

    /* If there is an ".." entry the caller must take care to
       ensure that it occupies the first list element. */
    if (DIR_IS_DOTDOT (list->list[0].fname))
        dot_dot_found = 1;

    reverse = reverse_f ? -1 : 1;
    case_sensitive = case_sensitive_f ? 1 : 0;
    exec_first = exec_first_f;
    qsort (&(list->list)[dot_dot_found], top + 1 - dot_dot_found, sizeof (file_entry), sort);

    clean_sort_keys (list, dot_dot_found, top + 1 - dot_dot_found);
}

/* --------------------------------------------------------------------------------------------- */

void
clean_dir (dir_list * list, int count)
{
    int i;

    for (i = 0; i < count; i++)
    {
        g_free (list->list[i].fname);
        list->list[i].fname = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */
/** Used to set up a directory list when there is no access to a directory */

gboolean
set_zero_dir (dir_list * list)
{
    /* Need to grow the *list? */
    if (list->size == 0 && !grow_list (list))
        return FALSE;

    memset (&(list->list)[0], 0, sizeof (file_entry));
    list->list[0].fnamelen = 2;
    list->list[0].fname = g_strndup ("..", list->list[0].fnamelen);
    list->list[0].f.link_to_dir = 0;
    list->list[0].f.stale_link = 0;
    list->list[0].f.dir_size_computed = 0;
    list->list[0].f.marked = 0;
    list->list[0].st.st_mode = 040755;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/**
   handle_path is a simplified handle_dirent. The difference is that
   handle_path doesn't pay attention to panels_options.show_dot_files
   and panels_options.show_backups.
   Moreover handle_path can't be used with a filemask.
   If you change handle_path then check also handle_dirent. */
/* Return values: -1 = failure, 0 = don't add, 1 = add to the list */

int
handle_path (dir_list * list, const char *path,
             struct stat *buf1, int next_free, int *link_to_dir, int *stale_link)
{
    vfs_path_t *vpath;

    if (DIR_IS_DOT (path) || DIR_IS_DOTDOT (path))
        return 0;

    vpath = vfs_path_from_str (path);
    if (mc_lstat (vpath, buf1) == -1)
    {
        vfs_path_free (vpath);
        return 0;
    }

    if (S_ISDIR (buf1->st_mode))
        tree_store_mark_checked (path);

    /* A link to a file or a directory? */
    *link_to_dir = 0;
    *stale_link = 0;
    if (S_ISLNK (buf1->st_mode))
    {
        struct stat buf2;
        if (mc_stat (vpath, &buf2) == 0)
            *link_to_dir = S_ISDIR (buf2.st_mode) != 0;
        else
            *stale_link = 1;
    }

    vfs_path_free (vpath);

    /* Need to grow the *list? */
    if (next_free == list->size && !grow_list (list))
        return -1;

    return 1;
}

/* --------------------------------------------------------------------------------------------- */

int
do_load_dir (const vfs_path_t * vpath, dir_list * list, sortfn * sort, gboolean lc_reverse,
             gboolean lc_case_sensitive, gboolean exec_ff, const char *fltr)
{
    DIR *dirp;
    struct dirent *dp;
    int status, link_to_dir, stale_link;
    int next_free = 0;
    struct stat st;

    /* ".." (if any) must be the first entry in the list */
    if (!set_zero_dir (list))
        return next_free;

    if (get_dotdot_dir_stat (vpath, &st))
        list->list[next_free].st = st;
    next_free++;

    dirp = mc_opendir (vpath);
    if (dirp == NULL)
    {
        message (D_ERROR, MSG_ERROR, _("Cannot read directory contents"));
        return next_free;
    }

    tree_store_start_check (vpath);

    {
        const char *vpath_str;

        vpath_str = vfs_path_as_str (vpath);
        /* Do not add a ".." entry to the root directory */
        if ((vpath_str[0] == PATH_SEP) && (vpath_str[1] == '\0'))
            next_free--;
    }

    while ((dp = mc_readdir (dirp)) != NULL)
    {
        status = handle_dirent (list, fltr, dp, &st, next_free, &link_to_dir, &stale_link);
        if (status == 0)
            continue;
        if (status == -1)
            goto ret;

        list->list[next_free].fnamelen = NLENGTH (dp);
        list->list[next_free].fname = g_strndup (dp->d_name, list->list[next_free].fnamelen);
        list->list[next_free].f.marked = 0;
        list->list[next_free].f.link_to_dir = link_to_dir;
        list->list[next_free].f.stale_link = stale_link;
        list->list[next_free].f.dir_size_computed = 0;
        list->list[next_free].st = st;
        list->list[next_free].sort_key = NULL;
        list->list[next_free].second_sort_key = NULL;
        next_free++;

        if ((next_free & 31) == 0)
            rotate_dash (TRUE);
    }

    if (next_free != 0)
        do_sort (list, sort, next_free - 1, lc_reverse, lc_case_sensitive, exec_ff);

  ret:
    mc_closedir (dirp);
    tree_store_end_check ();
    rotate_dash (FALSE);
    return next_free;
}


/* --------------------------------------------------------------------------------------------- */

gboolean
if_link_is_exe (const vfs_path_t * full_name_vpath, const file_entry * file)
{
    struct stat b;

    if (S_ISLNK (file->st.st_mode) && mc_stat (full_name_vpath, &b) == 0)
        return is_exe (b.st_mode);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/** If fltr is null, then it is a match */

int
do_reload_dir (const vfs_path_t * vpath, dir_list * list, sortfn * sort, int count,
               gboolean lc_reverse, gboolean lc_case_sensitive, gboolean exec_ff, const char *fltr)
{
    DIR *dirp;
    struct dirent *dp;
    int next_free = 0;
    int i, status, link_to_dir, stale_link;
    struct stat st;
    int marked_cnt;
    GHashTable *marked_files;
    const char *tmp_path;

    dirp = mc_opendir (vpath);
    if (dirp == NULL)
    {
        message (D_ERROR, MSG_ERROR, _("Cannot read directory contents"));
        clean_dir (list, count);
        return set_zero_dir (list) ? 1 : 0;
    }

    tree_store_start_check (vpath);

    marked_files = g_hash_table_new (g_str_hash, g_str_equal);
    alloc_dir_copy (list->size);
    for (marked_cnt = i = 0; i < count; i++)
    {
        dir_copy.list[i].fnamelen = list->list[i].fnamelen;
        dir_copy.list[i].fname = list->list[i].fname;
        dir_copy.list[i].f.marked = list->list[i].f.marked;
        dir_copy.list[i].f.dir_size_computed = list->list[i].f.dir_size_computed;
        dir_copy.list[i].f.link_to_dir = list->list[i].f.link_to_dir;
        dir_copy.list[i].f.stale_link = list->list[i].f.stale_link;
        dir_copy.list[i].sort_key = NULL;
        dir_copy.list[i].second_sort_key = NULL;
        if (list->list[i].f.marked)
        {
            g_hash_table_insert (marked_files, dir_copy.list[i].fname, &dir_copy.list[i]);
            marked_cnt++;
        }
    }

    /* Add ".." except to the root directory. The ".." entry
       (if any) must be the first in the list. */
    tmp_path = vfs_path_get_by_index (vpath, 0)->path;
    if (!
        (vfs_path_elements_count (vpath) == 1 && (tmp_path[0] == PATH_SEP)
         && (tmp_path[1] == '\0')))
    {
        if (!set_zero_dir (list))
        {
            clean_dir (list, count);
            clean_dir (&dir_copy, count);
            return next_free;
        }

        if (get_dotdot_dir_stat (vpath, &st))
            list->list[next_free].st = st;

        next_free++;
    }

    while ((dp = mc_readdir (dirp)))
    {
        status = handle_dirent (list, fltr, dp, &st, next_free, &link_to_dir, &stale_link);
        if (status == 0)
            continue;
        if (status == -1)
        {
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
        if (marked_cnt > 0)
        {
            if ((g_hash_table_lookup (marked_files, dp->d_name)))
            {
                list->list[next_free].f.marked = 1;
                marked_cnt--;
            }
        }

        list->list[next_free].fnamelen = NLENGTH (dp);
        list->list[next_free].fname = g_strndup (dp->d_name, list->list[next_free].fnamelen);
        list->list[next_free].f.link_to_dir = link_to_dir;
        list->list[next_free].f.stale_link = stale_link;
        list->list[next_free].f.dir_size_computed = 0;
        list->list[next_free].st = st;
        list->list[next_free].sort_key = NULL;
        list->list[next_free].second_sort_key = NULL;
        next_free++;
        if ((next_free % 16) == 0)
            rotate_dash (TRUE);
    }
    mc_closedir (dirp);
    tree_store_end_check ();
    g_hash_table_destroy (marked_files);
    if (next_free)
    {
        do_sort (list, sort, next_free - 1, lc_reverse, lc_case_sensitive, exec_ff);
    }
    clean_dir (&dir_copy, count);
    rotate_dash (FALSE);

    return next_free;
}

/* --------------------------------------------------------------------------------------------- */
