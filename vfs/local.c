#include <config.h>
#include <errno.h>
#include <sys/types.h>
#include <malloc.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "../src/mad.h"
#include "../src/fs.h"

#include <fcntl.h>
#include "../src/util.h"

#include "vfs.h"

    
static void *local_open (char *file, int flags, int mode)
{
    int *local_info;
    int fd;

    fd = open (file, flags, mode);
    if (fd == -1)
	return 0;

    local_info = (int *) xmalloc (sizeof (int), "Local fs");
    *local_info = fd;
    
    return local_info;
}

static int local_read (void *data, char *buffer, int count)
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

static int local_close (void *data)
{
    int fd;

    if (!data)
	return -1;
    
    fd =  *(int *) data;
    free (data);
    return close (fd);
}

static int local_errno (void)
{
    return errno;
}

static void *local_opendir (char *dirname)
{
    DIR **local_info;
    DIR *dir;

    dir = opendir (dirname);
    if (!dir)
	return 0;

    local_info = (DIR **) xmalloc (sizeof (DIR *), "Local fs");
    *local_info = dir;
    
    return local_info;
}

static void *local_readdir (void *data)
{
    return readdir (*(DIR **) data);
}

static int local_closedir (void *data)
{
    int i;

    i = closedir (* (DIR **) data);
    if (data)
	free (data);
    return i;
}

static int local_stat (char *path, struct stat *buf)
{
    return stat (path, buf);
}

static int local_lstat (char *path, struct stat *buf)
{
#ifndef HAVE_STATLSTAT
    return lstat (path,buf);
#else
    return statlstat (path, buf);
#endif
}

static int local_fstat (void *data, struct stat *buf)
{
    return fstat (*((int *) data), buf);    
}

static int local_chmod (char *path, int mode)
{
    return chmod (path, mode);
}

static int local_chown (char *path, int owner, int group)
{
    return chown (path, owner, group);
}

static int local_utime (char *path, struct utimbuf *times)
{
    return utime (path, times);
}

static int local_readlink (char *path, char *buf, int size)
{
    return readlink (path, buf, size);
}

static int local_unlink (char *path)
{
    return unlink (path);
}

static int local_symlink (char *n1, char *n2)
{
    return symlink (n1, n2);
}

static int local_write (void *data, char *buf, int nbyte)
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

static int local_rename (char *a, char *b)
{
    return rename (a, b);
}

static int local_chdir (char *path)
{
    return chdir (path);
}

static int local_lseek (void *data, off_t offset, int whence)
{
    int fd = * (int *) data;

    return lseek (fd, offset, whence);
}

static int local_mknod (char *path, int mode, int dev)
{
    return mknod (path, mode, dev);
}

static int local_link (char *p1, char *p2)
{
    return link (p1, p2);
}

static int local_mkdir (char *path, mode_t mode)
{
    return mkdir (path, mode);
}

static int local_rmdir (char *path)
{
    return rmdir (path);
}

static vfsid local_getid (char *path, struct vfs_stamping **parent)
{
    *parent = NULL;
    return (vfsid) -1; /* We do not free local fs stuff at all */
}

static int local_nothingisopen (vfsid id)
{
    return 0;
}

static void local_free (vfsid id)
{
}

static char *local_getlocalcopy (char *path)
{
    return strdup (path);
}

static void local_ungetlocalcopy (char *path, char *local, int has_changed)
{
}

#ifdef HAVE_MMAP
static caddr_t local_mmap (caddr_t addr, size_t len, int prot, int flags, void *data, off_t offset)
{
    int fd = * (int *)data;

    return mmap (addr, len, prot, flags, fd, offset);
}

static int local_munmap (caddr_t addr, size_t len, void *data)
{
    return munmap (addr, len);
}
#endif

vfs local_vfs_ops = {
    local_open,
    local_close,
    local_read,
    local_write,
    
    local_opendir,
    local_readdir,
    local_closedir,

    local_stat,
    local_lstat,
    local_fstat,

    local_chmod,
    local_chown,
    local_utime,

    local_readlink,
    local_symlink,
    local_link,
    local_unlink,

    local_rename,
    local_chdir,
    local_errno,
    local_lseek,
    local_mknod,
    
    local_getid,
    local_nothingisopen,
    local_free,
    
    local_getlocalcopy,
    local_ungetlocalcopy,
    
    local_mkdir,
    local_rmdir,
    
    NULL,
    NULL,
    NULL
#ifdef HAVE_MMAP
    ,local_mmap,
    local_munmap
#endif
};
