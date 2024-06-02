/*
   Directory routines

   Copyright (C) 1994-2024
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2013
   Andrew Borodin <aborodin@vmail.ru>, 2013-2022

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

#include "src/setup.h"          /* panels_options */

#include "treestore.h"
#include "file.h"               /* file_is_symlink_to_dir() */
#include "dir.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define MY_ISDIR(x) (\
    (is_exe (x->st.st_mode) && !(S_ISDIR (x->st.st_mode) || link_isdir (x)) && exec_first) \
        ? 1 \
        : ( (S_ISDIR (x->st.st_mode) || link_isdir (x)) ? 2 : 0) )

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* Reverse flag */
static int reverse = 1;

/* Are the files sorted case sensitively? */
static gboolean case_sensitive = OS_SORT_CASE_SENSITIVE_DEFAULT;

/* Are the exec_bit files top in list */
static gboolean exec_first = TRUE;

static dir_list dir_copy = { NULL, 0, 0, NULL };

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

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

static inline int
compare_by_names (file_entry_t *a, file_entry_t *b)
{
    /* create key if does not exist, key will be freed after sorting */
    if (a->name_sort_key == NULL)
        a->name_sort_key = str_create_key_for_filename (a->fname->str, case_sensitive);
    if (b->name_sort_key == NULL)
        b->name_sort_key = str_create_key_for_filename (b->fname->str, case_sensitive);

    return key_collate (a->name_sort_key, b->name_sort_key);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * clear keys, should be call after sorting is finished.
 */

static void
clean_sort_keys (dir_list *list, int start, int count)
{
    int i;

    for (i = 0; i < count; i++)
    {
        file_entry_t *fentry;

        fentry = &list->list[i + start];
        str_release_key (fentry->name_sort_key, case_sensitive);
        fentry->name_sort_key = NULL;
        str_release_key (fentry->extension_sort_key, case_sensitive);
        fentry->extension_sort_key = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * If you change handle_dirent then check also handle_path.
 * @return FALSE = don't add, TRUE = add to the list
 */

static gboolean
handle_dirent (struct vfs_dirent *dp, const file_filter_t *filter, struct stat *buf1,
               gboolean *link_to_dir, gboolean *stale_link)
{
    vfs_path_t *vpath;
    gboolean ok = TRUE;

    if (DIR_IS_DOT (dp->d_name) || DIR_IS_DOTDOT (dp->d_name))
        return FALSE;
    if (!panels_options.show_dot_files && (dp->d_name[0] == '.'))
        return FALSE;
    if (!panels_options.show_backups && dp->d_name[dp->d_len - 1] == '~')
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
    *link_to_dir = file_is_symlink_to_dir (vpath, buf1, stale_link);

    vfs_path_free (vpath, TRUE);

    if (filter != NULL && filter->handler != NULL)
    {
        gboolean files_only = (filter->flags & SELECT_FILES_ONLY) != 0;

        ok = ((S_ISDIR (buf1->st_mode) || *link_to_dir) && files_only)
            || mc_search_run (filter->handler, dp->d_name, 0, dp->d_len, NULL);
    }

    return ok;
}

/* --------------------------------------------------------------------------------------------- */
/** get info about ".." */

static gboolean
dir_get_dotdot_stat (const vfs_path_t *vpath, struct stat *st)
{
    gboolean ret = FALSE;

    if ((vpath != NULL) && (st != NULL))
    {
        const char *path;

        path = vfs_path_get_by_index (vpath, 0)->path;
        if (path != NULL && *path != '\0')
        {
            vfs_path_t *tmp_vpath;

            tmp_vpath = vfs_path_append_new (vpath, "..", (char *) NULL);
            ret = mc_stat (tmp_vpath, st) == 0;
            vfs_path_free (tmp_vpath, TRUE);
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
            dir_list_free_list (&dir_copy);

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
dir_list_grow (dir_list *list, int delta)
{
    int size;
    gboolean clear_flag = FALSE;

    if (list == NULL)
        return FALSE;

    if (delta == 0)
        return TRUE;

    size = list->size + delta;
    if (size <= 0)
    {
        size = DIR_LIST_MIN_SIZE;
        clear_flag = TRUE;
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

    list->len = clear_flag ? 0 : MIN (list->len, size);

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
dir_list_append (dir_list *list, const char *fname, const struct stat *st,
                 gboolean link_to_dir, gboolean stale_link)
{
    file_entry_t *fentry;

    /* Need to grow the *list? */
    if (list->len == list->size && !dir_list_grow (list, DIR_LIST_RESIZE_STEP))
        return FALSE;

    fentry = &list->list[list->len];
    fentry->fname = g_string_new (fname);
    fentry->f.marked = 0;
    fentry->f.link_to_dir = link_to_dir ? 1 : 0;
    fentry->f.stale_link = stale_link ? 1 : 0;
    fentry->f.dir_size_computed = 0;
    fentry->st = *st;
    fentry->name_sort_key = NULL;
    fentry->extension_sort_key = NULL;

    list->len++;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

int
unsorted (file_entry_t *a, file_entry_t *b)
{
    (void) a;
    (void) b;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

int
sort_name (file_entry_t *a, file_entry_t *b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || panels_options.mix_all_files)
        return compare_by_names (a, b);

    return bd - ad;
}

/* --------------------------------------------------------------------------------------------- */

int
sort_vers (file_entry_t *a, file_entry_t *b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || panels_options.mix_all_files)
    {
        int result;

        result = filevercmp (a->fname->str, b->fname->str);
        if (result != 0)
            return result * reverse;

        return compare_by_names (a, b);
    }

    return bd - ad;
}

/* --------------------------------------------------------------------------------------------- */

int
sort_ext (file_entry_t *a, file_entry_t *b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || panels_options.mix_all_files)
    {
        int r;

        if (a->extension_sort_key == NULL)
            a->extension_sort_key = str_create_key (extension (a->fname->str), case_sensitive);
        if (b->extension_sort_key == NULL)
            b->extension_sort_key = str_create_key (extension (b->fname->str), case_sensitive);

        r = str_key_collate (a->extension_sort_key, b->extension_sort_key, case_sensitive);
        if (r != 0)
            return r * reverse;

        return compare_by_names (a, b);
    }

    return bd - ad;
}

/* --------------------------------------------------------------------------------------------- */

int
sort_time (file_entry_t *a, file_entry_t *b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || panels_options.mix_all_files)
    {
        int result = _GL_CMP (a->st.st_mtime, b->st.st_mtime);

        if (result != 0)
            return result * reverse;

        return compare_by_names (a, b);
    }

    return bd - ad;
}

/* --------------------------------------------------------------------------------------------- */

int
sort_ctime (file_entry_t *a, file_entry_t *b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || panels_options.mix_all_files)
    {
        int result = _GL_CMP (a->st.st_ctime, b->st.st_ctime);

        if (result != 0)
            return result * reverse;

        return compare_by_names (a, b);
    }

    return bd - ad;
}

/* --------------------------------------------------------------------------------------------- */

int
sort_atime (file_entry_t *a, file_entry_t *b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || panels_options.mix_all_files)
    {
        int result = _GL_CMP (a->st.st_atime, b->st.st_atime);

        if (result != 0)
            return result * reverse;

        return compare_by_names (a, b);
    }

    return bd - ad;
}

/* --------------------------------------------------------------------------------------------- */

int
sort_inode (file_entry_t *a, file_entry_t *b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || panels_options.mix_all_files)
        return (a->st.st_ino - b->st.st_ino) * reverse;

    return bd - ad;
}

/* --------------------------------------------------------------------------------------------- */

int
sort_size (file_entry_t *a, file_entry_t *b)
{
    int ad = MY_ISDIR (a);
    int bd = MY_ISDIR (b);

    if (ad == bd || panels_options.mix_all_files)
    {
        int result = _GL_CMP (a->st.st_size, b->st.st_size);

        if (result != 0)
            return result * reverse;

        return compare_by_names (a, b);
    }

    return bd - ad;
}

/* --------------------------------------------------------------------------------------------- */

void
dir_list_sort (dir_list *list, GCompareFunc sort, const dir_sort_options_t *sort_op)
{
    if (list->len > 1 && sort != (GCompareFunc) unsorted)
    {
        file_entry_t *fentry = &list->list[0];
        int dot_dot_found;

        /* If there is an ".." entry the caller must take care to
           ensure that it occupies the first list element. */
        dot_dot_found = DIR_IS_DOTDOT (fentry->fname->str) ? 1 : 0;
        reverse = sort_op->reverse ? -1 : 1;
        case_sensitive = sort_op->case_sensitive ? 1 : 0;
        exec_first = sort_op->exec_first;
        qsort (&(list->list)[dot_dot_found], list->len - dot_dot_found, sizeof (file_entry_t),
               sort);

        clean_sort_keys (list, dot_dot_found, list->len - dot_dot_found);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
dir_list_clean (dir_list *list)
{
    int i;

    for (i = 0; i < list->len; i++)
    {
        file_entry_t *fentry;

        fentry = &list->list[i];
        g_string_free (fentry->fname, TRUE);
        fentry->fname = NULL;
    }

    list->len = 0;
    /* reduce memory usage */
    dir_list_grow (list, DIR_LIST_MIN_SIZE - list->size);
}

/* --------------------------------------------------------------------------------------------- */

void
dir_list_free_list (dir_list *list)
{
    int i;

    for (i = 0; i < list->len; i++)
    {
        file_entry_t *fentry;

        fentry = &list->list[i];
        g_string_free (fentry->fname, TRUE);
    }

    MC_PTR_FREE (list->list);
    list->len = 0;
    list->size = 0;
}

/* --------------------------------------------------------------------------------------------- */
/** Used to set up a directory list when there is no access to a directory */

gboolean
dir_list_init (dir_list *list)
{
    file_entry_t *fentry;

    /* Need to grow the *list? */
    if (list->size == 0 && !dir_list_grow (list, DIR_LIST_RESIZE_STEP))
    {
        list->len = 0;
        return FALSE;
    }

    fentry = &list->list[0];
    memset (fentry, 0, sizeof (*fentry));
    fentry->fname = g_string_new ("..");
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
handle_path (const char *path, struct stat *buf1, gboolean *link_to_dir, gboolean *stale_link)
{
    vfs_path_t *vpath;

    if (DIR_IS_DOT (path) || DIR_IS_DOTDOT (path))
        return FALSE;

    vpath = vfs_path_from_str (path);
    if (mc_lstat (vpath, buf1) == -1)
    {
        vfs_path_free (vpath, TRUE);
        return FALSE;
    }

    if (S_ISDIR (buf1->st_mode))
        tree_store_mark_checked (path);

    /* A link to a file or a directory? */
    *link_to_dir = FALSE;
    *stale_link = FALSE;
    if (S_ISLNK (buf1->st_mode))
    {
        struct stat buf2;

        if (mc_stat (vpath, &buf2) == 0)
            *link_to_dir = S_ISDIR (buf2.st_mode) != 0;
        else
            *stale_link = TRUE;
    }

    vfs_path_free (vpath, TRUE);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
dir_list_load (dir_list *list, const vfs_path_t *vpath, GCompareFunc sort,
               const dir_sort_options_t *sort_op, const file_filter_t *filter)
{
    DIR *dirp;
    struct vfs_dirent *dp;
    struct stat st;
    file_entry_t *fentry;
    const char *vpath_str;
    gboolean ret = TRUE;

    /* ".." (if any) must be the first entry in the list */
    if (!dir_list_init (list))
        return FALSE;

    fentry = &list->list[0];
    if (dir_get_dotdot_stat (vpath, &st))
        fentry->st = st;

    if (list->callback != NULL)
        list->callback (DIR_OPEN, (void *) vpath);
    dirp = mc_opendir (vpath);
    if (dirp == NULL)
        return FALSE;

    tree_store_start_check (vpath);

    vpath_str = vfs_path_as_str (vpath);
    /* Do not add a ".." entry to the root directory */
    if (IS_PATH_SEP (vpath_str[0]) && vpath_str[1] == '\0')
        dir_list_clean (list);

    while (ret && (dp = mc_readdir (dirp)) != NULL)
    {
        gboolean link_to_dir, stale_link;

        if (list->callback != NULL)
            list->callback (DIR_READ, dp);

        if (!handle_dirent (dp, filter, &st, &link_to_dir, &stale_link))
            continue;

        if (!dir_list_append (list, dp->d_name, &st, link_to_dir, stale_link))
            ret = FALSE;
    }

    if (ret)
        dir_list_sort (list, sort, sort_op);

    if (list->callback != NULL)
        list->callback (DIR_CLOSE, NULL);
    mc_closedir (dirp);
    tree_store_end_check ();

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
if_link_is_exe (const vfs_path_t *full_name_vpath, const file_entry_t *file)
{
    struct stat b;

    if (S_ISLNK (file->st.st_mode) && mc_stat (full_name_vpath, &b) == 0)
        return is_exe (b.st_mode);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/** If filter is null, then it is a match */

gboolean
dir_list_reload (dir_list *list, const vfs_path_t *vpath, GCompareFunc sort,
                 const dir_sort_options_t *sort_op, const file_filter_t *filter)
{
    DIR *dirp;
    struct vfs_dirent *dp;
    int i;
    struct stat st;
    int marked_cnt;
    GHashTable *marked_files;
    const char *tmp_path;
    gboolean ret = TRUE;

    if (list->callback != NULL)
        list->callback (DIR_OPEN, (void *) vpath);
    dirp = mc_opendir (vpath);
    if (dirp == NULL)
    {
        dir_list_clean (list);
        dir_list_init (list);
        return FALSE;
    }

    tree_store_start_check (vpath);

    marked_files = g_hash_table_new (g_str_hash, g_str_equal);
    alloc_dir_copy (list->len);
    for (marked_cnt = i = 0; i < list->len; i++)
    {
        file_entry_t *fentry, *dfentry;

        fentry = &list->list[i];
        dfentry = &dir_copy.list[i];

        dfentry->fname = mc_g_string_dup (fentry->fname);
        dfentry->f.marked = fentry->f.marked;
        dfentry->f.dir_size_computed = fentry->f.dir_size_computed;
        dfentry->f.link_to_dir = fentry->f.link_to_dir;
        dfentry->f.stale_link = fentry->f.stale_link;
        dfentry->name_sort_key = NULL;
        dfentry->extension_sort_key = NULL;
        if (fentry->f.marked != 0)
        {
            g_hash_table_insert (marked_files, dfentry->fname->str, dfentry);
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
            dir_list_free_list (&dir_copy);
            mc_closedir (dirp);
            return FALSE;
        }

        if (dir_get_dotdot_stat (vpath, &st))
        {
            file_entry_t *fentry;

            fentry = &list->list[0];
            fentry->st = st;
        }
    }

    while (ret && (dp = mc_readdir (dirp)) != NULL)
    {
        gboolean link_to_dir, stale_link;

        if (list->callback != NULL)
            list->callback (DIR_READ, dp);

        if (!handle_dirent (dp, filter, &st, &link_to_dir, &stale_link))
            continue;

        if (!dir_list_append (list, dp->d_name, &st, link_to_dir, stale_link))
            ret = FALSE;
        else
        {
            file_entry_t *fentry;

            fentry = &list->list[list->len - 1];

            /*
             * If we have marked files in the copy, scan through the copy
             * to find matching file.  Decrease number of remaining marks if
             * we copied one.
             */
            fentry->f.marked = (marked_cnt > 0
                                && g_hash_table_lookup (marked_files, dp->d_name) != NULL) ? 1 : 0;
            if (fentry->f.marked != 0)
                marked_cnt--;
        }
    }

    if (ret)
        dir_list_sort (list, sort, sort_op);

    if (list->callback != NULL)
        list->callback (DIR_CLOSE, NULL);
    mc_closedir (dirp);
    tree_store_end_check ();

    g_hash_table_destroy (marked_files);
    dir_list_free_list (&dir_copy);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

void
file_filter_clear (file_filter_t *filter)
{
    MC_PTR_FREE (filter->value);
    mc_search_free (filter->handler);
    filter->handler = NULL;
    /* keep filter->flags */
}

/* --------------------------------------------------------------------------------------------- */
