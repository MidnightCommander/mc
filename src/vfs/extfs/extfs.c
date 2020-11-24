/*
   Virtual File System: External file system.

   Copyright (C) 1995-2020
   Free Software Foundation, Inc.

   Written by:
   Jakub Jelinek, 1995
   Pavel Machek, 1998
   Andrew T. Veliath, 1999
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
#include <errno.h>
#include <sys/wait.h>

#include "lib/global.h"
#include "lib/fileloc.h"
#include "lib/mcconfig.h"
#include "lib/util.h"
#include "lib/widget.h"         /* message() */

#include "src/execute.h"        /* For shell_execute */

#include "lib/vfs/vfs.h"
#include "lib/vfs/utilvfs.h"
#include "lib/vfs/xdirentry.h"
#include "lib/vfs/gc.h"         /* vfs_rmstamp */

#include "extfs.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#undef ERRNOR
#define ERRNOR(x,y) do { my_errno = x; return y; } while(0)

#define RECORDSIZE 512

#define EXTFS_SUPER(a) ((struct extfs_super_t *) (a))

/*** file scope type declarations ****************************************************************/

struct extfs_super_t
{
    struct vfs_s_super base;    /* base class */

    int fstype;
    char *local_name;
    struct stat local_stat;
    dev_t rdev;
};

typedef struct
{
    char *path;
    char *prefix;
    gboolean need_archive;
} extfs_plugin_info_t;

/*** file scope variables ************************************************************************/

static GArray *extfs_plugins = NULL;

static gboolean errloop;
static gboolean notadir;

static struct vfs_s_subclass extfs_subclass;
static struct vfs_class *vfs_extfs_ops = VFS_CLASS (&extfs_subclass);

static int my_errno = 0;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static struct vfs_s_entry *extfs_resolve_symlinks_int (struct vfs_s_entry *entry, GSList * list);

/* --------------------------------------------------------------------------------------------- */

static struct extfs_super_t *
extfs_super_new (struct vfs_class *me, const char *name, const vfs_path_t * local_name_vpath,
                 int fstype)
{
    struct extfs_super_t *super;
    struct vfs_s_super *vsuper;

    super = g_new0 (struct extfs_super_t, 1);
    vsuper = VFS_SUPER (super);

    vsuper->me = me;
    vsuper->name = g_strdup (name);

    super->fstype = fstype;

    if (local_name_vpath != NULL)
    {
        super->local_name = g_strdup (vfs_path_get_last_path_str (local_name_vpath));
        mc_stat (local_name_vpath, &super->local_stat);
    }

    VFS_SUBCLASS (me)->supers = g_list_prepend (VFS_SUBCLASS (me)->supers, super);

    return super;
}

/* --------------------------------------------------------------------------------------------- */

/* unlike vfs_s_new_entry(), inode->ent is kept */
static struct vfs_s_entry *
extfs_entry_new (struct vfs_class *me, const char *name, struct vfs_s_inode *inode)
{
    struct vfs_s_entry *entry;

    (void) me;

    entry = g_new0 (struct vfs_s_entry, 1);

    entry->name = g_strdup (name);
    entry->ino = inode;

    return entry;
}

/* --------------------------------------------------------------------------------------------- */

static void
extfs_fill_name (void *data, void *user_data)
{
    struct vfs_s_super *a = VFS_SUPER (data);
    fill_names_f func = (fill_names_f) user_data;
    extfs_plugin_info_t *info;
    char *name;

    info = &g_array_index (extfs_plugins, extfs_plugin_info_t, EXTFS_SUPER (a)->fstype);
    name =
        g_strconcat (a->name != NULL ? a->name : "", PATH_SEP_STR, info->prefix,
                     VFS_PATH_URL_DELIMITER, (char *) NULL);
    func (name);
    g_free (name);
}

/* --------------------------------------------------------------------------------------------- */

