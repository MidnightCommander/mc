/*
   Tree Store
   Contains a storage of the file system tree representation

   This module has been converted to be a widget.

   The program load and saves the tree each time the tree widget is
   created and destroyed.  This is required for the future vfs layer,
   it will be possible to have tree views over virtual file systems.

   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2007, 2009,
   2011
   The Free Software Foundation, Inc.

   Written by:
   Janne Kukonlehto, 1994, 1996
   Norbert Warmuth, 1997
   Miguel de Icaza, 1996, 1999

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

/** \file treestore.c
 *  \brief Source: tree store
 *
 *  Contains a storage of the file system tree representation.
 */

#include <config.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/mcconfig.h"
#include "lib/vfs/vfs.h"
#include "lib/fileloc.h"
#include "lib/strescape.h"
#include "lib/hook.h"
#include "lib/util.h"

#include "src/setup.h"

#include "treestore.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define TREE_SIGNATURE "Midnight Commander TreeStore v 2.0"

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static struct TreeStore ts;

static hook_t *remove_entry_hooks;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static tree_entry *tree_store_add_entry (const vfs_path_t * name);

/* --------------------------------------------------------------------------------------------- */

static void
tree_store_dirty (int state)
{
    ts.dirty = state;
}

/* --------------------------------------------------------------------------------------------- */
/**
  *
  * @return the number of common bytes in the strings.
  */

static size_t
str_common (const vfs_path_t * s1_vpath, const vfs_path_t * s2_vpath)
{
    size_t result = 0;
    char *s1, *fs1;
    char *s2, *fs2;

    s1 = fs1 = vfs_path_to_str (s1_vpath);
    s2 = fs2 = vfs_path_to_str (s2_vpath);

    while (*s1 != '\0' && *s2 != '\0' && *s1++ == *s2++)
        result++;

    g_free (fs1);
    g_free (fs2);

    return result;
}

/* --------------------------------------------------------------------------------------------- */
/** The directory names are arranged in a single linked list in the same
  * order as they are displayed. When the tree is displayed the expected
  * order is like this:
  * /
  * /bin
  * /etc
  * /etc/X11
  * /etc/rc.d
  * /etc.old/X11
  * /etc.old/rc.d
  * /usr
  *
  * i.e. the required collating sequence when comparing two directory names is 
  * '\0' < PATH_SEP < all-other-characters-in-encoding-order
  *
  * Since strcmp doesn't fulfil this requirement we use pathcmp when
  * inserting directory names into the list. The meaning of the return value 
  * of pathcmp and strcmp are the same (an integer less than, equal to, or 
  * greater than zero if p1 is found to be less than, to match, or be greater 
  * than p2.
 */

static int
pathcmp (const vfs_path_t * p1_vpath, const vfs_path_t * p2_vpath)
{
    int ret_val;
    char *p1, *fp1;
    char *p2, *fp2;

    p1 = fp1 = vfs_path_to_str (p1_vpath);
    p2 = fp2 = vfs_path_to_str (p2_vpath);

    for (; *p1 == *p2; p1++, p2++)
        if (*p1 == '\0')
        {
            g_free (fp1);
            g_free (fp2);
            return 0;
        }

    if (*p1 == '\0')
        ret_val = -1;
    else if (*p2 == '\0')
        ret_val = 1;
    else if (*p1 == PATH_SEP)
        ret_val = -1;
    else if (*p2 == PATH_SEP)
        ret_val = 1;
    else
        ret_val = (*p1 - *p2);

    g_free (fp1);
    g_free (fp2);
    return ret_val;
}

/* --------------------------------------------------------------------------------------------- */

static char *
decode (char *buffer)
{
    char *res = g_strdup (buffer);
    char *p, *q;

    for (p = q = res; *p; p++, q++)
    {
        if (*p == '\n')
        {
            *q = 0;
            return res;
        }

        if (*p != '\\')
        {
            *q = *p;
            continue;
        }

        p++;

        switch (*p)
        {
        case 'n':
            *q = '\n';
            break;
        case '\\':
            *q = '\\';
            break;
        }
    }
    *q = *p;

    return res;
}

/* --------------------------------------------------------------------------------------------- */
/** Loads the tree store from the specified filename */

