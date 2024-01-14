/*
   Single File fileSystem

   Copyright (C) 1998-2024
   Free Software Foundation, Inc.

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

/**
 * \file
 * \brief Source: Single File fileSystem
 *
 * This defines whole class of filesystems which contain single file
 * inside. It is somehow similar to extfs, except that extfs makes
 * whole virtual trees and we do only single virtual files.
 *
 * If you want to gunzip something, you should open it with \verbatim #ugz \endverbatim
 * suffix, DON'T try to gunzip it yourself.
 *
 * Namespace: exports vfs_sfs_ops
 */

#include <config.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "lib/global.h"
#include "lib/util.h"
#include "lib/widget.h"         /* D_ERROR, D_NORMAL */

#include "src/execute.h"        /* EXECUTE_AS_SHELL */

#include "lib/vfs/vfs.h"
#include "lib/vfs/utilvfs.h"
#include "lib/vfs/xdirentry.h"
#include "src/vfs/local/local.h"
#include "lib/vfs/gc.h"         /* vfs_stamp_create */

#include "sfs.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define MAXFS 32

typedef enum
{
    F_NONE = 0x0,
    F_1 = 0x1,
    F_2 = 0x2,
    F_NOLOCALCOPY = 0x4,
    F_FULLMATCH = 0x8
} sfs_flags_t;

#define COPY_CHAR \
    if ((size_t) (t - pad) > sizeof (pad)) \
    { \
        g_free (pqname); \
        return (-1); \
    } \
    else \
        *t++ = *s_iter;

#define COPY_STRING(a) \
    if ((t - pad) + strlen (a) > sizeof (pad)) \
    { \
        g_free (pqname); \
        return (-1); \
    } \
    else \
    { \
        strcpy (t, a); \
        t += strlen (a); \
    }

/*** file scope type declarations ****************************************************************/

typedef struct cachedfile
{
    char *name;
    char *cache;
} cachedfile;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static GSList *head = NULL;

static struct vfs_s_subclass sfs_subclass;
static struct vfs_class *vfs_sfs_ops = VFS_CLASS (&sfs_subclass);

static int sfs_no = 0;
static struct
{
    char *prefix;
    char *command;
    sfs_flags_t flags;
} sfs_info[MAXFS];

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static int
cachedfile_compare (const void *a, const void *b)
{
    const cachedfile *cf = (const cachedfile *) a;
    const char *name = (const char *) b;

    return strcmp (name, cf->name);
}

/* --------------------------------------------------------------------------------------------- */

static int
sfs_vfmake (const vfs_path_t * vpath, vfs_path_t * cache_vpath)
{
    int w;
    char pad[10240];
    char *s_iter, *t = pad;
    gboolean was_percent = FALSE;
    vfs_path_t *pname;          /* name of parent archive */
    char *pqname;               /* name of parent archive, quoted */
    const vfs_path_element_t *path_element;
    mc_pipe_t *pip;
    GError *error = NULL;

    path_element = vfs_path_get_by_index (vpath, -1);
    pname = vfs_path_clone (vpath);
    vfs_path_remove_element_by_index (pname, -1);

    w = path_element->class->which (path_element->class, path_element->vfs_prefix);
    if (w == -1)
        vfs_die ("This cannot happen... Hopefully.\n");

    if ((sfs_info[w].flags & F_1) == 0
        && strcmp (vfs_path_get_last_path_str (pname), PATH_SEP_STR) != 0)
    {
        vfs_path_free (pname, TRUE);
        return (-1);
    }

    /*    if ((sfs_info[w].flags & F_2) || (!inpath) || (!*inpath)); else return -1; */
    if ((sfs_info[w].flags & F_NOLOCALCOPY) != 0)
        pqname = name_quote (vfs_path_as_str (pname), FALSE);
    else
    {
        vfs_path_t *s;

        s = mc_getlocalcopy (pname);
        if (s == NULL)
        {
            vfs_path_free (pname, TRUE);
            return (-1);
        }

        pqname = name_quote (vfs_path_get_last_path_str (s), FALSE);
        vfs_path_free (s, TRUE);
    }

    vfs_path_free (pname, TRUE);

    for (s_iter = sfs_info[w].command; *s_iter != '\0'; s_iter++)
    {
        if (was_percent)
        {
            const char *ptr = NULL;

            was_percent = FALSE;

            switch (*s_iter)
            {
            case '1':
                ptr = pqname;
                break;
            case '2':
                ptr = path_element->path;
                break;
            case '3':
                ptr = vfs_path_get_last_path_str (cache_vpath);
                break;
            case '%':
                COPY_CHAR;
                continue;
            default:
                break;
            }

            if (ptr != NULL)
            {
                COPY_STRING (ptr);
            }
        }
        else if (*s_iter == '%')
            was_percent = TRUE;
        else
        {
            COPY_CHAR;
        }
    }

    g_free (pqname);

    /* don't read stdout */
    pip = mc_popen (pad, FALSE, TRUE, &error);
    if (pip == NULL)
    {
        message (D_ERROR, MSG_ERROR, _("SFS virtual file system:\n%s"), error->message);
        g_error_free (error);
        return (-1);
    }

    pip->err.null_term = TRUE;

    mc_pread (pip, &error);
    if (error != NULL)
    {
        message (D_ERROR, MSG_ERROR, _("SFS virtual file system:\n%s"), error->message);
        g_error_free (error);
        mc_pclose (pip, NULL);
        return (-1);
    }

    if (pip->err.len > 0)
        message (D_ERROR, MSG_ERROR, _("SFS virtual file system:\n%s"), pip->err.buf);

    mc_pclose (pip, NULL);
    return 0;                   /* OK */
}

