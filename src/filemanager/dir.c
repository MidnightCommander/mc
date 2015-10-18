/*
   Directory routines

   Copyright (C) 1994-2015
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2013
   Andrew Borodin <aborodin@vmail.ru>, 2013

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

static dir_list dir_copy = { NULL, 0, 0 };

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
        file_entry_t *fentry;

        fentry = &list->list[i + start];
        str_release_key (fentry->sort_key, case_sensitive);
        fentry->sort_key = NULL;
        str_release_key (fentry->second_sort_key, case_sensitive);
        fentry->second_sort_key = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * If you change handle_dirent then check also handle_path.
 * @return FALSE = don't add, TRUE = add to the list
 */

static gboolean
handle_dirent (struct dirent *dp, const char *fltr, struct stat *buf1, int *link_to_dir,
               int *stale_link)
{
    vfs_path_t *vpath;

    if (DIR_IS_DOT (dp->d_name) || DIR_IS_DOTDOT (dp->d_name))
        return FALSE;
    if (!panels_options.show_dot_files && (dp->d_name[0] == '.'))
        return FALSE;
    if (!panels_options.show_backups && dp->d_name[strlen (dp->d_name) - 1] == '~')
        return FALSE;

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

    return (S_ISDIR (buf1->st_mode) || *link_to_dir != 0 || fltr == NULL
            || mc_search (fltr, NULL, dp->d_name, MC_SEARCH_T_GLOB));
}

/* --------------------------------------------------------------------------------------------- */
/** get info about ".." */

