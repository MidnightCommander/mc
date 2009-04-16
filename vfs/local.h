
/**
 * \file
 * \brief Header: local FS
 */

#ifndef MC_VFS_LOCAL_H
#define MC_VFS_LOCAL_H

#include "vfs-impl.h"

extern void init_localfs (void);

/* these functions are used by other filesystems, so they are
 * published here. */
extern int local_close (void *data);
extern ssize_t local_read (void *data, char *buffer, int count);
extern int local_fstat (void *data, struct stat *buf);
extern int local_errno (struct vfs_class *me);
extern off_t local_lseek (void *data, off_t offset, int whence);

#endif
