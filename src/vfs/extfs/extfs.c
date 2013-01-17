/*
   Virtual File System: External file system.

   Copyright (C) 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2006, 2007, 2009, 2011
   The Free Software Foundation, Inc.

   Written by:
   Jakub Jelinek, 1995
   Pavel Machek, 1998
   Andrew T. Veliath, 1999

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

/**
 * \file
 * \brief Source: Virtual File System: External file system
 * \author Jakub Jelinek
 * \author Pavel Machek
 * \author Andrew T. Veliath
 * \date 1995, 1998, 1999
 */

/* Namespace: init_extfs */

#include <config.h>

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/fileloc.h"
#include "lib/mcconfig.h"
#include "lib/util.h"
#include "lib/widget.h"         /* message() */

#include "src/execute.h"        /* For shell_execute */

#include "lib/vfs/vfs.h"
#include "lib/vfs/utilvfs.h"
#include "lib/vfs/gc.h"         /* vfs_rmstamp */

#include "extfs.h"

/*** global variables ****************************************************************************/

GArray *extfs_plugins = NULL;

/*** file scope macro definitions ****************************************************************/

#undef ERRNOR
#define ERRNOR(x,y) do { my_errno = x; return y; } while(0)

#define RECORDSIZE 512

/*** file scope type declarations ****************************************************************/

struct inode
{
    nlink_t nlink;
    struct entry *first_in_subdir;      /* only used if this is a directory */
    struct entry *last_in_subdir;
    ino_t inode;                /* This is inode # */
    dev_t dev;                  /* This is an internal identification of the extfs archive */
    struct archive *archive;    /* And this is an archive structure */
    dev_t rdev;
    mode_t mode;
    uid_t uid;
    gid_t gid;
    off_t size;
    time_t mtime;
    char *linkname;
    time_t atime;
    time_t ctime;
    char *local_filename;
};

struct entry
{
    struct entry *next_in_dir;
    struct entry *dir;
    char *name;
    struct inode *inode;
};

struct pseudofile
{
    struct archive *archive;
    gboolean has_changed;
    int local_handle;
    struct entry *entry;
};

struct archive
{
    int fstype;
    char *name;
    char *local_name;
    struct stat local_stat;
    dev_t rdev;
    int fd_usage;
    ino_t inode_counter;
    struct entry *root_entry;
    struct archive *next;
};

typedef struct
{
    char *path;
    char *prefix;
    gboolean need_archive;
} extfs_plugin_info_t;

/*** file scope variables ************************************************************************/

static gboolean errloop;
static gboolean notadir;

static struct vfs_class vfs_extfs_ops;
static struct archive *first_archive = NULL;
static int my_errno = 0;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void extfs_remove_entry (struct entry *e);
static void extfs_free (vfsid id);
static void extfs_free_entry (struct entry *e);
static struct entry *extfs_resolve_symlinks_int (struct entry *entry, GSList * list);

/* --------------------------------------------------------------------------------------------- */

static void
extfs_make_dots (struct entry *ent)
{
    struct entry *entry = g_new (struct entry, 1);
    struct entry *parentry = ent->dir;
    struct inode *inode = ent->inode, *parent;

    parent = (parentry != NULL) ? parentry->inode : NULL;
    entry->name = g_strdup (".");
    entry->inode = inode;
    entry->dir = ent;
    inode->local_filename = NULL;
    inode->first_in_subdir = entry;
    inode->nlink++;

    entry->next_in_dir = g_new (struct entry, 1);
    entry = entry->next_in_dir;
    entry->name = g_strdup ("..");
    inode->last_in_subdir = entry;
    entry->next_in_dir = NULL;
    if (parent != NULL)
    {
        entry->inode = parent;
        entry->dir = parentry;
        parent->nlink++;
    }
    else
    {
        entry->inode = inode;
        entry->dir = ent;
        inode->nlink++;
    }
}

/* --------------------------------------------------------------------------------------------- */

static struct entry *
extfs_generate_entry (struct archive *archive,
                      const char *name, struct entry *parentry, mode_t mode)
{
    mode_t myumask;
    struct inode *inode, *parent;
    struct entry *entry;

    parent = (parentry != NULL) ? parentry->inode : NULL;
    entry = g_new (struct entry, 1);

    entry->name = g_strdup (name);
    entry->next_in_dir = NULL;
    entry->dir = parentry;
    if (parent != NULL)
    {
        parent->last_in_subdir->next_in_dir = entry;
        parent->last_in_subdir = entry;
    }
    inode = g_new (struct inode, 1);
    entry->inode = inode;
    inode->local_filename = NULL;
    inode->linkname = NULL;
    inode->last_in_subdir = NULL;
    inode->inode = (archive->inode_counter)++;
    inode->dev = archive->rdev;
    inode->archive = archive;
    myumask = umask (022);
    umask (myumask);
    inode->mode = mode & ~myumask;
    mode = inode->mode;
    inode->rdev = 0;
    inode->uid = getuid ();
    inode->gid = getgid ();
    inode->size = 0;
    inode->mtime = time (NULL);
    inode->atime = inode->mtime;
    inode->ctime = inode->mtime;
    inode->nlink = 1;
    if (S_ISDIR (mode))
        extfs_make_dots (entry);
    return entry;
}

/* --------------------------------------------------------------------------------------------- */

