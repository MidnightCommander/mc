#ifndef _VFS_LOCAL_H_
#define _VFS_LOCAL_H_

#include "vfs-impl.h"

extern void init_localfs (void);

/* these functions are used by other filesystems, so they are
 * published here. */
extern int local_close (void *data);
extern int local_read (void *data, char *buffer, int count);
extern int local_fstat (void *data, struct stat *buf);
extern int local_errno (struct vfs_class *me);
extern int local_lseek (void *data, off_t offset, int whence);
#ifdef HAVE_MMAP
extern caddr_t local_mmap (struct vfs_class *me, caddr_t addr, size_t len,
                           int prot, int flags, void *data, off_t offset);
extern int local_munmap (struct vfs_class *me, caddr_t addr, size_t len, void *data);
#endif

#endif