static gboolean
dir_get_dotdot_stat (const vfs_path_t * vpath, struct stat *st)
{
    gboolean ret = FALSE;

    if ((vpath != NULL) && (st != NULL))
    {
        const char *path;

        path = vfs_path_get_by_index (vpath, 0)->path;
        if (path != NULL && *path != '\0')
        {
            vfs_path_t *tmp_vpath;

            tmp_vpath = vfs_path_append_new (vpath, "..", NULL);
            ret = mc_stat (tmp_vpath, st) == 0;
            vfs_path_free (tmp_vpath);
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
        if (dir_copy.list != NULL)
        {
            int i;

            for (i = 0; i < dir_copy.len; i++)
            {
                file_entry_t *fentry;

                fentry = &(dir_copy.list)[i];
                g_free (fentry->fname);
            }
            g_free (dir_copy.list);
        }

        dir_copy.list = g_new0 (file_entry_t, size);
        dir_copy.size = size;
        dir_copy.len = 0;
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Increase or decrease directory list size.
 *
 * @param list directory list
 * @param delta value by increase (if positive) or decrease (if negative) list size
 *
 * @return FALSE on failure, TRUE on success
 */

gboolean
dir_list_grow (dir_list * list, int delta)
{
    int size;
    gboolean clear = FALSE;

    if (list == NULL)
        return FALSE;

    if (delta == 0)
        return TRUE;

    size = list->size + delta;
    if (size <= 0)
    {
        size = DIR_LIST_MIN_SIZE;
        clear = TRUE;
    }

    if (size != list->size)
    {
        file_entry_t *fe;

        fe = g_try_renew (file_entry_t, list->list, size);
        if (fe == NULL)
            return FALSE;

        list->list = fe;
        list->size = size;
    }

    list->len = clear ? 0 : min (list->len, size);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Append file info to the directory list.
 *
 * @param list directory list
 * @param fname file name
 * @param st file stat info
 * @param link_to_dir is file link to directory
 * @param stale_link is file stale elink
 *
 * @return FALSE on failure, TRUE on success
 */

gboolean
dir_list_append (dir_list * list, const char *fname, const struct stat * st,
                 gboolean link_to_dir, gboolean stale_link)
{
    file_entry_t *fentry;

    /* Need to grow the *list? */
    if (list->len == list->size && !dir_list_grow (list, DIR_LIST_RESIZE_STEP))
        return FALSE;

    fentry = &list->list[list->len];
    fentry->fnamelen = strlen (fname);
    fentry->fname = g_strndup (fname, fentry->fnamelen);
    fentry->f.marked = 0;
    fentry->f.link_to_dir = link_to_dir ? 1 : 0;
    fentry->f.stale_link = stale_link ? 1 : 0;
    fentry->f.dir_size_computed = 0;
    fentry->st = *st;
    fentry->sort_key = NULL;
    fentry->second_sort_key = NULL;

    list->len++;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

int
unsorted (file_entry_t * a, file_entry_t * b)
{
    (void) a;
    (void) b;
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

int
sort_name (file_entry_t * a, file_entry_t * b)
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
sort_vers (file_entry_t * a, file_entry_t * b)
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
sort_ext (file_entry_t * a, file_entry_t * b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || panels_options.mix_all_files)
    {
        int r;

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
sort_time (file_entry_t * a, file_entry_t * b)
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
sort_ctime (file_entry_t * a, file_entry_t * b)
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
sort_atime (file_entry_t * a, file_entry_t * b)
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
sort_inode (file_entry_t * a, file_entry_t * b)
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
sort_size (file_entry_t * a, file_entry_t * b)
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
dir_list_sort (dir_list * list, GCompareFunc sort, const dir_sort_options_t * sort_op)
{
    file_entry_t *fentry;
    int dot_dot_found = 0;

    if (list->len < 2 || sort == (GCompareFunc) unsorted)
        return;

    /* If there is an ".." entry the caller must take care to
       ensure that it occupies the first list element. */
    fentry = &list->list[0];
    if (DIR_IS_DOTDOT (fentry->fname))
        dot_dot_found = 1;

    reverse = sort_op->reverse ? -1 : 1;
    case_sensitive = sort_op->case_sensitive ? 1 : 0;
    exec_first = sort_op->exec_first;
    qsort (&(list->list)[dot_dot_found], list->len - dot_dot_found, sizeof (file_entry_t), sort);

    clean_sort_keys (list, dot_dot_found, list->len - dot_dot_found);
}

/* --------------------------------------------------------------------------------------------- */

void
dir_list_clean (dir_list * list)
{
    int i;

    for (i = 0; i < list->len; i++)
    {
        file_entry_t *fentry;

        fentry = &list->list[i];
        MC_PTR_FREE (fentry->fname);
    }

    list->len = 0;
    /* reduce memory usage */
    dir_list_grow (list, DIR_LIST_MIN_SIZE - list->size);
}

/* --------------------------------------------------------------------------------------------- */
/** Used to set up a directory list when there is no access to a directory */

gboolean
dir_list_init (dir_list * list)
{
    file_entry_t *fentry;

    /* Need to grow the *list? */
    if (list->size == 0 && !dir_list_grow (list, DIR_LIST_RESIZE_STEP))
    {
        list->len = 0;
        return FALSE;
    }

    fentry = &list->list[0];
    memset (fentry, 0, sizeof (file_entry_t));
    fentry->fnamelen = 2;
    fentry->fname = g_strndup ("..", fentry->fnamelen);
    fentry->f.link_to_dir = 0;
    fentry->f.stale_link = 0;
    fentry->f.dir_size_computed = 0;
    fentry->f.marked = 0;
    fentry->st.st_mode = 040755;
    list->len = 1;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/**
   handle_path is a simplified handle_dirent. The difference is that
   handle_path doesn't pay attention to panels_options.show_dot_files
   and panels_options.show_backups.
   Moreover handle_path can't be used with a filemask.
   If you change handle_path then check also handle_dirent. */
/* Return values: FALSE = don't add, TRUE = add to the list */

gboolean
handle_path (const char *path, struct stat * buf1, int *link_to_dir, int *stale_link)
{
    vfs_path_t *vpath;

    if (DIR_IS_DOT (path) || DIR_IS_DOTDOT (path))
        return FALSE;

    vpath = vfs_path_from_str (path);
    if (mc_lstat (vpath, buf1) == -1)
    {
        vfs_path_free (vpath);
        return FALSE;
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

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
dir_list_load (dir_list * list, const vfs_path_t * vpath, GCompareFunc sort,
               const dir_sort_options_t * sort_op, const char *fltr)
{
    DIR *dirp;
    struct dirent *dp;
    int link_to_dir, stale_link;
    struct stat st;
    file_entry_t *fentry;
    const char *vpath_str;

    /* ".." (if any) must be the first entry in the list */
    if (!dir_list_init (list))
        return;

    fentry = &list->list[0];
    if (dir_get_dotdot_stat (vpath, &st))
        fentry->st = st;

    dirp = mc_opendir (vpath);
    if (dirp == NULL)
    {
        message (D_ERROR, MSG_ERROR, _("Cannot read directory contents"));
        return;
    }

    tree_store_start_check (vpath);

    vpath_str = vfs_path_as_str (vpath);
    /* Do not add a ".." entry to the root directory */
    if (IS_PATH_SEP (vpath_str[0]) && vpath_str[1] == '\0')
        dir_list_clean (list);

    while ((dp = mc_readdir (dirp)) != NULL)
    {
        if (!handle_dirent (dp, fltr, &st, &link_to_dir, &stale_link))
            continue;

        if (!dir_list_append (list, dp->d_name, &st, link_to_dir != 0, stale_link != 0))
            goto ret;

        if ((list->len & 31) == 0)
            rotate_dash (TRUE);
    }

    dir_list_sort (list, sort, sort_op);

  ret:
    mc_closedir (dirp);
    tree_store_end_check ();
    rotate_dash (FALSE);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
if_link_is_exe (const vfs_path_t * full_name_vpath, const file_entry_t * file)
{
    struct stat b;

    if (S_ISLNK (file->st.st_mode) && mc_stat (full_name_vpath, &b) == 0)
        return is_exe (b.st_mode);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/** If fltr is null, then it is a match */

void
dir_list_reload (dir_list * list, const vfs_path_t * vpath, GCompareFunc sort,
                 const dir_sort_options_t * sort_op, const char *fltr)
{
    DIR *dirp;
    struct dirent *dp;
    int i, link_to_dir, stale_link;
    struct stat st;
    int marked_cnt;
    GHashTable *marked_files;
    const char *tmp_path;

    dirp = mc_opendir (vpath);
    if (dirp == NULL)
    {
        message (D_ERROR, MSG_ERROR, _("Cannot read directory contents"));
        dir_list_clean (list);
        dir_list_init (list);
        return;
    }

    tree_store_start_check (vpath);

    marked_files = g_hash_table_new (g_str_hash, g_str_equal);
    alloc_dir_copy (list->len);
    for (marked_cnt = i = 0; i < list->len; i++)
    {
        file_entry_t *fentry, *dfentry;

        fentry = &list->list[i];
        dfentry = &dir_copy.list[i];

        dfentry->fnamelen = fentry->fnamelen;
        dfentry->fname = g_strndup (fentry->fname, fentry->fnamelen);
        dfentry->f.marked = fentry->f.marked;
        dfentry->f.dir_size_computed = fentry->f.dir_size_computed;
        dfentry->f.link_to_dir = fentry->f.link_to_dir;
        dfentry->f.stale_link = fentry->f.stale_link;
        dfentry->sort_key = NULL;
        dfentry->second_sort_key = NULL;
        if (fentry->f.marked)
        {
            g_hash_table_insert (marked_files, dfentry->fname, dfentry);
            marked_cnt++;
        }
    }

    /* save len for later dir_list_clean() */
    dir_copy.len = list->len;

    /* Add ".." except to the root directory. The ".." entry
       (if any) must be the first in the list. */
    tmp_path = vfs_path_get_by_index (vpath, 0)->path;
    if (vfs_path_elements_count (vpath) == 1 && IS_PATH_SEP (tmp_path[0]) && tmp_path[1] == '\0')
    {
        /* root directory */
        dir_list_clean (list);
    }
    else
    {
        dir_list_clean (list);
        if (!dir_list_init (list))
        {
            dir_list_clean (&dir_copy);
            return;
        }

        if (dir_get_dotdot_stat (vpath, &st))
        {
            file_entry_t *fentry;

            fentry = &list->list[0];
            fentry->st = st;
        }
    }

    while ((dp = mc_readdir (dirp)) != NULL)
    {
        file_entry_t *fentry;

        if (!handle_dirent (dp, fltr, &st, &link_to_dir, &stale_link))
            continue;

        if (!dir_list_append (list, dp->d_name, &st, link_to_dir != 0, stale_link != 0))
        {
            mc_closedir (dirp);
            /* Norbert (Feb 12, 1997):
               Just in case someone finds this memory leak:
               -1 means big trouble (at the moment no memory left),
               I don't bother with further cleanup because if one gets to
               this point he will have more problems than a few memory
               leaks and because one 'dir_list_clean' would not be enough (and
               because I don't want to spent the time to make it working,
               IMHO it's not worthwhile).
               dir_list_clean (&dir_copy);
             */
            tree_store_end_check ();
            g_hash_table_destroy (marked_files);
            return;
        }
        fentry = &list->list[list->len - 1];

        fentry->f.marked = 0;

        /*
         * If we have marked files in the copy, scan through the copy
         * to find matching file.  Decrease number of remaining marks if
         * we copied one.
         */
        if (marked_cnt > 0 && g_hash_table_lookup (marked_files, dp->d_name) != NULL)
        {
            fentry->f.marked = 1;
            marked_cnt--;
        }

        if ((list->len & 15) == 0)
            rotate_dash (TRUE);
    }
    mc_closedir (dirp);
    tree_store_end_check ();
    g_hash_table_destroy (marked_files);

    dir_list_sort (list, sort, sort_op);

    dir_list_clean (&dir_copy);
    rotate_dash (FALSE);
}

/* --------------------------------------------------------------------------------------------- */
