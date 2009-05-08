
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

#include "../src/global.h"

#include "../src/tty/tty.h"	/* enable/disable interrupt key */

#include "../src/wtools.h"	/* message() */
#include "../src/main.h"	/* print_vfs_message */

#include "utilvfs.h"
#include "vfs.h"
#include "local.h"

/**
 * Note: Some of this functions are not static. This has rather good
 * reason: exactly same functions would have to appear in sfs.c. This
 * saves both computer's memory and my work.  <pavel@ucw.cz>
 */

static struct vfs_class vfs_local_ops;

static void *
local_open (struct vfs_class *me, const char *file, int flags, int mode)
{
    int *local_info;
    int fd;

    (void) me;

    fd = open (file, NO_LINEAR (flags), mode);
    if (fd == -1)
	return 0;

    local_info = g_new (int, 1);
    *local_info = fd;

    return local_info;
}

ssize_t
local_read (void *data, char *buffer, int count)
{
    int n;

    if (!data)
	return -1;
    
    while ((n = read (*((int *) data), buffer, count)) == -1){
#ifdef EAGAIN
	if (errno == EAGAIN) continue;
#endif
#ifdef EINTR
	if (errno == EINTR) continue;
#endif
	return -1;
    }
    return n;
}

int
local_close (void *data)
{
    int fd;

    if (!data)
	return -1;
    
    fd =  *(int *) data;
    g_free (data);
    return close (fd);
}

int
local_errno (struct vfs_class *me)
{
    (void) me;
    return errno;
}

static void *
local_opendir (struct vfs_class *me, const char *dirname)
{
    DIR **local_info;
    DIR *dir;

    (void) me;

    dir = opendir (dirname);
    if (!dir)
	return 0;

    local_info = (DIR **) g_new (DIR *, 1);
    *local_info = dir;
    
    return local_info;
}

static void *
local_readdir (void *data)
{
    return readdir (*(DIR **) data);
}

static int
local_closedir (void *data)
{
    int i;

    i = closedir (* (DIR **) data);
    g_free (data);
    return i;
}

static int
local_stat (struct vfs_class *me, const char *path, struct stat *buf)
{
    (void) me;

    return stat (path, buf);
}

static int
local_lstat (struct vfs_class *me, const char *path, struct stat *buf)
{
    (void) me;

#ifndef HAVE_STATLSTAT
    return lstat (path,buf);
#else
    return statlstat (path, buf);
#endif
}

int
local_fstat (void *data, struct stat *buf)
{
    /* FIXME: avoid type cast */
    return fstat (*((int *) data), buf);
}

static int
local_chmod (struct vfs_class *me, const char *path, int mode)
{
    (void) me;

    return chmod (path, mode);
}

static int
local_chown (struct vfs_class *me, const char *path, int owner, int group)
{
    (void) me;

    return chown (path, owner, group);
}

static int
local_utime (struct vfs_class *me, const char *path, struct utimbuf *times)
{
    (void) me;

    return utime (path, times);
}

static int
local_readlink (struct vfs_class *me, const char *path, char *buf, size_t size)
{
    (void) me;

    return readlink (path, buf, size);
}

static int
local_unlink (struct vfs_class *me, const char *path)
{
    (void) me;

    return unlink (path);
}

static int
local_symlink (struct vfs_class *me, const char *n1, const char *n2)
{
    (void) me;

    return symlink (n1, n2);
}

static ssize_t
local_write (void *data, const char *buf, int nbyte)
{
    int fd;
    int n;

    if (!data)
	return -1;
    
    fd = * (int *) data;
    while ((n = write (fd, buf, nbyte)) == -1){
#ifdef EAGAIN
	if (errno == EAGAIN) continue;
#endif
#ifdef EINTR
	if (errno == EINTR) continue;
#endif
	break;
    }
    return n;
}

static int
local_rename (struct vfs_class *me, const char *a, const char *b)
{
    (void) me;

    return rename (a, b);
}

static int
local_chdir (struct vfs_class *me, const char *path)
{
    (void) me;

    return chdir (path);
}

off_t
local_lseek (void *data, off_t offset, int whence)
{
    int fd = * (int *) data;

    return lseek (fd, offset, whence);
}

static int
local_mknod (struct vfs_class *me, const char *path, int mode, int dev)
{
    (void) me;

    return mknod (path, mode, dev);
}

static int
local_link (struct vfs_class *me, const char *p1, const char *p2)
{
    (void) me;

    return link (p1, p2);
}

static int
local_mkdir (struct vfs_class *me, const char *path, mode_t mode)
{
    (void) me;

    return mkdir (path, mode);
}

static int
local_rmdir (struct vfs_class *me, const char *path)
{
    (void) me;

    return rmdir (path);
}

static char *
local_getlocalcopy (struct vfs_class *me, const char *path)
{
    (void) me;

    return g_strdup (path);
}

static int
local_ungetlocalcopy (struct vfs_class *me, const char *path,
		      const char *local, int has_changed)
{
    (void) me;
    (void) path;
    (void) local;
    (void) has_changed;

    return 0;
}

static int
local_which (struct vfs_class *me, const char *path)
{
    (void) me;
    (void) path;

    return 0;		/* Every path which other systems do not like is expected to be ours */
}

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
