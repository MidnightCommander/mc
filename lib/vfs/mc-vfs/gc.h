
/**
 * \file
 * \brief Header: Virtual File System: garbage collection code
 */

#ifndef MC_VFS_GC_H
#define MC_VFS_GC_H

#include "vfs-impl.h"

struct vfs_stamping
{
    struct vfs_class *v;
    vfsid id;
    struct vfs_stamping *next;
    struct timeval time;
};

void vfs_stamp (struct vfs_class *vclass, vfsid id);
void vfs_rmstamp (struct vfs_class *vclass, vfsid id);
void vfs_stamp_create (struct vfs_class *vclass, vfsid id);
void vfs_timeout_handler (void);
int vfs_timeouts (void);
vfsid vfs_getid (struct vfs_class *vclass, const char *dir);
void vfs_gc_done (void);

#endif /* MC_VFS_GC_H */