static struct entry *
extfs_find_entry_int (struct entry *dir, const char *name, GSList * list,
                      gboolean make_dirs, gboolean make_file)
{
    struct entry *pent, *pdir;
    const char *p, *name_end;
    char *q;
    char c = PATH_SEP;

    if (g_path_is_absolute (name))
    {
        /* Handle absolute paths */
        name = (char *) g_path_skip_root (name);
        dir = dir->inode->archive->root_entry;
    }

    pent = dir;
    p = name;
    name_end = name + strlen (name);

    q = strchr (p, PATH_SEP);
    if (q == '\0')
        q = strchr (p, '\0');

    while ((pent != NULL) && (c != '\0') && (*p != '\0'))
    {
        c = *q;
        *q = '\0';

        if (strcmp (p, ".") != 0)
        {
            if (strcmp (p, "..") == 0)
                pent = pent->dir;
            else
            {
                pent = extfs_resolve_symlinks_int (pent, list);
                if (pent == NULL)
                {
                    *q = c;
                    return NULL;
                }
                if (!S_ISDIR (pent->inode->mode))
                {
                    *q = c;
                    notadir = TRUE;
                    return NULL;
                }

                pdir = pent;
                for (pent = pent->inode->first_in_subdir; pent != NULL; pent = pent->next_in_dir)
                    /* Hack: I keep the original semanthic unless
                       q+1 would break in the strchr */
                    if (strcmp (pent->name, p) == 0)
                    {
                        if (q + 1 > name_end)
                        {
                            *q = c;
                            notadir = !S_ISDIR (pent->inode->mode);
                            return pent;
                        }
                        break;
                    }

                /* When we load archive, we create automagically
                 * non-existant directories
                 */
                if (pent == NULL && make_dirs)
                    pent = extfs_generate_entry (dir->inode->archive, p, pdir, S_IFDIR | 0777);
                if (pent == NULL && make_file)
                    pent = extfs_generate_entry (dir->inode->archive, p, pdir, S_IFREG | 0666);
            }
        }
        /* Next iteration */
        *q = c;
        p = q + 1;
        q = strchr (p, PATH_SEP);
        if (q == '\0')
            q = strchr (p, '\0');
    }
    if (pent == NULL)
        my_errno = ENOENT;
    return pent;
}

/* --------------------------------------------------------------------------------------------- */

static struct entry *
extfs_find_entry (struct entry *dir, const char *name, gboolean make_dirs, gboolean make_file)
{
    struct entry *res;

    errloop = FALSE;
    notadir = FALSE;

    res = extfs_find_entry_int (dir, name, NULL, make_dirs, make_file);
    if (res == NULL)
    {
        if (errloop)
            my_errno = ELOOP;
        else if (notadir)
            my_errno = ENOTDIR;
    }
    return res;
}

/* --------------------------------------------------------------------------------------------- */

