/*
   Virtual File System: local file system.

   Copyright (C) 1995-2024
   Free Software Foundation, Inc.

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
 * \brief Source: local FS
 */

#include <config.h>

#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#ifdef ENABLE_EXT2FS_ATTR
#include <e2p/e2p.h>            /* fgetflags(), fsetflags() */
#endif

#include "lib/global.h"

#include "lib/vfs/xdirentry.h"  /* vfs_s_subclass */
#include "lib/vfs/utilvfs.h"

#include "local.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static struct vfs_s_subclass local_subclass;
static struct vfs_class *vfs_local_ops = VFS_CLASS (&local_subclass);

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void *
local_open (const vfs_path_t *vpath, int flags, mode_t mode)
{
    int *local_info;
    int fd;
    const char *path;

    path = vfs_path_get_last_path_str (vpath);
    fd = open (path, NO_LINEAR (flags), mode);
    if (fd == -1)
        return 0;

    local_info = g_new (int, 1);
    *local_info = fd;

    return local_info;
}

/* --------------------------------------------------------------------------------------------- */

static void *
local_opendir (const vfs_path_t *vpath)
{
    DIR **local_info;
    DIR *dir = NULL;
    const char *path;

    path = vfs_path_get_last_path_str (vpath);

    /* On Linux >= 5.1, MC sometimes shows empty directories on mounted CIFS shares.
     * Rereading directory restores the directory content.
     *
     * Reopen directory, if first readdir() returns NULL and errno == EINTR.
     */
    while (dir == NULL)
    {
        dir = opendir (path);
        if (dir == NULL)
            return NULL;

        if (readdir (dir) == NULL && errno == EINTR)
        {
            closedir (dir);
            dir = NULL;
        }
        else
            rewinddir (dir);
    }

    local_info = (DIR **) g_new (DIR *, 1);
    *local_info = dir;

    return local_info;
}

/* --------------------------------------------------------------------------------------------- */

static struct vfs_dirent *
local_readdir (void *data)
{
    struct dirent *d;

    d = readdir (*(DIR **) data);

    return (d != NULL ? vfs_dirent_init (NULL, d->d_name, d->d_ino) : NULL);
}

/* --------------------------------------------------------------------------------------------- */

static int
local_closedir (void *data)
{
    int i;

    i = closedir (*(DIR **) data);
    g_free (data);
    return i;
}

/* --------------------------------------------------------------------------------------------- */

static int
local_stat (const vfs_path_t *vpath, struct stat *buf)
{
    const char *path;

    path = vfs_path_get_last_path_str (vpath);
    return stat (path, buf);
}

/* --------------------------------------------------------------------------------------------- */

static int
local_lstat (const vfs_path_t *vpath, struct stat *buf)
{
    const char *path;

    path = vfs_path_get_last_path_str (vpath);
#ifndef HAVE_STATLSTAT
    return lstat (path, buf);
#else
    return statlstat (path, buf);
#endif
}

/* --------------------------------------------------------------------------------------------- */

static int
local_chmod (const vfs_path_t *vpath, mode_t mode)
{
    const char *path;

    path = vfs_path_get_last_path_str (vpath);
    return chmod (path, mode);
}

/* --------------------------------------------------------------------------------------------- */

static int
local_chown (const vfs_path_t *vpath, uid_t owner, gid_t group)
{
    const char *path;

    path = vfs_path_get_last_path_str (vpath);
    return chown (path, owner, group);
}

/* --------------------------------------------------------------------------------------------- */

#ifdef ENABLE_EXT2FS_ATTR

static int
local_fgetflags (const vfs_path_t *vpath, unsigned long *flags)
{
    const char *path;

    path = vfs_path_get_last_path_str (vpath);
    return fgetflags (path, flags);
}

/* --------------------------------------------------------------------------------------------- */

static int
local_fsetflags (const vfs_path_t *vpath, unsigned long flags)
{
    const char *path;

    path = vfs_path_get_last_path_str (vpath);
    return fsetflags (path, flags);
}

#endif /* ENABLE_EXT2FS_ATTR */

/* --------------------------------------------------------------------------------------------- */

static int
local_utime (const vfs_path_t *vpath, mc_timesbuf_t *times)
{
    return vfs_utime (vfs_path_get_last_path_str (vpath), times);
}

/* --------------------------------------------------------------------------------------------- */

static int
local_readlink (const vfs_path_t *vpath, char *buf, size_t size)
{
    const char *path;

    path = vfs_path_get_last_path_str (vpath);
    return readlink (path, buf, size);
}

/* --------------------------------------------------------------------------------------------- */

static int
local_unlink (const vfs_path_t *vpath)
{
    const char *path;

    path = vfs_path_get_last_path_str (vpath);
    return unlink (path);
}

/* --------------------------------------------------------------------------------------------- */

static int
local_symlink (const vfs_path_t *vpath1, const vfs_path_t *vpath2)
{
    const char *path1, *path2;

    path1 = vfs_path_get_last_path_str (vpath1);
    path2 = vfs_path_get_last_path_str (vpath2);
    return symlink (path1, path2);
}

/* --------------------------------------------------------------------------------------------- */

static ssize_t
local_write (void *data, const char *buf, size_t nbyte)
{
    int fd;
    int n;

    if (data == NULL)
        return (-1);

    fd = *(int *) data;

    while ((n = write (fd, buf, nbyte)) == -1)
    {
#ifdef EAGAIN
        if (errno == EAGAIN)
            continue;
#endif
#ifdef EINTR
        if (errno == EINTR)
            continue;
#endif
        break;
    }

    return n;
}