/* --------------------------------------------------------------------------------------------- */

static const char *
sfs_redirect (const vfs_path_t * vpath)
{
    GSList *cur;
    cachedfile *cf;
    vfs_path_t *cache_vpath;
    int handle;

    cur = g_slist_find_custom (head, vfs_path_as_str (vpath), cachedfile_compare);

    if (cur != NULL)
    {
        cf = (cachedfile *) cur->data;
        vfs_stamp (vfs_sfs_ops, cf);
        return cf->cache;
    }

    handle = vfs_mkstemps (&cache_vpath, "sfs", vfs_path_get_last_path_str (vpath));

    if (handle == -1)
        return "/SOMEONE_PLAYING_DIRTY_TMP_TRICKS_ON_US";

    close (handle);

    if (sfs_vfmake (vpath, cache_vpath) == 0)
    {
        cf = g_new (cachedfile, 1);
        cf->name = g_strdup (vfs_path_as_str (vpath));
        cf->cache = vfs_path_free (cache_vpath, FALSE);
        head = g_slist_prepend (head, cf);

        vfs_stamp_create (vfs_sfs_ops, (cachedfile *) head->data);
        return cf->cache;
    }

    mc_unlink (cache_vpath);
    vfs_path_free (cache_vpath, TRUE);
    return "/I_MUST_NOT_EXIST";
}

/* --------------------------------------------------------------------------------------------- */

static void *
sfs_open (const vfs_path_t * vpath /*struct vfs_class *me, const char *path */ , int flags,
          mode_t mode)
{
    int *info;
    int fd;

    fd = open (sfs_redirect (vpath), NO_LINEAR (flags), mode);
    if (fd == -1)
        return NULL;

    info = g_new (int, 1);
    *info = fd;

    return info;
}

/* --------------------------------------------------------------------------------------------- */

static int
sfs_stat (const vfs_path_t * vpath, struct stat *buf)
{
    return stat (sfs_redirect (vpath), buf);
}

/* --------------------------------------------------------------------------------------------- */

static int
sfs_lstat (const vfs_path_t * vpath, struct stat *buf)
{
#ifndef HAVE_STATLSTAT
    return lstat (sfs_redirect (vpath), buf);
#else
    return statlstat (sfs_redirect (vpath), buf);
#endif
}

/* --------------------------------------------------------------------------------------------- */

static int
sfs_chmod (const vfs_path_t * vpath, mode_t mode)
{
    return chmod (sfs_redirect (vpath), mode);
}

/* --------------------------------------------------------------------------------------------- */

static int
sfs_chown (const vfs_path_t * vpath, uid_t owner, gid_t group)
{
    return chown (sfs_redirect (vpath), owner, group);
}

/* --------------------------------------------------------------------------------------------- */

static int
sfs_utime (const vfs_path_t * vpath, mc_timesbuf_t * times)
{
#ifdef HAVE_UTIMENSAT
    return utimensat (AT_FDCWD, sfs_redirect (vpath), *times, 0);
#else
    return utime (sfs_redirect (vpath), times);
#endif
}

/* --------------------------------------------------------------------------------------------- */

static int
sfs_readlink (const vfs_path_t * vpath, char *buf, size_t size)
{
    return readlink (sfs_redirect (vpath), buf, size);
}

/* --------------------------------------------------------------------------------------------- */

static vfsid
sfs_getid (const vfs_path_t * vpath)
{
    GSList *cur;

    cur = g_slist_find_custom (head, vfs_path_as_str (vpath), cachedfile_compare);

    return (vfsid) (cur != NULL ? cur->data : NULL);
}

/* --------------------------------------------------------------------------------------------- */

static void
sfs_free (vfsid id)
{
    struct cachedfile *which;
    GSList *cur;

    which = (struct cachedfile *) id;
    cur = g_slist_find (head, which);
    if (cur == NULL)
        vfs_die ("Free of thing which is unknown to me\n");

    which = (struct cachedfile *) cur->data;
    unlink (which->cache);
    g_free (which->cache);
    g_free (which->name);
    g_free (which);

    head = g_slist_delete_link (head, cur);
}

/* --------------------------------------------------------------------------------------------- */