static int
tree_store_load_from (char *name)
{
    FILE *file;
    char buffer[MC_MAXPATHLEN + 20], oldname[MC_MAXPATHLEN];
    char *different;
    int common;
    int do_load;

    g_return_val_if_fail (name != NULL, FALSE);

    if (ts.loaded)
        return TRUE;

    file = fopen (name, "r");

    if (file)
    {
        if (fgets (buffer, sizeof (buffer), file) != NULL)
        {
            if (strncmp (buffer, TREE_SIGNATURE, strlen (TREE_SIGNATURE)) != 0)
            {
                fclose (file);
                do_load = FALSE;
            }
            else
                do_load = TRUE;
        }
        else
            do_load = FALSE;
    }
    else
        do_load = FALSE;

    if (do_load)
    {
        ts.loaded = TRUE;

        /* File open -> read contents */
        oldname[0] = 0;
        while (fgets (buffer, MC_MAXPATHLEN, file))
        {
            tree_entry *e;
            int scanned;
            char *lc_name;

            /* Skip invalid records */
            if ((buffer[0] != '0' && buffer[0] != '1'))
                continue;

            if (buffer[1] != ':')
                continue;

            scanned = buffer[0] == '1';

            lc_name = decode (buffer + 2);
            if (lc_name[0] != PATH_SEP)
            {
                /* Clear-text decompression */
                char *s = strtok (lc_name, " ");

                if (s)
                {
                    common = atoi (s);
                    different = strtok (NULL, "");
                    if (different)
                    {
                        vfs_path_t *vpath;

                        vpath = vfs_path_from_str (oldname);
                        strcpy (oldname + common, different);
                        if (vfs_file_is_local (vpath))
                        {
                            vfs_path_t *tmp_vpath;

                            tmp_vpath = vfs_path_from_str (oldname);
                            e = tree_store_add_entry (tmp_vpath);
                            vfs_path_free (tmp_vpath);
                            e->scanned = scanned;
                        }
                        vfs_path_free (vpath);
                    }
                }
            }
            else
            {
                vfs_path_t *vpath;

                vpath = vfs_path_from_str (lc_name);
                if (vfs_file_is_local (vpath))
                {
                    e = tree_store_add_entry (vpath);
                    e->scanned = scanned;
                }
                vfs_path_free (vpath);
                strcpy (oldname, lc_name);
            }
            g_free (lc_name);
        }
        fclose (file);
    }

    /* Nothing loaded, we add some standard directories */
    if (!ts.tree_first)
    {
        vfs_path_t *tmp_vpath = vfs_path_from_str (PATH_SEP_STR);
        tree_store_add_entry (tmp_vpath);
        tree_store_rescan (tmp_vpath);
        vfs_path_free (tmp_vpath);
        ts.loaded = TRUE;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static char *
encode (const vfs_path_t * vpath, size_t offset)
{
    char *string, *ret_val;

    string = vfs_path_to_str (vpath);
    ret_val = strutils_escape (string + offset, -1, "\n\\", FALSE);
    g_free (string);
    return ret_val;
}

/* --------------------------------------------------------------------------------------------- */
/** Saves the tree to the specified filename */

static int
tree_store_save_to (char *name)
{
    tree_entry *current;
    FILE *file;

    file = fopen (name, "w");
    if (!file)
        return errno;

    fprintf (file, "%s\n", TREE_SIGNATURE);

    current = ts.tree_first;
    while (current)
    {
        int i, common;

        if (vfs_file_is_local (current->name))
        {
            /* Clear-text compression */
            if (current->prev && (common = str_common (current->prev->name, current->name)) > 2)
            {
                char *encoded = encode (current->name, common);

                i = fprintf (file, "%d:%d %s\n", current->scanned, common, encoded);
                g_free (encoded);
            }
            else
            {
                char *encoded = encode (current->name, 0);

                i = fprintf (file, "%d:%s\n", current->scanned, encoded);
                g_free (encoded);
            }

            if (i == EOF)
            {
                fprintf (stderr, _("Cannot write to the %s file:\n%s\n"),
                         name, unix_error_string (errno));
                break;
            }
        }
        current = current->next;
    }
    tree_store_dirty (FALSE);
    fclose (file);

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static tree_entry *
tree_store_add_entry (const vfs_path_t * name)
{
    int flag = -1;
    tree_entry *current = ts.tree_first;
    tree_entry *old = NULL;
    tree_entry *new;
    int submask = 0;

    if (ts.tree_last && ts.tree_last->next)
        abort ();

    /* Search for the correct place */
    while (current != NULL && (flag = pathcmp (current->name, name)) < 0)
    {
        old = current;
        current = current->next;
    }

    if (flag == 0)
        return current;         /* Already in the list */

    /* Not in the list -> add it */
    new = g_new0 (tree_entry, 1);
    if (!current)
    {
        /* Append to the end of the list */
        if (!ts.tree_first)
        {
            /* Empty list */
            ts.tree_first = new;
            new->prev = NULL;
        }
        else
        {
            if (old != NULL)
                old->next = new;
            new->prev = old;
        }
        new->next = NULL;
        ts.tree_last = new;
    }
    else
    {
        /* Insert in to the middle of the list */
        new->prev = old;
        if (old != NULL)
        {
            /* Yes, in the middle */
            new->next = old->next;
            old->next = new;
        }
        else
        {
            /* Nope, in the beginning of the list */
            new->next = ts.tree_first;
            ts.tree_first = new;
        }
        new->next->prev = new;
    }

    /* Calculate attributes */
    new->name = vfs_path_clone (name);
    new->sublevel = vfs_path_tokens_count (new->name);
    {
        const char *new_name;

        new_name = vfs_path_get_last_path_str (new->name);
        new->subname = strrchr (new_name, '/');
        if (new->subname == NULL)
            new->subname = new_name;
        else
            new->subname++;
    }
    if (new->next)
        submask = new->next->submask;
    else
        submask = 0;
    submask |= 1 << new->sublevel;
    submask &= (2 << new->sublevel) - 1;
    new->submask = submask;
    new->mark = 0;

    /* Correct the submasks of the previous entries */
    current = new->prev;
    while (current && current->sublevel > new->sublevel)
    {
        current->submask |= 1 << new->sublevel;
        current = current->prev;
    }

    tree_store_dirty (TRUE);
    return new;
}

/* --------------------------------------------------------------------------------------------- */

static void
tree_store_notify_remove (tree_entry * entry)
{
    hook_t *p = remove_entry_hooks;
    tree_store_remove_fn r;

    while (p != NULL)
    {
        r = (tree_store_remove_fn) p->hook_fn;
        r (entry, p->hook_data);
        p = p->next;
    }
}

/* --------------------------------------------------------------------------------------------- */

static tree_entry *
remove_entry (tree_entry * entry)
{
    tree_entry *current = entry->prev;
    long submask = 0;
    tree_entry *ret = NULL;

    tree_store_notify_remove (entry);

    /* Correct the submasks of the previous entries */
    if (entry->next)
        submask = entry->next->submask;
    while (current && current->sublevel > entry->sublevel)
    {
        submask |= 1 << current->sublevel;
        submask &= (2 << current->sublevel) - 1;
        current->submask = submask;
        current = current->prev;
    }

    /* Unlink the entry from the list */
    if (entry->prev)
        entry->prev->next = entry->next;
    else
        ts.tree_first = entry->next;

    if (entry->next)
        entry->next->prev = entry->prev;
    else
        ts.tree_last = entry->prev;

    /* Free the memory used by the entry */
    g_free (entry->name);
    g_free (entry);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static void
process_special_dirs (GList ** special_dirs, char *file)
{
    gchar **buffers, **start_buff;
    mc_config_t *cfg;
    gsize buffers_len;

    cfg = mc_config_init (file, TRUE);
    if (cfg == NULL)
        return;

    start_buff = buffers = mc_config_get_string_list (cfg, "Special dirs", "list", &buffers_len);
    if (buffers != NULL)
    {
        while (*buffers != NULL)
        {
            *special_dirs = g_list_prepend (*special_dirs, *buffers);
            *buffers = NULL;
            buffers++;
        }
        g_strfreev (start_buff);
    }
    mc_config_deinit (cfg);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
should_skip_directory (const vfs_path_t * vpath)
{
    static GList *special_dirs = NULL;
    GList *l;
    static gboolean loaded = FALSE;
    char *dir;
    gboolean ret = FALSE;

    if (!loaded)
    {
        loaded = TRUE;
        setup_init ();
        process_special_dirs (&special_dirs, profile_name);
        process_special_dirs (&special_dirs, global_profile_name);
    }

    dir = vfs_path_to_str (vpath);
    for (l = special_dirs; l != NULL; l = g_list_next (l))
        if (strncmp (dir, l->data, strlen (l->data)) == 0)
        {
            ret = TRUE;
            break;
        }

    g_free (dir);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* Searches for specified directory */
tree_entry *
tree_store_whereis (const vfs_path_t * name)
{
    tree_entry *current = ts.tree_first;
    int flag = -1;

    while (current && (flag = pathcmp (current->name, name)) < 0)
        current = current->next;

    if (flag == 0)
        return current;
    else
        return NULL;
}

/* --------------------------------------------------------------------------------------------- */

struct TreeStore *
tree_store_get (void)
{
    return &ts;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * \fn int tree_store_load(void)
 * \brief Loads the tree from the default location
 * \return 1 if success (true), 0 otherwise (false)
 */

int
tree_store_load (void)
{
    char *name;
    int retval;

    name = mc_config_get_full_path (MC_TREESTORE_FILE);
    retval = tree_store_load_from (name);
    g_free (name);

    return retval;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * \fn int tree_store_save(void)
 * \brief Saves the tree to the default file in an atomic fashion
 * \return 0 if success, errno on error
 */

int
tree_store_save (void)
{
    char *name;
    int retval;

    name = mc_config_get_full_path (MC_TREESTORE_FILE);
    mc_util_make_backup_if_possible (name, ".tmp");

    retval = tree_store_save_to (name);
    if (retval != 0)
    {
        mc_util_restore_from_backup_if_possible (name, ".tmp");
        g_free (name);
        return retval;
    }

    mc_util_unlink_backup_if_possible (name, ".tmp");
    g_free (name);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

void
tree_store_add_entry_remove_hook (tree_store_remove_fn callback, void *data)
{
    add_hook (&remove_entry_hooks, (void (*)(void *)) callback, data);
}

/* --------------------------------------------------------------------------------------------- */

void
tree_store_remove_entry_remove_hook (tree_store_remove_fn callback)
{
    delete_hook (&remove_entry_hooks, (void (*)(void *)) callback);
}


/* --------------------------------------------------------------------------------------------- */

void
tree_store_remove_entry (const vfs_path_t * name_vpath)
{
    tree_entry *current, *base, *old;
    size_t len;

    g_return_if_fail (name_vpath != NULL);

    /* Miguel Ugly hack */
    {
        char *name;
        gboolean is_root;

        name = vfs_path_to_str (name_vpath);
        is_root = (name[0] == PATH_SEP && name[1] == '\0');
        g_free (name);
        if (is_root)
            return;
    }
    /* Miguel Ugly hack end */

    base = tree_store_whereis (name_vpath);
    if (!base)
        return;                 /* Doesn't exist */

    len = vfs_path_len (base->name);
    current = base->next;
    while (current != NULL && vfs_path_equal_len (current->name, base->name, len))
    {
        char *current_name;
        gboolean ok;

        current_name = vfs_path_to_str (current->name);
        ok = (current_name[len] == '\0' || current_name[len] == PATH_SEP);
        g_free (current_name);
        if (!ok)
            break;
        old = current;
        current = current->next;
        remove_entry (old);
    }
    remove_entry (base);
    tree_store_dirty (TRUE);
}

/* --------------------------------------------------------------------------------------------- */
/** This subdirectory exists -> clear deletion mark */

void
tree_store_mark_checked (const char *subname)
{
    vfs_path_t *name;
    char *check_name;
    tree_entry *current, *base;
    int flag = 1;
    if (!ts.loaded)
        return;

    if (ts.check_name == NULL)
        return;

    /* Calculate the full name of the subdirectory */
    if (subname[0] == '.' && (subname[1] == 0 || (subname[1] == '.' && subname[2] == 0)))
        return;

    check_name = vfs_path_to_str (ts.check_name);
    if (check_name[0] == PATH_SEP && check_name[1] == 0)
        name = vfs_path_build_filename (PATH_SEP_STR, subname, NULL);
    else
        name = vfs_path_append_new (ts.check_name, subname, NULL);
    g_free (check_name);

    /* Search for the subdirectory */
    current = ts.check_start;
    while (current && (flag = pathcmp (current->name, name)) < 0)
        current = current->next;

    if (flag != 0)
    {
        /* Doesn't exist -> add it */
        current = tree_store_add_entry (name);
        ts.add_queue_vpath = g_list_prepend (ts.add_queue_vpath, vfs_path_clone (name));
    }
    g_free (name);

    /* Clear the deletion mark from the subdirectory and its children */
    base = current;
    if (base)
    {
        size_t len;

        len = vfs_path_len (base->name);
        base->mark = 0;
        current = base->next;
        while (current != NULL && vfs_path_equal_len (current->name, base->name, len))
        {
            gboolean ok;

            check_name = vfs_path_to_str (current->name);
            ok = (check_name[len] == '\0' || check_name[len] == PATH_SEP || len == 1);
            g_free (check_name);
            if (!ok)
                break;
            current->mark = 0;
            current = current->next;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */
/** Mark the subdirectories of the current directory for delete */

tree_entry *
tree_store_start_check (const vfs_path_t * vpath)
{
    tree_entry *current, *retval;
    size_t len;

    if (!ts.loaded)
        return NULL;

    g_return_val_if_fail (ts.check_name == NULL, NULL);
    ts.check_start = NULL;

    /* Search for the start of subdirectories */
    current = tree_store_whereis (vpath);
    if (!current)
    {
        struct stat s;

        if (mc_stat (vpath, &s) == -1)
            return NULL;

        if (!S_ISDIR (s.st_mode))
            return NULL;

        current = tree_store_add_entry (vpath);
        ts.check_name = vfs_path_clone (vpath);

        return current;
    }

    ts.check_name = vfs_path_clone (vpath);

    retval = current;

    /* Mark old subdirectories for delete */
    ts.check_start = current->next;
    len = vfs_path_len (ts.check_name);

    current = ts.check_start;
    while (current != NULL && vfs_path_equal_len (current->name, ts.check_name, len))
    {
        char *current_name;
        gboolean ok;

        current_name = vfs_path_to_str (current->name);
        ok = (current_name[len] == '\0' || current_name[len] == PATH_SEP || len == 1);
        g_free (current_name);
        if (!ok)
            break;
        current->mark = 1;
        current = current->next;
    }

    return retval;
}

/* --------------------------------------------------------------------------------------------- */
/** Delete subdirectories which still have the deletion mark */

void
tree_store_end_check (void)
{
    tree_entry *current, *old;
    size_t len;
    GList *the_queue;

    if (!ts.loaded)
        return;

    g_return_if_fail (ts.check_name != NULL);

    /* Check delete marks and delete if found */
    len = vfs_path_len (ts.check_name);

    current = ts.check_start;
    while (current != NULL && vfs_path_equal_len (current->name, ts.check_name, len))
    {
        char *current_name;
        gboolean ok;

        current_name = vfs_path_to_str (current->name);
        ok = (current_name[len] == '\0' || current_name[len] == PATH_SEP || len == 1);
        g_free (current_name);

        if (!ok)
            break;
        old = current;
        current = current->next;
        if (old->mark)
            remove_entry (old);
    }

    /* get the stuff in the scan order */
    ts.add_queue_vpath = g_list_reverse (ts.add_queue_vpath);
    the_queue = ts.add_queue_vpath;
    ts.add_queue_vpath = NULL;
    vfs_path_free (ts.check_name);
    ts.check_name = NULL;

    g_list_foreach (the_queue, (GFunc) vfs_path_free, NULL);
    g_list_free (the_queue);
}

/* --------------------------------------------------------------------------------------------- */

tree_entry *
tree_store_rescan (const vfs_path_t * vpath)
{
    DIR *dirp;
    struct dirent *dp;
    struct stat buf;
    tree_entry *entry;

    if (should_skip_directory (vpath))
    {
        entry = tree_store_add_entry (vpath);
        entry->scanned = 1;
        return entry;
    }

    entry = tree_store_start_check (vpath);
    if (entry == NULL)
        return NULL;

    dirp = mc_opendir (vpath);
    if (dirp)
    {
        for (dp = mc_readdir (dirp); dp; dp = mc_readdir (dirp))
        {
            vfs_path_t *tmp_vpath;

            if (dp->d_name[0] == '.')
            {
                if (dp->d_name[1] == 0 || (dp->d_name[1] == '.' && dp->d_name[2] == 0))
                    continue;
            }

            tmp_vpath = vfs_path_append_new (vpath, dp->d_name, NULL);
            if (mc_lstat (tmp_vpath, &buf) != -1)
            {
                if (S_ISDIR (buf.st_mode))
                    tree_store_mark_checked (dp->d_name);
            }
            vfs_path_free (tmp_vpath);
        }
        mc_closedir (dirp);
    }
    tree_store_end_check ();
    entry->scanned = 1;

    return entry;
}

/* --------------------------------------------------------------------------------------------- */