/* --------------------------------------------------------------------------------------------- */

static int
local_rename (const vfs_path_t *vpath1, const vfs_path_t *vpath2)
{
    const char *path1, *path2;

    path1 = vfs_path_get_last_path_str (vpath1);
    path2 = vfs_path_get_last_path_str (vpath2);
    return rename (path1, path2);
}

/* --------------------------------------------------------------------------------------------- */

static int
local_chdir (const vfs_path_t *vpath)
{
    const char *path;

    path = vfs_path_get_last_path_str (vpath);
    return chdir (path);
}

/* --------------------------------------------------------------------------------------------- */

static int
local_mknod (const vfs_path_t *vpath, mode_t mode, dev_t dev)
{
    const char *path;

    path = vfs_path_get_last_path_str (vpath);
    return mknod (path, mode, dev);
}

/* --------------------------------------------------------------------------------------------- */

static int
local_link (const vfs_path_t *vpath1, const vfs_path_t *vpath2)
{
    const char *path1, *path2;

    path1 = vfs_path_get_last_path_str (vpath1);
    path2 = vfs_path_get_last_path_str (vpath2);
    return link (path1, path2);
}

/* --------------------------------------------------------------------------------------------- */

static int
local_mkdir (const vfs_path_t *vpath, mode_t mode)
{
    const char *path;

    path = vfs_path_get_last_path_str (vpath);
    return mkdir (path, mode);
}

/* --------------------------------------------------------------------------------------------- */

static int
local_rmdir (const vfs_path_t *vpath)
{
    const char *path;

    path = vfs_path_get_last_path_str (vpath);
    return rmdir (path);
}

/* --------------------------------------------------------------------------------------------- */

static vfs_path_t *
local_getlocalcopy (const vfs_path_t *vpath)
{
    return vfs_path_clone (vpath);
}

/* --------------------------------------------------------------------------------------------- */

static int
local_ungetlocalcopy (const vfs_path_t *vpath, const vfs_path_t *local, gboolean has_changed)
{
    (void) vpath;
    (void) local;
    (void) has_changed;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
local_which (struct vfs_class *me, const char *path)
{
    (void) me;
    (void) path;

    return 0;                   /* Every path which other systems do not like is expected to be ours */
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

ssize_t
local_read (void *data, char *buffer, size_t count)
{
    int n;
    int fd;

    if (data == NULL)
        return (-1);

    fd = *(int *) data;

    while ((n = read (fd, buffer, count)) == -1)
    {
#ifdef EAGAIN
        if (errno == EAGAIN)
            continue;
#endif
#ifdef EINTR
        if (errno == EINTR)
            continue;
#endif
        return (-1);
    }

    return n;
}

/* --------------------------------------------------------------------------------------------- */

int
local_close (void *data)
{
    int fd;

    if (data == NULL)
        return (-1);

    fd = *(int *) data;
    g_free (data);
    return close (fd);
}

/* --------------------------------------------------------------------------------------------- */

int
local_errno (struct vfs_class *me)
{
    (void) me;
    return errno;
}

/* --------------------------------------------------------------------------------------------- */

int
local_fstat (void *data, struct stat *buf)
{
    int fd = *(int *) data;

    return fstat (fd, buf);
}

/* --------------------------------------------------------------------------------------------- */

off_t
local_lseek (void *data, off_t offset, int whence)
{
    int fd = *(int *) data;

    return lseek (fd, offset, whence);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
local_nothingisopen (vfsid id)
{
    (void) id;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
vfs_init_localfs (void)
{
    /* NULLize vfs_s_subclass members */
    memset (&local_subclass, 0, sizeof (local_subclass));

    vfs_init_class (vfs_local_ops, "localfs", VFSF_LOCAL, NULL);
    vfs_local_ops->which = local_which;
    vfs_local_ops->open = local_open;
    vfs_local_ops->close = local_close;
    vfs_local_ops->read = local_read;
    vfs_local_ops->write = local_write;
    vfs_local_ops->opendir = local_opendir;
    vfs_local_ops->readdir = local_readdir;
    vfs_local_ops->closedir = local_closedir;
    vfs_local_ops->stat = local_stat;
    vfs_local_ops->lstat = local_lstat;
    vfs_local_ops->fstat = local_fstat;
    vfs_local_ops->chmod = local_chmod;
    vfs_local_ops->chown = local_chown;
#ifdef ENABLE_EXT2FS_ATTR
    vfs_local_ops->fgetflags = local_fgetflags;
    vfs_local_ops->fsetflags = local_fsetflags;
#endif
    vfs_local_ops->utime = local_utime;
    vfs_local_ops->readlink = local_readlink;
    vfs_local_ops->symlink = local_symlink;
    vfs_local_ops->link = local_link;
    vfs_local_ops->unlink = local_unlink;
    vfs_local_ops->rename = local_rename;
    vfs_local_ops->chdir = local_chdir;
    vfs_local_ops->ferrno = local_errno;
    vfs_local_ops->lseek = local_lseek;
    vfs_local_ops->mknod = local_mknod;
    vfs_local_ops->getlocalcopy = local_getlocalcopy;
    vfs_local_ops->ungetlocalcopy = local_ungetlocalcopy;
    vfs_local_ops->mkdir = local_mkdir;
    vfs_local_ops->rmdir = local_rmdir;
    vfs_local_ops->nothingisopen = local_nothingisopen;
    vfs_register_class (vfs_local_ops);
}

/* --------------------------------------------------------------------------------------------- */