static void
sfs_fill_names (struct vfs_class *me, fill_names_f func)
{
    GSList *cur;

    (void) me;

    for (cur = head; cur != NULL; cur = g_slist_next (cur))
        func (((cachedfile *) cur->data)->name);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
sfs_nothingisopen (vfsid id)
{
    /* FIXME: Investigate whether have to guard this like in
       the other VFSs (see fd_usage in extfs) -- Norbert */
    (void) id;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static vfs_path_t *
sfs_getlocalcopy (const vfs_path_t * vpath)
{
    return vfs_path_from_str (sfs_redirect (vpath));
}

/* --------------------------------------------------------------------------------------------- */

static int
sfs_ungetlocalcopy (const vfs_path_t * vpath, const vfs_path_t * local, gboolean has_changed)
{
    (void) vpath;
    (void) local;
    (void) has_changed;
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
sfs_init (struct vfs_class *me)
{
    char *mc_sfsini;
    FILE *cfg;
    char key[256];

    (void) me;

    mc_sfsini = g_build_filename (mc_global.sysconfig_dir, "sfs.ini", (char *) NULL);
    cfg = fopen (mc_sfsini, "r");

    if (cfg == NULL)
    {
        fprintf (stderr, _("%s: Warning: file %s not found\n"), "sfs_init()", mc_sfsini);
        g_free (mc_sfsini);
        return 0;
    }
    g_free (mc_sfsini);

    sfs_no = 0;
    while (sfs_no < MAXFS && fgets (key, sizeof (key), cfg) != NULL)
    {
        char *c, *semi = NULL;
        sfs_flags_t flags = F_NONE;

        if (*key == '#' || *key == '\n')
            continue;

        for (c = key; *c != '\0'; c++)
            if (*c == ':' || IS_PATH_SEP (*c))
            {
                semi = c;
                if (IS_PATH_SEP (*c))
                {
                    *c = '\0';
                    flags |= F_FULLMATCH;
                }
                break;
            }

        if (semi == NULL)
        {
          invalid_line:
            fprintf (stderr, _("Warning: Invalid line in %s:\n%s\n"), "sfs.ini", key);
            continue;
        }

        for (c = semi + 1; *c != '\0' && !whitespace (*c); c++)
            switch (*c)
            {
            case '1':
                flags |= F_1;
                break;
            case '2':
                flags |= F_2;
                break;
            case 'R':
                flags |= F_NOLOCALCOPY;
                break;
            default:
                fprintf (stderr, _("Warning: Invalid flag %c in %s:\n%s\n"), *c, "sfs.ini", key);
            }

        if (*c == '\0')
            goto invalid_line;

        c++;
        *(semi + 1) = '\0';
        semi = strchr (c, '\n');
        if (semi != NULL)
            *semi = '\0';

        sfs_info[sfs_no].prefix = g_strdup (key);
        sfs_info[sfs_no].command = g_strdup (c);
        sfs_info[sfs_no].flags = flags;
        sfs_no++;
    }
    fclose (cfg);

    return 1;
}

/* --------------------------------------------------------------------------------------------- */

static void
sfs_done (struct vfs_class *me)
{
    int i;

    (void) me;

    for (i = 0; i < sfs_no; i++)
    {
        MC_PTR_FREE (sfs_info[i].prefix);
        MC_PTR_FREE (sfs_info[i].command);
    }
    sfs_no = 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
sfs_which (struct vfs_class *me, const char *path)
{
    int i;

    (void) me;

    for (i = 0; i < sfs_no; i++)
        if ((sfs_info[i].flags & F_FULLMATCH) != 0)
        {
            if (strcmp (path, sfs_info[i].prefix) == 0)
                return i;
        }
        else if (strncmp (path, sfs_info[i].prefix, strlen (sfs_info[i].prefix)) == 0)
            return i;

    return (-1);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
vfs_init_sfs (void)
{
    /* NULLize vfs_s_subclass members */
    memset (&sfs_subclass, 0, sizeof (sfs_subclass));

    vfs_init_class (vfs_sfs_ops, "sfs", VFSF_UNKNOWN, NULL);
    vfs_sfs_ops->init = sfs_init;
    vfs_sfs_ops->done = sfs_done;
    vfs_sfs_ops->fill_names = sfs_fill_names;
    vfs_sfs_ops->which = sfs_which;
    vfs_sfs_ops->open = sfs_open;
    vfs_sfs_ops->close = local_close;
    vfs_sfs_ops->read = local_read;
    vfs_sfs_ops->stat = sfs_stat;
    vfs_sfs_ops->lstat = sfs_lstat;
    vfs_sfs_ops->fstat = local_fstat;
    vfs_sfs_ops->chmod = sfs_chmod;
    vfs_sfs_ops->chown = sfs_chown;
    vfs_sfs_ops->utime = sfs_utime;
    vfs_sfs_ops->readlink = sfs_readlink;
    vfs_sfs_ops->ferrno = local_errno;
    vfs_sfs_ops->lseek = local_lseek;
    vfs_sfs_ops->getid = sfs_getid;
    vfs_sfs_ops->nothingisopen = sfs_nothingisopen;
    vfs_sfs_ops->free = sfs_free;
    vfs_sfs_ops->getlocalcopy = sfs_getlocalcopy;
    vfs_sfs_ops->ungetlocalcopy = sfs_ungetlocalcopy;
    vfs_register_class (vfs_sfs_ops);
}

/* --------------------------------------------------------------------------------------------- */