static void
extfs_fill_names (struct vfs_class *me, fill_names_f func)
{
    struct archive *a = first_archive;

    (void) me;

    while (a != NULL)
    {
        extfs_plugin_info_t *info;
        char *name;

        info = &g_array_index (extfs_plugins, extfs_plugin_info_t, a->fstype);
        name =
            g_strconcat (a->name ? a->name : "", "/", info->prefix, VFS_PATH_URL_DELIMITER,
                         (char *) NULL);
        func (name);
        g_free (name);
        a = a->next;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
extfs_free_archive (struct archive *archive)
{
    extfs_free_entry (archive->root_entry);
    if (archive->local_name != NULL)
    {
        struct stat my;
        vfs_path_t *local_name_vpath, *name_vpath;

        local_name_vpath = vfs_path_from_str (archive->local_name);
        name_vpath = vfs_path_from_str (archive->local_name);
        mc_stat (local_name_vpath, &my);
        mc_ungetlocalcopy (name_vpath, local_name_vpath,
                           archive->local_stat.st_mtime != my.st_mtime);
        vfs_path_free (local_name_vpath);
        vfs_path_free (name_vpath);
        g_free (archive->local_name);
    }
    g_free (archive->name);
    g_free (archive);
}

/* --------------------------------------------------------------------------------------------- */

static FILE *
extfs_open_archive (int fstype, const char *name, struct archive **pparc)
{
    const extfs_plugin_info_t *info;
    static dev_t archive_counter = 0;
    FILE *result = NULL;
    mode_t mode;
    char *cmd;
    struct stat mystat;
    struct archive *current_archive;
    struct entry *root_entry;
    char *tmp = NULL;
    vfs_path_t *local_name_vpath = NULL;
    vfs_path_t *name_vpath;

    name_vpath = vfs_path_from_str (name);
    info = &g_array_index (extfs_plugins, extfs_plugin_info_t, fstype);

    if (info->need_archive)
    {
        if (mc_stat (name_vpath, &mystat) == -1)
            goto ret;

        if (!vfs_file_is_local (name_vpath))
        {
            local_name_vpath = mc_getlocalcopy (name_vpath);
            if (local_name_vpath == NULL)
                goto ret;
        }

        tmp = name_quote (vfs_path_get_last_path_str (name_vpath), 0);
    }

    cmd = g_strconcat (info->path, info->prefix, " list ",
                       vfs_path_get_last_path_str (local_name_vpath) != NULL ?
                       vfs_path_get_last_path_str (local_name_vpath) : tmp, (char *) NULL);
    g_free (tmp);

    open_error_pipe ();
    result = popen (cmd, "r");
    g_free (cmd);
    if (result == NULL)
    {
        close_error_pipe (D_ERROR, NULL);
        if (local_name_vpath != NULL)
        {
            mc_ungetlocalcopy (name_vpath, local_name_vpath, FALSE);
            vfs_path_free (local_name_vpath);
        }
        goto ret;
    }

#ifdef ___QNXNTO__
    setvbuf (result, NULL, _IONBF, 0);
#endif

    current_archive = g_new (struct archive, 1);
    current_archive->fstype = fstype;
    current_archive->name = (name != NULL) ? g_strdup (name) : NULL;
    current_archive->local_name = g_strdup (vfs_path_get_last_path_str (local_name_vpath));

    if (local_name_vpath != NULL)
    {
        mc_stat (local_name_vpath, &current_archive->local_stat);
        vfs_path_free (local_name_vpath);
    }
    current_archive->inode_counter = 0;
    current_archive->fd_usage = 0;
    current_archive->rdev = archive_counter++;
    current_archive->next = first_archive;
    first_archive = current_archive;
    mode = mystat.st_mode & 07777;
    if (mode & 0400)
        mode |= 0100;
    if (mode & 0040)
        mode |= 0010;
    if (mode & 0004)
        mode |= 0001;
    mode |= S_IFDIR;
    root_entry = extfs_generate_entry (current_archive, PATH_SEP_STR, NULL, mode);
    root_entry->inode->uid = mystat.st_uid;
    root_entry->inode->gid = mystat.st_gid;
    root_entry->inode->atime = mystat.st_atime;
    root_entry->inode->ctime = mystat.st_ctime;
    root_entry->inode->mtime = mystat.st_mtime;
    current_archive->root_entry = root_entry;

    *pparc = current_archive;

  ret:
    vfs_path_free (name_vpath);
    return result;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Main loop for reading an archive.
 * Return 0 on success, -1 on error.
 */

static int
extfs_read_archive (int fstype, const char *name, struct archive **pparc)
{
    FILE *extfsd;
    const extfs_plugin_info_t *info;
    char *buffer;
    struct archive *current_archive;
    char *current_file_name, *current_link_name;

    info = &g_array_index (extfs_plugins, extfs_plugin_info_t, fstype);

    extfsd = extfs_open_archive (fstype, name, &current_archive);

    if (extfsd == NULL)
    {
        message (D_ERROR, MSG_ERROR, _("Cannot open %s archive\n%s"), info->prefix, name);
        return -1;
    }

    buffer = g_malloc (BUF_4K);
    while (fgets (buffer, BUF_4K, extfsd) != NULL)
    {
        struct stat hstat;

        current_link_name = NULL;
        if (vfs_parse_ls_lga (buffer, &hstat, &current_file_name, &current_link_name, NULL))
        {
            struct entry *entry, *pent;
            struct inode *inode;
            char *p, *q, *cfn = current_file_name;

            if (*cfn != '\0')
            {
                if (*cfn == PATH_SEP)
                    cfn++;
                p = strchr (cfn, '\0');
                if (p != cfn && *(p - 1) == PATH_SEP)
                    *(p - 1) = '\0';
                p = strrchr (cfn, PATH_SEP);
                if (p == NULL)
                {
                    p = cfn;
                    q = strchr (cfn, '\0');
                }
                else
                {
                    *(p++) = '\0';
                    q = cfn;
                }
                if (S_ISDIR (hstat.st_mode) && (strcmp (p, ".") == 0 || strcmp (p, "..") == 0))
                    goto read_extfs_continue;
                pent = extfs_find_entry (current_archive->root_entry, q, TRUE, FALSE);
                if (pent == NULL)
                {
                    /* FIXME: Should clean everything one day */
                    g_free (buffer);
                    pclose (extfsd);
                    close_error_pipe (D_ERROR, _("Inconsistent extfs archive"));
                    return -1;
                }
                entry = g_new (struct entry, 1);
                entry->name = g_strdup (p);
                entry->next_in_dir = NULL;
                entry->dir = pent;
                if (pent->inode->last_in_subdir)
                {
                    pent->inode->last_in_subdir->next_in_dir = entry;
                    pent->inode->last_in_subdir = entry;
                }
                if (!S_ISLNK (hstat.st_mode) && (current_link_name != NULL))
                {
                    pent = extfs_find_entry (current_archive->root_entry,
                                             current_link_name, FALSE, FALSE);
                    if (pent == NULL)
                    {
                        /* FIXME: Should clean everything one day */
                        g_free (buffer);
                        pclose (extfsd);
                        close_error_pipe (D_ERROR, _("Inconsistent extfs archive"));
                        return -1;
                    }

                    entry->inode = pent->inode;
                    pent->inode->nlink++;
                }
                else
                {
                    inode = g_new (struct inode, 1);
                    entry->inode = inode;
                    inode->local_filename = NULL;
                    inode->inode = (current_archive->inode_counter)++;
                    inode->nlink = 1;
                    inode->dev = current_archive->rdev;
                    inode->archive = current_archive;
                    inode->mode = hstat.st_mode;
#ifdef HAVE_STRUCT_STAT_ST_RDEV
                    inode->rdev = hstat.st_rdev;
#else
                    inode->rdev = 0;
#endif
                    inode->uid = hstat.st_uid;
                    inode->gid = hstat.st_gid;
                    inode->size = hstat.st_size;
                    inode->mtime = hstat.st_mtime;
                    inode->atime = hstat.st_atime;
                    inode->ctime = hstat.st_ctime;
                    inode->first_in_subdir = NULL;
                    inode->last_in_subdir = NULL;
                    if (current_link_name != NULL && S_ISLNK (hstat.st_mode))
                    {
                        inode->linkname = current_link_name;
                        current_link_name = NULL;
                    }
                    else
                    {
                        if (S_ISLNK (hstat.st_mode))
                            inode->mode &= ~S_IFLNK;    /* You *DON'T* want to do this always */
                        inode->linkname = NULL;
                    }
                    if (S_ISDIR (hstat.st_mode))
                        extfs_make_dots (entry);
                }
            }
          read_extfs_continue:
            g_free (current_file_name);
            g_free (current_link_name);
        }
    }
    g_free (buffer);

    /* Check if extfs 'list' returned 0 */
    if (pclose (extfsd) != 0)
    {
        extfs_free (current_archive);
        close_error_pipe (D_ERROR, _("Inconsistent extfs archive"));
        return -1;
    }

    close_error_pipe (D_ERROR, NULL);
    *pparc = current_archive;
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
extfs_which (struct vfs_class *me, const char *path)
{
    size_t path_len;
    size_t i;

    (void) me;

    path_len = strlen (path);

    for (i = 0; i < extfs_plugins->len; i++)
    {
        extfs_plugin_info_t *info;

        info = &g_array_index (extfs_plugins, extfs_plugin_info_t, i);

        if ((strncmp (path, info->prefix, path_len) == 0)
            && ((info->prefix[path_len] == '\0') || (info->prefix[path_len] == '+')))
            return i;
    }
    return -1;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Dissect the path and create corresponding superblock.
 */
static const char *
extfs_get_path_int (const vfs_path_t * vpath, struct archive **archive, gboolean do_not_open)
{
    char *archive_name;
    int result = -1;
    struct archive *parc;
    int fstype;
    const vfs_path_element_t *path_element;

    path_element = vfs_path_get_by_index (vpath, -1);

    fstype = extfs_which (path_element->class, path_element->vfs_prefix);
    if (fstype == -1)
        return NULL;

    archive_name = vfs_path_to_str_elements_count (vpath, -1);

    /*
     * All filesystems should have some local archive, at least
     * it can be PATH_SEP ('/').
     */
    for (parc = first_archive; parc != NULL; parc = parc->next)
        if (parc->name != NULL)
        {
            if (strcmp (parc->name, archive_name) == 0)
            {
                vfs_stamp (&vfs_extfs_ops, (vfsid) parc);
                goto return_success;
            }
        }

    result = do_not_open ? -1 : extfs_read_archive (fstype, archive_name, &parc);
    if (result == -1)
    {
        path_element->class->verrno = EIO;
        g_free (archive_name);
        return NULL;
    }

  return_success:
    *archive = parc;
    g_free (archive_name);
    return path_element->path;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Dissect the path and create corresponding superblock.
 * The result should be freed.
 */

static char *
extfs_get_path (const vfs_path_t * vpath, struct archive **archive, gboolean do_not_open)
{
    return g_strdup (extfs_get_path_int (vpath, archive, do_not_open));
}

/* --------------------------------------------------------------------------------------------- */
/* Return allocated path (without leading slash) inside the archive  */

static char *
extfs_get_path_from_entry (struct entry *entry)
{
    GString *localpath;

    localpath = g_string_new ("");

    while (entry->dir != NULL)
    {
        g_string_prepend (localpath, entry->name);
        if (entry->dir->dir != NULL)
            g_string_prepend_c (localpath, PATH_SEP);
        entry = entry->dir;
    }

    return g_string_free (localpath, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static struct entry *
extfs_resolve_symlinks_int (struct entry *entry, GSList * list)
{
    struct entry *pent = NULL;

    if (!S_ISLNK (entry->inode->mode))
        return entry;

    if (g_slist_find (list, entry) != NULL)
    {
        /* Here we protect us against symlink looping */
        errloop = TRUE;
    }
    else
    {
        GSList *looping;

        looping = g_slist_prepend (list, entry);
        pent = extfs_find_entry_int (entry->dir, entry->inode->linkname, looping, FALSE, FALSE);
        looping = g_slist_delete_link (looping, looping);

        if (pent == NULL)
            my_errno = ENOENT;
    }

    return pent;
}

/* --------------------------------------------------------------------------------------------- */

static struct entry *
extfs_resolve_symlinks (struct entry *entry)
{
    struct entry *res;

    errloop = FALSE;
    notadir = FALSE;
    res = extfs_resolve_symlinks_int (entry, NULL);
    if (res == NULL)
    {
        if (errloop)
            my_errno = ELOOP;
        else if (notadir)
            my_errno = ENOTDIR;
    }
    return res;
}

/* --------------------------------------------------------------------------------------------- */

static char *
extfs_get_archive_name (struct archive *archive)
{
    const char *archive_name;

    if (archive->local_name)
        archive_name = archive->local_name;
    else
        archive_name = archive->name;

    if (!archive_name || !*archive_name)
        return g_strdup ("no_archive_name");
    else
    {
        char *ret_str;
        vfs_path_t *vpath;
        const vfs_path_element_t *path_element;

        vpath = vfs_path_from_str (archive_name);
        path_element = vfs_path_get_by_index (vpath, -1);
        ret_str = g_strdup (path_element->path);
        vfs_path_free (vpath);
        return ret_str;
    }
}

/* --------------------------------------------------------------------------------------------- */
/** Don't pass localname as NULL */

static int
extfs_cmd (const char *str_extfs_cmd, struct archive *archive,
           struct entry *entry, const char *localname)
{
    char *file;
    char *quoted_file;
    char *quoted_localname;
    char *archive_name, *quoted_archive_name;
    const extfs_plugin_info_t *info;
    char *cmd;
    int retval;

    file = extfs_get_path_from_entry (entry);
    quoted_file = name_quote (file, 0);
    g_free (file);

    archive_name = extfs_get_archive_name (archive);
    quoted_archive_name = name_quote (archive_name, 0);
    g_free (archive_name);
    quoted_localname = name_quote (localname, 0);
    info = &g_array_index (extfs_plugins, extfs_plugin_info_t, archive->fstype);
    cmd = g_strconcat (info->path, info->prefix, str_extfs_cmd,
                       quoted_archive_name, " ", quoted_file, " ", quoted_localname, (char *) NULL);
    g_free (quoted_file);
    g_free (quoted_localname);
    g_free (quoted_archive_name);

    open_error_pipe ();
    retval = my_system (EXECUTE_AS_SHELL, mc_global.tty.shell, cmd);
    g_free (cmd);
    close_error_pipe (D_ERROR, NULL);
    return retval;
}

/* --------------------------------------------------------------------------------------------- */

static void
extfs_run (const vfs_path_t * vpath)
{
    struct archive *archive = NULL;
    char *p, *q, *archive_name, *quoted_archive_name;
    char *cmd;
    const extfs_plugin_info_t *info;

    p = extfs_get_path (vpath, &archive, FALSE);
    if (p == NULL)
        return;
    q = name_quote (p, 0);
    g_free (p);

    archive_name = extfs_get_archive_name (archive);
    quoted_archive_name = name_quote (archive_name, 0);
    g_free (archive_name);
    info = &g_array_index (extfs_plugins, extfs_plugin_info_t, archive->fstype);
    cmd =
        g_strconcat (info->path, info->prefix, " run ", quoted_archive_name, " ", q, (char *) NULL);
    g_free (quoted_archive_name);
    g_free (q);
    shell_execute (cmd, 0);
    g_free (cmd);
}

/* --------------------------------------------------------------------------------------------- */

static void *
extfs_open (const vfs_path_t * vpath, int flags, mode_t mode)
{
    struct pseudofile *extfs_info;
    struct archive *archive = NULL;
    char *q;
    struct entry *entry;
    int local_handle;
    gboolean created = FALSE;

    q = extfs_get_path (vpath, &archive, FALSE);
    if (q == NULL)
        return NULL;
    entry = extfs_find_entry (archive->root_entry, q, FALSE, FALSE);
    if ((entry == NULL) && ((flags & O_CREAT) != 0))
    {
        /* Create new entry */
        entry = extfs_find_entry (archive->root_entry, q, FALSE, TRUE);
        created = (entry != NULL);
    }

    g_free (q);
    if (entry == NULL)
        return NULL;
    entry = extfs_resolve_symlinks (entry);
    if (entry == NULL)
        return NULL;

    if (S_ISDIR (entry->inode->mode))
        ERRNOR (EISDIR, NULL);

    if (entry->inode->local_filename == NULL)
    {
        vfs_path_t *local_filename_vpath;
        const char *local_filename;

        local_handle = vfs_mkstemps (&local_filename_vpath, "extfs", entry->name);

        if (local_handle == -1)
            return NULL;
        close (local_handle);
        local_filename = vfs_path_get_by_index (local_filename_vpath, -1)->path;

        if (!created && ((flags & O_TRUNC) == 0)
            && extfs_cmd (" copyout ", archive, entry, local_filename))
        {
            unlink (local_filename);
            vfs_path_free (local_filename_vpath);
            my_errno = EIO;
            return NULL;
        }
        entry->inode->local_filename = g_strdup (local_filename);
        vfs_path_free (local_filename_vpath);
    }

    local_handle = open (entry->inode->local_filename, NO_LINEAR (flags), mode);

    if (local_handle == -1)
    {
        /* file exists(may be). Need to drop O_CREAT flag and truncate file content */
        flags = ~O_CREAT & (NO_LINEAR (flags) | O_TRUNC);
        local_handle = open (entry->inode->local_filename, flags, mode);
    }

    if (local_handle == -1)
        ERRNOR (EIO, NULL);

    extfs_info = g_new (struct pseudofile, 1);
    extfs_info->archive = archive;
    extfs_info->entry = entry;
    extfs_info->has_changed = created;
    extfs_info->local_handle = local_handle;

    /* i.e. we had no open files and now we have one */
    vfs_rmstamp (&vfs_extfs_ops, (vfsid) archive);
    archive->fd_usage++;
    return extfs_info;
}

/* --------------------------------------------------------------------------------------------- */

static ssize_t
extfs_read (void *data, char *buffer, size_t count)
{
    struct pseudofile *file = (struct pseudofile *) data;

    return read (file->local_handle, buffer, count);
}

/* --------------------------------------------------------------------------------------------- */

static int
extfs_close (void *data)
{
    struct pseudofile *file;
    int errno_code = 0;
    file = (struct pseudofile *) data;

    close (file->local_handle);

    /* Commit the file if it has changed */
    if (file->has_changed)
    {
        struct stat file_status;

        if (extfs_cmd (" copyin ", file->archive, file->entry, file->entry->inode->local_filename))
            errno_code = EIO;

        if (stat (file->entry->inode->local_filename, &file_status) != 0)
            errno_code = EIO;
        else
            file->entry->inode->size = file_status.st_size;

        file->entry->inode->mtime = time (NULL);
    }

    if (--file->archive->fd_usage == 0)
        vfs_stamp_create (&vfs_extfs_ops, file->archive);

    g_free (data);
    if (errno_code != 0)
        ERRNOR (EIO, -1);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
extfs_errno (struct vfs_class *me)
{
    (void) me;
    return my_errno;
}

/* --------------------------------------------------------------------------------------------- */

static void *
extfs_opendir (const vfs_path_t * vpath)
{
    struct archive *archive = NULL;
    char *q;
    struct entry *entry;
    struct entry **info;

    q = extfs_get_path (vpath, &archive, FALSE);
    if (q == NULL)
        return NULL;
    entry = extfs_find_entry (archive->root_entry, q, FALSE, FALSE);
    g_free (q);
    if (entry == NULL)
        return NULL;
    entry = extfs_resolve_symlinks (entry);
    if (entry == NULL)
        return NULL;
    if (!S_ISDIR (entry->inode->mode))
        ERRNOR (ENOTDIR, NULL);

    info = g_new (struct entry *, 2);
    info[0] = entry->inode->first_in_subdir;
    info[1] = entry->inode->first_in_subdir;

    return info;
}

/* --------------------------------------------------------------------------------------------- */

static void *
extfs_readdir (void *data)
{
    static union vfs_dirent dir;
    struct entry **info = (struct entry **) data;

    if (*info == NULL)
        return NULL;

    g_strlcpy (dir.dent.d_name, (*info)->name, MC_MAXPATHLEN);

    compute_namelen (&dir.dent);
    *info = (*info)->next_in_dir;

    return (void *) &dir;
}

/* --------------------------------------------------------------------------------------------- */

static int
extfs_closedir (void *data)
{
    g_free (data);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
extfs_stat_move (struct stat *buf, const struct inode *inode)
{
    buf->st_dev = inode->dev;
    buf->st_ino = inode->inode;
    buf->st_mode = inode->mode;
    buf->st_nlink = inode->nlink;
    buf->st_uid = inode->uid;
    buf->st_gid = inode->gid;
#ifdef HAVE_STRUCT_STAT_ST_RDEV
    buf->st_rdev = inode->rdev;
#endif
    buf->st_size = inode->size;
#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
    buf->st_blksize = RECORDSIZE;
#endif
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
    buf->st_blocks = (inode->size + RECORDSIZE - 1) / RECORDSIZE;
#endif
    buf->st_atime = inode->atime;
    buf->st_mtime = inode->mtime;
    buf->st_ctime = inode->ctime;
}

/* --------------------------------------------------------------------------------------------- */

static int
extfs_internal_stat (const vfs_path_t * vpath, struct stat *buf, gboolean resolve)
{
    struct archive *archive;
    const char *q;
    struct entry *entry;
    int result = -1;

    q = extfs_get_path_int (vpath, &archive, FALSE);
    if (q == NULL)
        goto cleanup;
    entry = extfs_find_entry (archive->root_entry, q, FALSE, FALSE);
    if (entry == NULL)
        goto cleanup;
    if (resolve)
    {
        entry = extfs_resolve_symlinks (entry);
        if (entry == NULL)
            goto cleanup;
    }
    extfs_stat_move (buf, entry->inode);
    result = 0;
  cleanup:
    return result;
}

/* --------------------------------------------------------------------------------------------- */

static int
extfs_stat (const vfs_path_t * vpath, struct stat *buf)
{
    return extfs_internal_stat (vpath, buf, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static int
extfs_lstat (const vfs_path_t * vpath, struct stat *buf)
{
    return extfs_internal_stat (vpath, buf, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static int
extfs_fstat (void *data, struct stat *buf)
{
    struct pseudofile *file = (struct pseudofile *) data;

    extfs_stat_move (buf, file->entry->inode);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
extfs_readlink (const vfs_path_t * vpath, char *buf, size_t size)
{
    struct archive *archive;
    const char *q;
    size_t len;
    struct entry *entry;
    int result = -1;

    q = extfs_get_path_int (vpath, &archive, FALSE);
    if (q == NULL)
        goto cleanup;
    entry = extfs_find_entry (archive->root_entry, q, FALSE, FALSE);
    if (entry == NULL)
        goto cleanup;
    if (!S_ISLNK (entry->inode->mode))
    {
        const vfs_path_element_t *path_element;

        path_element = vfs_path_get_by_index (vpath, -1);
        path_element->class->verrno = EINVAL;
        goto cleanup;
    }
    len = strlen (entry->inode->linkname);
    if (size < len)
        len = size;
    /* readlink() does not append a NUL character to buf */
    result = len;
    memcpy (buf, entry->inode->linkname, result);
  cleanup:
    return result;
}

/* --------------------------------------------------------------------------------------------- */

static int
extfs_chown (const vfs_path_t * vpath, uid_t owner, gid_t group)
{
    (void) vpath;
    (void) owner;
    (void) group;
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
extfs_chmod (const vfs_path_t * vpath, mode_t mode)
{
    (void) vpath;
    (void) mode;
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static ssize_t
extfs_write (void *data, const char *buf, size_t nbyte)
{
    struct pseudofile *file = (struct pseudofile *) data;

    file->has_changed = TRUE;
    return write (file->local_handle, buf, nbyte);
}

/* --------------------------------------------------------------------------------------------- */

static int
extfs_unlink (const vfs_path_t * vpath)
{
    struct archive *archive;
    const char *q;
    struct entry *entry;
    int result = -1;

    q = extfs_get_path_int (vpath, &archive, FALSE);
    if (q == NULL)
        goto cleanup;
    entry = extfs_find_entry (archive->root_entry, q, FALSE, FALSE);
    if (entry == NULL)
        goto cleanup;
    entry = extfs_resolve_symlinks (entry);
    if (entry == NULL)
        goto cleanup;
    if (S_ISDIR (entry->inode->mode))
    {
        const vfs_path_element_t *path_element;

        path_element = vfs_path_get_by_index (vpath, -1);
        path_element->class->verrno = EISDIR;
        goto cleanup;
    }
    if (extfs_cmd (" rm ", archive, entry, ""))
    {
        my_errno = EIO;
        goto cleanup;
    }
    extfs_remove_entry (entry);
    result = 0;
  cleanup:
    return result;
}

/* --------------------------------------------------------------------------------------------- */

static int
extfs_mkdir (const vfs_path_t * vpath, mode_t mode)
{
    struct archive *archive;
    const char *q;
    struct entry *entry;
    int result = -1;
    const vfs_path_element_t *path_element;

    (void) mode;

    path_element = vfs_path_get_by_index (vpath, -1);
    q = extfs_get_path_int (vpath, &archive, FALSE);
    if (q == NULL)
        goto cleanup;
    entry = extfs_find_entry (archive->root_entry, q, FALSE, FALSE);
    if (entry != NULL)
    {
        path_element->class->verrno = EEXIST;
        goto cleanup;
    }
    entry = extfs_find_entry (archive->root_entry, q, TRUE, FALSE);
    if (entry == NULL)
        goto cleanup;
    entry = extfs_resolve_symlinks (entry);
    if (entry == NULL)
        goto cleanup;
    if (!S_ISDIR (entry->inode->mode))
    {
        path_element->class->verrno = ENOTDIR;
        goto cleanup;
    }

    if (extfs_cmd (" mkdir ", archive, entry, ""))
    {
        my_errno = EIO;
        extfs_remove_entry (entry);
        goto cleanup;
    }
    result = 0;
  cleanup:
    return result;
}

/* --------------------------------------------------------------------------------------------- */

static int
extfs_rmdir (const vfs_path_t * vpath)
{
    struct archive *archive;
    const char *q;
    struct entry *entry;
    int result = -1;

    q = extfs_get_path_int (vpath, &archive, FALSE);
    if (q == NULL)
        goto cleanup;
    entry = extfs_find_entry (archive->root_entry, q, FALSE, FALSE);
    if (entry == NULL)
        goto cleanup;
    entry = extfs_resolve_symlinks (entry);
    if (entry == NULL)
        goto cleanup;
    if (!S_ISDIR (entry->inode->mode))
    {
        const vfs_path_element_t *path_element;

        path_element = vfs_path_get_by_index (vpath, -1);
        path_element->class->verrno = ENOTDIR;
        goto cleanup;
    }

    if (extfs_cmd (" rmdir ", archive, entry, ""))
    {
        my_errno = EIO;
        goto cleanup;
    }
    extfs_remove_entry (entry);
    result = 0;
  cleanup:
    return result;
}

/* --------------------------------------------------------------------------------------------- */

static int
extfs_chdir (const vfs_path_t * vpath)
{
    struct archive *archive = NULL;
    char *q;
    struct entry *entry;

    my_errno = ENOTDIR;
    q = extfs_get_path (vpath, &archive, FALSE);
    if (q == NULL)
        return -1;
    entry = extfs_find_entry (archive->root_entry, q, FALSE, FALSE);
    g_free (q);
    if (entry == NULL)
        return -1;
    entry = extfs_resolve_symlinks (entry);
    if ((entry == NULL) || (!S_ISDIR (entry->inode->mode)))
        return -1;
    my_errno = 0;
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static off_t
extfs_lseek (void *data, off_t offset, int whence)
{
    struct pseudofile *file = (struct pseudofile *) data;

    return lseek (file->local_handle, offset, whence);
}

/* --------------------------------------------------------------------------------------------- */

static vfsid
extfs_getid (const vfs_path_t * vpath)
{
    struct archive *archive = NULL;
    char *p;

    p = extfs_get_path (vpath, &archive, TRUE);
    if (p == NULL)
        return NULL;
    g_free (p);
    return (vfsid) archive;
}

/* --------------------------------------------------------------------------------------------- */

static int
extfs_nothingisopen (vfsid id)
{
    return (((struct archive *) id)->fd_usage <= 0);
}

/* --------------------------------------------------------------------------------------------- */

static void
extfs_remove_entry (struct entry *e)
{
    int i = --e->inode->nlink;
    struct entry *pe, *ent, *prev;

    if (S_ISDIR (e->inode->mode) && e->inode->first_in_subdir != NULL)
    {
        struct entry *f = e->inode->first_in_subdir;
        e->inode->first_in_subdir = NULL;
        extfs_remove_entry (f);
    }
    pe = e->dir;
    if (e == pe->inode->first_in_subdir)
        pe->inode->first_in_subdir = e->next_in_dir;

    prev = NULL;
    for (ent = pe->inode->first_in_subdir; ent && ent->next_in_dir; ent = ent->next_in_dir)
        if (e == ent->next_in_dir)
        {
            prev = ent;
            break;
        }
    if (prev)
        prev->next_in_dir = e->next_in_dir;
    if (e == pe->inode->last_in_subdir)
        pe->inode->last_in_subdir = prev;

    if (i <= 0)
    {
        if (e->inode->local_filename != NULL)
        {
            unlink (e->inode->local_filename);
            g_free (e->inode->local_filename);
        }
        g_free (e->inode->linkname);
        g_free (e->inode);
    }

    g_free (e->name);
    g_free (e);
}

/* --------------------------------------------------------------------------------------------- */

static void
extfs_free_entry (struct entry *e)
{
    int i = --e->inode->nlink;

    if (S_ISDIR (e->inode->mode) && e->inode->first_in_subdir != NULL)
    {
        struct entry *f = e->inode->first_in_subdir;

        e->inode->first_in_subdir = NULL;
        extfs_free_entry (f);
    }
    if (i <= 0)
    {
        if (e->inode->local_filename != NULL)
        {
            unlink (e->inode->local_filename);
            g_free (e->inode->local_filename);
        }
        g_free (e->inode->linkname);
        g_free (e->inode);
    }
    if (e->next_in_dir != NULL)
        extfs_free_entry (e->next_in_dir);
    g_free (e->name);
    g_free (e);
}

/* --------------------------------------------------------------------------------------------- */

static void
extfs_free (vfsid id)
{
    struct archive *archive = (struct archive *) id;

    if (archive == first_archive)
    {
        first_archive = archive->next;
    }
    else
    {
        struct archive *parc;
        for (parc = first_archive; parc != NULL; parc = parc->next)
            if (parc->next == archive)
            {
                parc->next = archive->next;
                break;
            }
    }
    extfs_free_archive (archive);
}

/* --------------------------------------------------------------------------------------------- */

static vfs_path_t *
extfs_getlocalcopy (const vfs_path_t * vpath)
{
    struct pseudofile *fp;
    vfs_path_t *p;

    fp = (struct pseudofile *) extfs_open (vpath, O_RDONLY, 0);
    if (fp == NULL)
        return NULL;
    if (fp->entry->inode->local_filename == NULL)
    {
        extfs_close ((void *) fp);
        return NULL;
    }
    p = vfs_path_from_str (fp->entry->inode->local_filename);
    fp->archive->fd_usage++;
    extfs_close ((void *) fp);
    return p;
}

/* --------------------------------------------------------------------------------------------- */

static int
extfs_ungetlocalcopy (const vfs_path_t * vpath, const vfs_path_t * local, gboolean has_changed)
{
    struct pseudofile *fp;

    fp = (struct pseudofile *) extfs_open (vpath, O_RDONLY, 0);
    if (fp == NULL)
        return 0;

    if (strcmp (fp->entry->inode->local_filename, vfs_path_get_last_path_str (local)) == 0)
    {
        fp->archive->fd_usage--;
        if (has_changed)
            fp->has_changed = TRUE;
        extfs_close ((void *) fp);
        return 0;
    }
    else
    {
        /* Should not happen */
        extfs_close ((void *) fp);
        return 0;
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
extfs_get_plugins (const char *where, gboolean silent)
{
    char *dirname;
    GDir *dir;
    const char *filename;

    dirname = g_build_path (PATH_SEP_STR, where, MC_EXTFS_DIR, (char *) NULL);
    dir = g_dir_open (dirname, 0, NULL);

    /* We may not use vfs_die() message or message or similar,
     * UI is not initialized at this time and message would not
     * appear on screen. */
    if (dir == NULL)
    {
        if (!silent)
            fprintf (stderr, _("Warning: cannot open %s directory\n"), dirname);
        g_free (dirname);
        return FALSE;
    }

    if (extfs_plugins == NULL)
        extfs_plugins = g_array_sized_new (FALSE, TRUE, sizeof (extfs_plugin_info_t), 32);

    while ((filename = g_dir_read_name (dir)) != NULL)
    {
        char fullname[MC_MAXPATHLEN];
        struct stat s;

        g_snprintf (fullname, sizeof (fullname), "%s" PATH_SEP_STR "%s", dirname, filename);

        if ((stat (fullname, &s) == 0)
            && S_ISREG (s.st_mode) && !S_ISDIR (s.st_mode)
            && (((s.st_mode & S_IXOTH) != 0) ||
                ((s.st_mode & S_IXUSR) != 0) || ((s.st_mode & S_IXGRP) != 0)))
        {
            int f;

            f = open (fullname, O_RDONLY);

            if (f > 0)
            {
                size_t len, i;
                extfs_plugin_info_t info;
                gboolean found = FALSE;

                close (f);

                /* Handle those with a trailing '+', those flag that the
                 * file system does not require an archive to work
                 */
                len = strlen (filename);
                info.need_archive = (filename[len - 1] != '+');
                info.path = g_strconcat (dirname, PATH_SEP_STR, (char *) NULL);
                info.prefix = g_strdup (filename);

                /* prepare to compare file names without trailing '+' */
                if (!info.need_archive)
                    info.prefix[len - 1] = '\0';

                /* don't overload already found plugin */
                for (i = 0; i < extfs_plugins->len; i++)
                {
                    extfs_plugin_info_t *p;

                    p = &g_array_index (extfs_plugins, extfs_plugin_info_t, i);

                    /* 2 files with same names cannot be in a directory */
                    if ((strcmp (info.path, p->path) != 0)
                        && (strcmp (info.prefix, p->prefix) == 0))
                    {
                        found = TRUE;
                        break;
                    }
                }

                if (found)
                {
                    g_free (info.path);
                    g_free (info.prefix);
                }
                else
                {
                    /* restore file name */
                    if (!info.need_archive)
                        info.prefix[len - 1] = '+';
                    g_array_append_val (extfs_plugins, info);
                }
            }
        }
    }

    g_dir_close (dir);
    g_free (dirname);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static int
extfs_init (struct vfs_class *me)
{
    gboolean d1, d2;

    (void) me;

    /* 1st: scan user directory */
    d1 = extfs_get_plugins (mc_config_get_data_path (), TRUE);  /* silent about user dir */
    /* 2nd: scan system dir */
    d2 = extfs_get_plugins (LIBEXECDIR, d1);

    return (d1 || d2 ? 1 : 0);
}

/* --------------------------------------------------------------------------------------------- */

static void
extfs_done (struct vfs_class *me)
{
    size_t i;
    struct archive *ar;

    (void) me;

    for (ar = first_archive; ar != NULL;)
    {
        extfs_free ((vfsid) ar);
        ar = first_archive;
    }

    for (i = 0; i < extfs_plugins->len; i++)
    {
        extfs_plugin_info_t *info;

        info = &g_array_index (extfs_plugins, extfs_plugin_info_t, i);
        g_free (info->path);
        g_free (info->prefix);
    }

    if (extfs_plugins != NULL)
        g_array_free (extfs_plugins, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static int
extfs_setctl (const vfs_path_t * vpath, int ctlop, void *arg)
{
    (void) arg;

    if (ctlop == VFS_SETCTL_RUN)
    {
        extfs_run (vpath);
        return 1;
    }
    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
init_extfs (void)
{
    vfs_extfs_ops.name = "extfs";
    vfs_extfs_ops.init = extfs_init;
    vfs_extfs_ops.done = extfs_done;
    vfs_extfs_ops.fill_names = extfs_fill_names;
    vfs_extfs_ops.which = extfs_which;
    vfs_extfs_ops.open = extfs_open;
    vfs_extfs_ops.close = extfs_close;
    vfs_extfs_ops.read = extfs_read;
    vfs_extfs_ops.write = extfs_write;
    vfs_extfs_ops.opendir = extfs_opendir;
    vfs_extfs_ops.readdir = extfs_readdir;
    vfs_extfs_ops.closedir = extfs_closedir;
    vfs_extfs_ops.stat = extfs_stat;
    vfs_extfs_ops.lstat = extfs_lstat;
    vfs_extfs_ops.fstat = extfs_fstat;
    vfs_extfs_ops.chmod = extfs_chmod;
    vfs_extfs_ops.chown = extfs_chown;
    vfs_extfs_ops.readlink = extfs_readlink;
    vfs_extfs_ops.unlink = extfs_unlink;
    vfs_extfs_ops.chdir = extfs_chdir;
    vfs_extfs_ops.ferrno = extfs_errno;
    vfs_extfs_ops.lseek = extfs_lseek;
    vfs_extfs_ops.getid = extfs_getid;
    vfs_extfs_ops.nothingisopen = extfs_nothingisopen;
    vfs_extfs_ops.free = extfs_free;
    vfs_extfs_ops.getlocalcopy = extfs_getlocalcopy;
    vfs_extfs_ops.ungetlocalcopy = extfs_ungetlocalcopy;
    vfs_extfs_ops.mkdir = extfs_mkdir;
    vfs_extfs_ops.rmdir = extfs_rmdir;
    vfs_extfs_ops.setctl = extfs_setctl;
    vfs_register_class (&vfs_extfs_ops);
}

/* --------------------------------------------------------------------------------------------- */
