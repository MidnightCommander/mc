#ifndef __GC_H
#define __GC_H

#include "vfs-impl.h"

struct vfs_stamping {
    struct vfs_class *v;
    vfsid id;
    struct vfs_stamping *next;
    struct timeval time;
};

extern int vfs_timeout;

void vfs_stamp (struct vfs_class *vclass, vfsid id);
void vfs_rmstamp (struct vfs_class *vclass, vfsid id);
void vfs_stamp_create (struct vfs_class *vclass, vfsid id);
void vfs_add_current_stamps (void);
void vfs_timeout_handler (void);
void vfs_expire (int now);
int vfs_timeouts (void);
void vfs_release_path (const char *dir);
vfsid vfs_getid (struct vfs_class *vclass, const char *dir);
void vfs_gc_done (void);

#endif				/* __GC_H */