static gint
extfs_cmp_archive (const void *a, const void *b)
{
    const struct vfs_s_super *ar = (const struct vfs_s_super *) a;
    const char *archive_name = (const char *) b;

    return (ar->name != NULL && strcmp (ar->name, archive_name) == 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */

static struct vfs_s_entry *
extfs_generate_entry (struct extfs_super_t *archive, const char *name, struct vfs_s_inode *parent,
                      mode_t mode)
{
    struct vfs_class *me = VFS_SUPER (archive)->me;
    struct stat st;
    mode_t myumask;
    struct vfs_s_inode *inode;
    struct vfs_s_entry *entry;

    st.st_ino = VFS_SUPER (archive)->ino_usage++;
    st.st_dev = archive->rdev;
    myumask = umask (022);
    umask (myumask);
    st.st_mode = mode & ~myumask;
    st.st_rdev = 0;
    st.st_uid = getuid ();
    st.st_gid = getgid ();
    st.st_size = 0;
    st.st_mtime = time (NULL);
    st.st_atime = st.st_mtime;
    st.st_ctime = st.st_mtime;
    st.st_nlink = 1;

    inode = vfs_s_new_inode (me, VFS_SUPER (archive), &st);
    entry = vfs_s_new_entry (me, name, inode);
    if (parent != NULL)
        vfs_s_insert_entry (me, parent, entry);

    return entry;
}

/* --------------------------------------------------------------------------------------------- */

static struct vfs_s_entry *
extfs_find_entry_int (struct vfs_s_inode *dir, const char *name, GSList * list, int flags)
{
    struct vfs_s_entry *pent, *pdir;
    const char *p, *name_end;
    char *q;
    char c = PATH_SEP;
    struct extfs_super_t *super;

    if (g_path_is_absolute (name))
    {
        /* Handle absolute paths */
        name = g_path_skip_root (name);
        dir = dir->super->root;
    }

    super = EXTFS_SUPER (dir->super);
    pent = dir->ent;
    p = name;
    name_end = name + strlen (name);

    while ((pent != NULL) && (c != '\0') && (*p != '\0'))
    {
        q = strchr (p, PATH_SEP);
        if (q == NULL)
            q = (char *) name_end;

        c = *q;
        *q = '\0';

        if (DIR_IS_DOTDOT (p))
            pent = pent->dir->ent;
        else
        {
            GList *pl;

            pent = extfs_resolve_symlinks_int (pent, list);
            if (pent == NULL)
            {
                *q = c;
                return NULL;
            }

            if (!S_ISDIR (pent->ino->st.st_mode))
            {
                *q = c;
                notadir = TRUE;
                return NULL;
            }

            pdir = pent;
            pl = g_queue_find_custom (pent->ino->subdir, p, vfs_s_entry_compare);
            pent = pl != NULL ? VFS_ENTRY (pl->data) : NULL;
            if (pent != NULL && q + 1 > name_end)
            {
                /* Hack: I keep the original semanthic unless q+1 would break in the strchr */
                *q = c;
                notadir = !S_ISDIR (pent->ino->st.st_mode);
                return pent;
            }

            /* When we load archive, we create automagically non-existent directories */
            if (pent == NULL && (flags & FL_MKDIR) != 0)
                pent = extfs_generate_entry (super, p, pdir->ino, S_IFDIR | 0777);
            if (pent == NULL && (flags & FL_MKFILE) != 0)
                pent = extfs_generate_entry (super, p, pdir->ino, S_IFREG | 0666);
        }

        /* Next iteration */
        *q = c;
        if (c != '\0')
            p = q + 1;
    }
    if (pent == NULL)
        my_errno = ENOENT;
    return pent;
}

/* --------------------------------------------------------------------------------------------- */

static struct vfs_s_entry *
extfs_find_entry (struct vfs_s_inode *dir, const char *name, int flags)
{
    struct vfs_s_entry *res;

    errloop = FALSE;
    notadir = FALSE;

    res = extfs_find_entry_int (dir, name, NULL, flags);
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
    g_list_foreach (VFS_SUBCLASS (me)->supers, extfs_fill_name, func);
}

/* --------------------------------------------------------------------------------------------- */

/* Create this function because VFSF_USETMP flag is not used in extfs */
static void
extfs_free_inode (struct vfs_class *me, struct vfs_s_inode *ino)
{
    (void) me;

    if (ino->localname != NULL)
    {
        unlink (ino->localname);
        MC_PTR_FREE (ino->localname);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
extfs_free_archive (struct vfs_class *me, struct vfs_s_super *psup)
{
    struct extfs_super_t *archive = EXTFS_SUPER (psup);

    (void) me;

    if (archive->local_name != NULL)
    {
        struct stat my;
        vfs_path_t *local_name_vpath, *name_vpath;

        local_name_vpath = vfs_path_from_str (archive->local_name);
        name_vpath = vfs_path_from_str (psup->name);
        mc_stat (local_name_vpath, &my);
        mc_ungetlocalcopy (name_vpath, local_name_vpath,
                           archive->local_stat.st_mtime != my.st_mtime);
        vfs_path_free (local_name_vpath);
        vfs_path_free (name_vpath);
        g_free (archive->local_name);
    }
}

/* --------------------------------------------------------------------------------------------- */

static inline char *
extfs_skip_leading_dotslash (char *s)
{
    /* Skip leading "./" (if present).
     * Some programs don't understand it:
     *
     * $ zip file.zip ./-file2.txt file1.txt
     *   adding: -file2.txt (stored 0%)
     *   adding: file1.txt (stored 0%)
     * $ /usr/lib/mc/extfs.d/uzip copyout file.zip ./-file2.txt ./tmp-file2.txt
     * caution: filename not matched:  ./-file2.txt
     */
    if (s[0] == '.' && s[1] == PATH_SEP)
        s += 2;

    return s;
}

/* --------------------------------------------------------------------------------------------- */

static FILE *
extfs_open_archive (int fstype, const char *name, struct extfs_super_t **pparc)
{
    const extfs_plugin_info_t *info;
    static dev_t archive_counter = 0;
    FILE *result = NULL;
    mode_t mode;
    char *cmd;
    struct stat mystat;
    struct extfs_super_t *current_archive;
    struct vfs_s_entry *root_entry;
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

        tmp = name_quote (vfs_path_get_last_path_str (name_vpath), FALSE);
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

    current_archive = extfs_super_new (vfs_extfs_ops, name, local_name_vpath, fstype);
    current_archive->rdev = archive_counter++;
    vfs_path_free (local_name_vpath);

    mode = mystat.st_mode & 07777;
    if (mode & 0400)
        mode |= 0100;
    if (mode & 0040)
        mode |= 0010;
    if (mode & 0004)
        mode |= 0001;
    mode |= S_IFDIR;

    root_entry = extfs_generate_entry (current_archive, PATH_SEP_STR, NULL, mode);
    root_entry->ino->st.st_uid = mystat.st_uid;
    root_entry->ino->st.st_gid = mystat.st_gid;
    root_entry->ino->st.st_atime = mystat.st_atime;
    root_entry->ino->st.st_ctime = mystat.st_ctime;
    root_entry->ino->st.st_mtime = mystat.st_mtime;
    root_entry->ino->ent = root_entry;
    VFS_SUPER (current_archive)->root = root_entry->ino;

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
extfs_read_archive (FILE * extfsd, struct extfs_super_t *current_archive)
{
    int ret = 0;
    char *buffer;
    struct vfs_s_super *super = VFS_SUPER (current_archive);

    buffer = g_malloc (BUF_4K);

    while (fgets (buffer, BUF_4K, extfsd) != NULL)
    {
        struct stat hstat;
        char *current_file_name = NULL, *current_link_name = NULL;

        if (vfs_parse_ls_lga (buffer, &hstat, &current_file_name, &current_link_name, NULL))
        {
            struct vfs_s_entry *entry, *pent = NULL;
            struct vfs_s_inode *inode;
            char *p, *q, *cfn = current_file_name;

            if (*cfn != '\0')
            {
                cfn = extfs_skip_leading_dotslash (cfn);
                if (IS_PATH_SEP (*cfn))
                    cfn++;
                p = strchr (cfn, '\0');
                if (p != cfn && IS_PATH_SEP (p[-1]))
                    p[-1] = '\0';
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

                if (*q != '\0')
                {
                    pent = extfs_find_entry (super->root, q, FL_MKDIR);
                    if (pent == NULL)
                    {
                        ret = -1;
                        break;
                    }
                }

                if (pent != NULL)
                {
                    entry = extfs_entry_new (super->me, p, pent->ino);
                    entry->dir = pent->ino;
                    g_queue_push_tail (pent->ino->subdir, entry);
                }
                else
                {
                    entry = extfs_entry_new (super->me, p, super->root);
                    entry->dir = super->root;
                    g_queue_push_tail (super->root->subdir, entry);
                }

                if (!S_ISLNK (hstat.st_mode) && (current_link_name != NULL))
                {
                    pent = extfs_find_entry (super->root, current_link_name, FL_NONE);
                    if (pent == NULL)
                    {
                        ret = -1;
                        break;
                    }

                    pent->ino->st.st_nlink++;
                    entry->ino = pent->ino;
                }
                else
                {
                    struct stat st;

                    st.st_ino = super->ino_usage++;
                    st.st_nlink = 1;
                    st.st_dev = current_archive->rdev;
                    st.st_mode = hstat.st_mode;
#ifdef HAVE_STRUCT_STAT_ST_RDEV
                    st.st_rdev = hstat.st_rdev;
#else
                    st.st_rdev = 0;
#endif
                    st.st_uid = hstat.st_uid;
                    st.st_gid = hstat.st_gid;
                    st.st_size = hstat.st_size;
                    st.st_mtime = hstat.st_mtime;
                    st.st_atime = hstat.st_atime;
                    st.st_ctime = hstat.st_ctime;

                    if (current_link_name == NULL && S_ISLNK (hstat.st_mode))
                        st.st_mode &= ~S_IFLNK; /* You *DON'T* want to do this always */

                    inode = vfs_s_new_inode (super->me, super, &st);
                    inode->ent = entry;
                    entry->ino = inode;

                    if (current_link_name != NULL && S_ISLNK (hstat.st_mode))
                    {
                        inode->linkname = current_link_name;
                        current_link_name = NULL;
                    }
                }
            }

            g_free (current_file_name);
            g_free (current_link_name);
        }
    }

    g_free (buffer);

    return ret;
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

static int
extfs_open_and_read_archive (int fstype, const char *name, struct extfs_super_t **archive)
{
    int result = -1;
    FILE *extfsd;
    struct extfs_super_t *a;

    extfsd = extfs_open_archive (fstype, name, archive);
    a = *archive;

    if (extfsd == NULL)
    {
        const extfs_plugin_info_t *info;

        info = &g_array_index (extfs_plugins, extfs_plugin_info_t, fstype);
        message (D_ERROR, MSG_ERROR, _("Cannot open %s archive\n%s"), info->prefix, name);
    }
    else if (extfs_read_archive (extfsd, a) != 0)
    {
        pclose (extfsd);
        close_error_pipe (D_ERROR, _("Inconsistent extfs archive"));
    }
    else if (pclose (extfsd) != 0)
    {
        VFS_SUPER (a)->me->free (VFS_SUPER (a));
        close_error_pipe (D_ERROR, _("Inconsistent extfs archive"));
    }
    else
    {
        close_error_pipe (D_ERROR, NULL);
        result = 0;
    }

    return result;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Dissect the path and create corresponding superblock.
 */
static const char *
extfs_get_path (const vfs_path_t * vpath, struct extfs_super_t **archive, int flags)
{
    char *archive_name;
    int result = -1;
    GList *parc;
    int fstype;
    const vfs_path_element_t *path_element;
    struct extfs_super_t *a = NULL;

    path_element = vfs_path_get_by_index (vpath, -1);

    fstype = extfs_which (path_element->class, path_element->vfs_prefix);
    if (fstype == -1)
        return NULL;

    archive_name = vfs_path_to_str_elements_count (vpath, -1);

    /* All filesystems should have some local archive, at least it can be PATH_SEP ('/'). */
    parc = g_list_find_custom (extfs_subclass.supers, archive_name, extfs_cmp_archive);
    if (parc != NULL)
    {
        a = EXTFS_SUPER (parc->data);
        vfs_stamp (vfs_extfs_ops, (vfsid) a);
        g_free (archive_name);
    }
    else
    {
        if ((flags & FL_NO_OPEN) == 0)
            result = extfs_open_and_read_archive (fstype, archive_name, &a);

        g_free (archive_name);

        if (result == -1)
        {
            path_element->class->verrno = EIO;
            return NULL;
        }
    }

    *archive = a;
    return path_element->path;
}

/* --------------------------------------------------------------------------------------------- */
/* Return allocated path (without leading slash) inside the archive  */

static char *
extfs_get_path_from_entry (const struct vfs_s_entry *entry)
{
    const struct vfs_s_entry *e;
    GString *localpath;

    localpath = g_string_new ("");

    for (e = entry; e->dir != NULL; e = e->dir->ent)
    {
        g_string_prepend (localpath, e->name);
        if (e->dir->ent->dir != NULL)
            g_string_prepend_c (localpath, PATH_SEP);
    }

    return g_string_free (localpath, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static struct vfs_s_entry *
extfs_resolve_symlinks_int (struct vfs_s_entry *entry, GSList * list)
{
    struct vfs_s_entry *pent = NULL;

    if (!S_ISLNK (entry->ino->st.st_mode))
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
        pent = extfs_find_entry_int (entry->dir, entry->ino->linkname, looping, FL_NONE);
        looping = g_slist_delete_link (looping, looping);

        if (pent == NULL)
            my_errno = ENOENT;
    }

    return pent;
}

/* --------------------------------------------------------------------------------------------- */

static struct vfs_s_entry *
extfs_resolve_symlinks (struct vfs_s_entry *entry)
{
    struct vfs_s_entry *res;

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
extfs_get_archive_name (const struct extfs_super_t *archive)
{
    const char *archive_name;

    if (archive->local_name != NULL)
        archive_name = archive->local_name;
    else
        archive_name = VFS_SUPER (archive)->name;

    if (archive_name == NULL || *archive_name == '\0')
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
extfs_cmd (const char *str_extfs_cmd, const struct extfs_super_t *archive,
           const struct vfs_s_entry *entry, const char *localname)
{
    char *file;
    char *quoted_file;
    char *quoted_localname;
    char *archive_name, *quoted_archive_name;
    const extfs_plugin_info_t *info;
    char *cmd;
    int retval;

    file = extfs_get_path_from_entry (entry);
    quoted_file = name_quote (file, FALSE);
    g_free (file);

    /* Skip leading "./" (if present) added in name_quote() */
    file = extfs_skip_leading_dotslash (quoted_file);

    archive_name = extfs_get_archive_name (archive);
    quoted_archive_name = name_quote (archive_name, FALSE);
    g_free (archive_name);
    quoted_localname = name_quote (localname, FALSE);
    info = &g_array_index (extfs_plugins, extfs_plugin_info_t, archive->fstype);
    cmd = g_strconcat (info->path, info->prefix, str_extfs_cmd,
                       quoted_archive_name, " ", file, " ", quoted_localname, (char *) NULL);
    g_free (quoted_file);
    g_free (quoted_localname);
    g_free (quoted_archive_name);

    open_error_pipe ();
    retval = my_system (EXECUTE_AS_SHELL, mc_global.shell->path, cmd);
    g_free (cmd);
    close_error_pipe (D_ERROR, NULL);
    return retval;
}

/* --------------------------------------------------------------------------------------------- */

static void
extfs_run (const vfs_path_t * vpath)
{
    struct extfs_super_t *archive = NULL;
    const char *p;
    char *q, *archive_name, *quoted_archive_name;
    char *cmd;
    const extfs_plugin_info_t *info;

    p = extfs_get_path (vpath, &archive, FL_NONE);
    if (p == NULL)
        return;
    q = name_quote (p, FALSE);

    archive_name = extfs_get_archive_name (archive);
    quoted_archive_name = name_quote (archive_name, FALSE);
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
    vfs_file_handler_t *extfs_info;
    struct extfs_super_t *archive = NULL;
    const char *q;
    struct vfs_s_entry *entry;
    int local_handle;
    gboolean created = FALSE;

    q = extfs_get_path (vpath, &archive, FL_NONE);
    if (q == NULL)
        return NULL;
    entry = extfs_find_entry (VFS_SUPER (archive)->root, q, FL_NONE);
    if ((entry == NULL) && ((flags & O_CREAT) != 0))
    {
        /* Create new entry */
        entry = extfs_find_entry (VFS_SUPER (archive)->root, q, FL_MKFILE);
        created = (entry != NULL);
    }

    if (entry == NULL)
        return NULL;
    entry = extfs_resolve_symlinks (entry);
    if (entry == NULL)
        return NULL;

    if (S_ISDIR (entry->ino->st.st_mode))
        ERRNOR (EISDIR, NULL);

    if (entry->ino->localname == NULL)
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
        entry->ino->localname = g_strdup (local_filename);
        vfs_path_free (local_filename_vpath);
    }

    local_handle = open (entry->ino->localname, NO_LINEAR (flags), mode);

    if (local_handle == -1)
    {
        /* file exists(may be). Need to drop O_CREAT flag and truncate file content */
        flags = ~O_CREAT & (NO_LINEAR (flags) | O_TRUNC);
        local_handle = open (entry->ino->localname, flags, mode);
    }

    if (local_handle == -1)
        ERRNOR (EIO, NULL);

    extfs_info = g_new (vfs_file_handler_t, 1);
    vfs_s_init_fh (extfs_info, entry->ino, created);
    extfs_info->handle = local_handle;

    /* i.e. we had no open files and now we have one */
    vfs_rmstamp (vfs_extfs_ops, (vfsid) archive);
    VFS_SUPER (archive)->fd_usage++;
    return extfs_info;
}

/* --------------------------------------------------------------------------------------------- */

static ssize_t
extfs_read (void *fh, char *buffer, size_t count)
{
    vfs_file_handler_t *file = VFS_FILE_HANDLER (fh);

    return read (file->handle, buffer, count);
}

/* --------------------------------------------------------------------------------------------- */

static int
extfs_close (void *fh)
{
    vfs_file_handler_t *file = VFS_FILE_HANDLER (fh);
    int errno_code = 0;

    close (file->handle);
    file->handle = -1;

    /* Commit the file if it has changed */
    if (file->changed)
    {
        struct stat file_status;

        if (extfs_cmd
            (" copyin ", EXTFS_SUPER (VFS_FILE_HANDLER_SUPER (fh)), file->ino->ent,
             file->ino->localname))
            errno_code = EIO;

        if (stat (file->ino->localname, &file_status) != 0)
            errno_code = EIO;
        else
            file->ino->st.st_size = file_status.st_size;

        file->ino->st.st_mtime = time (NULL);
    }

    if (--VFS_FILE_HANDLER_SUPER (fh)->fd_usage == 0)
        vfs_stamp_create (vfs_extfs_ops, VFS_FILE_HANDLER_SUPER (fh));

    g_free (fh);
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
    struct extfs_super_t *archive = NULL;
    const char *q;
    struct vfs_s_entry *entry;
    GList **info;

    q = extfs_get_path (vpath, &archive, FL_NONE);
    if (q == NULL)
        return NULL;
    entry = extfs_find_entry (VFS_SUPER (archive)->root, q, FL_NONE);
    if (entry == NULL)
        return NULL;
    entry = extfs_resolve_symlinks (entry);
    if (entry == NULL)
        return NULL;
    if (!S_ISDIR (entry->ino->st.st_mode))
        ERRNOR (ENOTDIR, NULL);

    info = g_new (GList *, 1);
    *info = g_queue_peek_head_link (entry->ino->subdir);

    return info;
}

/* --------------------------------------------------------------------------------------------- */

static void *
extfs_readdir (void *data)
{
    static union vfs_dirent dir;
    GList **info = (GList **) data;

    if (*info == NULL)
        return NULL;

    g_strlcpy (dir.dent.d_name, VFS_ENTRY ((*info)->data)->name, MC_MAXPATHLEN);

    *info = g_list_next (*info);

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
extfs_stat_move (struct stat *buf, const struct vfs_s_inode *inode)
{
    *buf = inode->st;

#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
    buf->st_blksize = RECORDSIZE;
#endif
    vfs_adjust_stat (buf);
#ifdef HAVE_STRUCT_STAT_ST_MTIM
    buf->st_atim.tv_nsec = buf->st_mtim.tv_nsec = buf->st_ctim.tv_nsec = 0;
#endif
}

/* --------------------------------------------------------------------------------------------- */

static int
extfs_internal_stat (const vfs_path_t * vpath, struct stat *buf, gboolean resolve)
{
    struct extfs_super_t *archive;
    const char *q;
    struct vfs_s_entry *entry;
    int result = -1;

    q = extfs_get_path (vpath, &archive, FL_NONE);
    if (q == NULL)
        goto cleanup;
    entry = extfs_find_entry (VFS_SUPER (archive)->root, q, FL_NONE);
    if (entry == NULL)
        goto cleanup;
    if (resolve)
    {
        entry = extfs_resolve_symlinks (entry);
        if (entry == NULL)
            goto cleanup;
    }
    extfs_stat_move (buf, entry->ino);
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
extfs_fstat (void *fh, struct stat *buf)
{
    vfs_file_handler_t *file = VFS_FILE_HANDLER (fh);

    extfs_stat_move (buf, file->ino);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
extfs_readlink (const vfs_path_t * vpath, char *buf, size_t size)
{
    struct extfs_super_t *archive;
    const char *q;
    size_t len;
    struct vfs_s_entry *entry;
    int result = -1;

    q = extfs_get_path (vpath, &archive, FL_NONE);
    if (q == NULL)
        goto cleanup;
    entry = extfs_find_entry (VFS_SUPER (archive)->root, q, FL_NONE);
    if (entry == NULL)
        goto cleanup;
    if (!S_ISLNK (entry->ino->st.st_mode))
    {
        const vfs_path_element_t *path_element;

        path_element = vfs_path_get_by_index (vpath, -1);
        path_element->class->verrno = EINVAL;
        goto cleanup;
    }
    len = strlen (entry->ino->linkname);
    if (size < len)
        len = size;
    /* readlink() does not append a NUL character to buf */
    result = len;
    memcpy (buf, entry->ino->linkname, result);
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
extfs_write (void *fh, const char *buf, size_t nbyte)
{
    vfs_file_handler_t *file = VFS_FILE_HANDLER (fh);

    file->changed = TRUE;
    return write (file->handle, buf, nbyte);
}

/* --------------------------------------------------------------------------------------------- */

static int
extfs_unlink (const vfs_path_t * vpath)
{
    struct extfs_super_t *archive;
    const char *q;
    struct vfs_s_entry *entry;
    int result = -1;

    q = extfs_get_path (vpath, &archive, FL_NONE);
    if (q == NULL)
        goto cleanup;
    entry = extfs_find_entry (VFS_SUPER (archive)->root, q, FL_NONE);
    if (entry == NULL)
        goto cleanup;
    entry = extfs_resolve_symlinks (entry);
    if (entry == NULL)
        goto cleanup;
    if (S_ISDIR (entry->ino->st.st_mode))
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
    vfs_s_free_entry (VFS_SUPER (archive)->me, entry);
    result = 0;
  cleanup:
    return result;
}

/* --------------------------------------------------------------------------------------------- */

static int
extfs_mkdir (const vfs_path_t * vpath, mode_t mode)
{
    struct extfs_super_t *archive;
    const char *q;
    struct vfs_s_entry *entry;
    int result = -1;
    const vfs_path_element_t *path_element;

    (void) mode;

    path_element = vfs_path_get_by_index (vpath, -1);
    q = extfs_get_path (vpath, &archive, FL_NONE);
    if (q == NULL)
        goto cleanup;
    entry = extfs_find_entry (VFS_SUPER (archive)->root, q, FL_NONE);
    if (entry != NULL)
    {
        path_element->class->verrno = EEXIST;
        goto cleanup;
    }
    entry = extfs_find_entry (VFS_SUPER (archive)->root, q, FL_MKDIR);
    if (entry == NULL)
        goto cleanup;
    entry = extfs_resolve_symlinks (entry);
    if (entry == NULL)
        goto cleanup;
    if (!S_ISDIR (entry->ino->st.st_mode))
    {
        path_element->class->verrno = ENOTDIR;
        goto cleanup;
    }

    if (extfs_cmd (" mkdir ", archive, entry, ""))
    {
        my_errno = EIO;
        vfs_s_free_entry (VFS_SUPER (archive)->me, entry);
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
    struct extfs_super_t *archive;
    const char *q;
    struct vfs_s_entry *entry;
    int result = -1;

    q = extfs_get_path (vpath, &archive, FL_NONE);
    if (q == NULL)
        goto cleanup;
    entry = extfs_find_entry (VFS_SUPER (archive)->root, q, FL_NONE);
    if (entry == NULL)
        goto cleanup;
    entry = extfs_resolve_symlinks (entry);
    if (entry == NULL)
        goto cleanup;
    if (!S_ISDIR (entry->ino->st.st_mode))
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
    vfs_s_free_entry (VFS_SUPER (archive)->me, entry);
    result = 0;
  cleanup:
    return result;
}

/* --------------------------------------------------------------------------------------------- */

static int
extfs_chdir (const vfs_path_t * vpath)
{
    void *data;

    my_errno = ENOTDIR;
    data = extfs_opendir (vpath);
    if (data == NULL)
        return (-1);
    extfs_closedir (data);
    my_errno = 0;
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static off_t
extfs_lseek (void *fh, off_t offset, int whence)
{
    vfs_file_handler_t *file = VFS_FILE_HANDLER (fh);

    return lseek (file->handle, offset, whence);
}

/* --------------------------------------------------------------------------------------------- */

static vfsid
extfs_getid (const vfs_path_t * vpath)
{
    struct extfs_super_t *archive = NULL;
    const char *p;

    p = extfs_get_path (vpath, &archive, FL_NO_OPEN);
    return (p == NULL ? NULL : (vfsid) archive);
}

/* --------------------------------------------------------------------------------------------- */

static vfs_path_t *
extfs_getlocalcopy (const vfs_path_t * vpath)
{
    vfs_file_handler_t *fh;
    vfs_path_t *p;

    fh = VFS_FILE_HANDLER (extfs_open (vpath, O_RDONLY, 0));
    if (fh == NULL)
        return NULL;
    if (fh->ino->localname == NULL)
    {
        extfs_close ((void *) fh);
        return NULL;
    }
    p = vfs_path_from_str (fh->ino->localname);
    VFS_FILE_HANDLER_SUPER (fh)->fd_usage++;
    extfs_close ((void *) fh);
    return p;
}

/* --------------------------------------------------------------------------------------------- */

static int
extfs_ungetlocalcopy (const vfs_path_t * vpath, const vfs_path_t * local, gboolean has_changed)
{
    vfs_file_handler_t *fh;

    fh = VFS_FILE_HANDLER (extfs_open (vpath, O_RDONLY, 0));
    if (fh == NULL)
        return 0;

    if (strcmp (fh->ino->localname, vfs_path_get_last_path_str (local)) == 0)
    {
        VFS_FILE_HANDLER_SUPER (fh)->fd_usage--;
        if (has_changed)
            fh->changed = TRUE;
        extfs_close ((void *) fh);
        return 0;
    }
    else
    {
        /* Should not happen */
        extfs_close ((void *) fh);
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

            if (f >= 0)
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

    while (VFS_SUBCLASS (me)->supers != NULL)
        me->free ((vfsid) VFS_SUBCLASS (me)->supers->data);

    if (extfs_plugins == NULL)
        return;

    for (i = 0; i < extfs_plugins->len; i++)
    {
        extfs_plugin_info_t *info;

        info = &g_array_index (extfs_plugins, extfs_plugin_info_t, i);
        g_free (info->path);
        g_free (info->prefix);
    }

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
vfs_init_extfs (void)
{
    vfs_init_subclass (&extfs_subclass, "extfs", VFSF_UNKNOWN, NULL);
    vfs_extfs_ops->init = extfs_init;
    vfs_extfs_ops->done = extfs_done;
    vfs_extfs_ops->fill_names = extfs_fill_names;
    vfs_extfs_ops->which = extfs_which;
    vfs_extfs_ops->open = extfs_open;
    vfs_extfs_ops->close = extfs_close;
    vfs_extfs_ops->read = extfs_read;
    vfs_extfs_ops->write = extfs_write;
    vfs_extfs_ops->opendir = extfs_opendir;
    vfs_extfs_ops->readdir = extfs_readdir;
    vfs_extfs_ops->closedir = extfs_closedir;
    vfs_extfs_ops->stat = extfs_stat;
    vfs_extfs_ops->lstat = extfs_lstat;
    vfs_extfs_ops->fstat = extfs_fstat;
    vfs_extfs_ops->chmod = extfs_chmod;
    vfs_extfs_ops->chown = extfs_chown;
    vfs_extfs_ops->readlink = extfs_readlink;
    vfs_extfs_ops->unlink = extfs_unlink;
    vfs_extfs_ops->chdir = extfs_chdir;
    vfs_extfs_ops->ferrno = extfs_errno;
    vfs_extfs_ops->lseek = extfs_lseek;
    vfs_extfs_ops->getid = extfs_getid;
    vfs_extfs_ops->getlocalcopy = extfs_getlocalcopy;
    vfs_extfs_ops->ungetlocalcopy = extfs_ungetlocalcopy;
    vfs_extfs_ops->mkdir = extfs_mkdir;
    vfs_extfs_ops->rmdir = extfs_rmdir;
    vfs_extfs_ops->setctl = extfs_setctl;
    extfs_subclass.free_inode = extfs_free_inode;
    extfs_subclass.free_archive = extfs_free_archive;
    vfs_register_class (vfs_extfs_ops);
}

/* --------------------------------------------------------------------------------------------- */
