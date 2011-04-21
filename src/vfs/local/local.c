/* Virtual File System: local file system.
   Copyright (C) 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2006, 2007, 2009 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */


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
#include <fcntl.h>

#include "lib/global.h"

#include "lib/vfs/utilvfs.h"

#include "local.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static struct vfs_class vfs_local_ops;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Note: Some of this functions are not static. This has rather good
 * reason: exactly same functions would have to appear in sfs.c. This
 * saves both computer's memory and my work.  <pavel@ucw.cz>
 */


/* --------------------------------------------------------------------------------------------- */

static void *
local_open (const vfs_path_t * vpath, int flags, mode_t mode)
{
    int *local_info;
    int fd;

    fd = open (vpath->unparsed, NO_LINEAR (flags), mode);
    if (fd == -1)
        return 0;

    local_info = g_new (int, 1);
    *local_info = fd;

    return local_info;
}

/* --------------------------------------------------------------------------------------------- */

static void *
local_opendir (const vfs_path_t * vpath)
{
    DIR **local_info;
    DIR *dir;

    dir = opendir (vpath->unparsed);
    if (!dir)
        return 0;

    local_info = (DIR **) g_new (DIR *, 1);
    *local_info = dir;

    return local_info;
}

/* --------------------------------------------------------------------------------------------- */

static void *
local_readdir (void *data)
{
    return readdir (*(DIR **) data);
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
local_stat (const vfs_path_t * vpath, struct stat *buf)
{
    return stat (vpath->unparsed, buf);
}

/* --------------------------------------------------------------------------------------------- */

static int
local_lstat (const vfs_path_t * vpath, struct stat *buf)
{
#ifndef HAVE_STATLSTAT
    return lstat (vpath->unparsed, buf);
#else
    return statlstat (vpath->unparsed, buf);
#endif
}

/* --------------------------------------------------------------------------------------------- */

static int
local_chmod (const vfs_path_t * vpath, int mode)
{
    return chmod (vpath->unparsed, mode);
}

/* --------------------------------------------------------------------------------------------- */

static int
local_chown (const vfs_path_t * vpath, uid_t owner, gid_t group)
{
    return chown (vpath->unparsed, owner, group);
}

/* --------------------------------------------------------------------------------------------- */

static int
local_utime (const vfs_path_t * vpath, struct utimbuf *times)
{
    return utime (vpath->unparsed, times);
}

/* --------------------------------------------------------------------------------------------- */

static int
local_readlink (const vfs_path_t * vpath, char *buf, size_t size)
{
    return readlink (vpath->unparsed, buf, size);
}

/* --------------------------------------------------------------------------------------------- */

static int
local_unlink (const vfs_path_t * vpath)
{
    return unlink (vpath->unparsed);
}

/* --------------------------------------------------------------------------------------------- */

static int
local_symlink (const vfs_path_t * vpath1, const vfs_path_t * vpath2)
{
    return symlink (vpath1->unparsed, vpath2->unparsed);
}

/* --------------------------------------------------------------------------------------------- */

static ssize_t
local_write (void *data, const char *buf, size_t nbyte)
{
    int fd;
    int n;

    if (!data)
        return -1;

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
local_rename (const vfs_path_t * vpath1, const vfs_path_t * vpath2)
{
    return rename (vpath1->unparsed, vpath2->unparsed);
}

/* --------------------------------------------------------------------------------------------- */

static int
local_chdir (const vfs_path_t * vpath)
{
    return chdir (vpath->unparsed);
}

/* --------------------------------------------------------------------------------------------- */

static int
local_mknod (const vfs_path_t * vpath, mode_t mode, dev_t dev)
{
    return mknod (vpath->unparsed, mode, dev);
}

/* --------------------------------------------------------------------------------------------- */

static int
local_link (const vfs_path_t * vpath1, const vfs_path_t * vpath2)
{
    return link (vpath1->unparsed, vpath2->unparsed);
}

/* --------------------------------------------------------------------------------------------- */

static int
local_mkdir (const vfs_path_t * vpath, mode_t mode)
{
    return mkdir (vpath->unparsed, mode);
}

/* --------------------------------------------------------------------------------------------- */

static int
local_rmdir (const vfs_path_t * vpath)
{
    return rmdir (vpath->unparsed);
}

/* --------------------------------------------------------------------------------------------- */

static char *
local_getlocalcopy (const vfs_path_t * vpath)
{
    return g_strdup (vpath->unparsed);
}

/* --------------------------------------------------------------------------------------------- */

static int
local_ungetlocalcopy (const vfs_path_t * vpath, const char *local, int has_changed)
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

    if (!data)
        return -1;

    while ((n = read (*((int *) data), buffer, count)) == -1)
    {
#ifdef EAGAIN
        if (errno == EAGAIN)
            continue;
#endif
#ifdef EINTR
        if (errno == EINTR)
            continue;
#endif
        return -1;
    }
    return n;
}

/* --------------------------------------------------------------------------------------------- */

int
local_close (void *data)
{
    int fd;

    if (!data)
        return -1;

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
    /* FIXME: avoid type cast */
    return fstat (*((int *) data), buf);
}

/* --------------------------------------------------------------------------------------------- */

off_t
local_lseek (void *data, off_t offset, int whence)
{
    int fd = *(int *) data;

    return lseek (fd, offset, whence);
}

/* --------------------------------------------------------------------------------------------- */

void
init_localfs (void)
{
    vfs_local_ops.name = "localfs";
    vfs_local_ops.flags = VFSF_LOCAL;
    vfs_local_ops.which = local_which;
    vfs_local_ops.open = local_open;
    vfs_local_ops.close = local_close;
    vfs_local_ops.read = local_read;
    vfs_local_ops.write = local_write;
    vfs_local_ops.opendir = local_opendir;
    vfs_local_ops.readdir = local_readdir;
    vfs_local_ops.closedir = local_closedir;
    vfs_local_ops.stat = local_stat;
    vfs_local_ops.lstat = local_lstat;
    vfs_local_ops.fstat = local_fstat;
    vfs_local_ops.chmod = local_chmod;
    vfs_local_ops.chown = local_chown;
    vfs_local_ops.utime = local_utime;
    vfs_local_ops.readlink = local_readlink;
    vfs_local_ops.symlink = local_symlink;
    vfs_local_ops.link = local_link;
    vfs_local_ops.unlink = local_unlink;
    vfs_local_ops.rename = local_rename;
    vfs_local_ops.chdir = local_chdir;
    vfs_local_ops.ferrno = local_errno;
    vfs_local_ops.lseek = local_lseek;
    vfs_local_ops.mknod = local_mknod;
    vfs_local_ops.getlocalcopy = local_getlocalcopy;
    vfs_local_ops.ungetlocalcopy = local_ungetlocalcopy;
    vfs_local_ops.mkdir = local_mkdir;
    vfs_local_ops.rmdir = local_rmdir;
    vfs_register_class (&vfs_local_ops);
}

/* --------------------------------------------------------------------------------------------- */
