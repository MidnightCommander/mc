#ifndef __GC_H
#define __GC_H

struct vfs_stamping {
    struct vfs_class *v;
    vfsid id;
    struct vfs_stamping *parent;
    struct vfs_stamping *next;
    struct timeval time;
};

extern int vfs_timeout;

void vfs_stamp (struct vfs_class *, vfsid);
void vfs_rmstamp (struct vfs_class *, vfsid, int);
void vfs_add_noncurrent_stamps (struct vfs_class *, vfsid,
				struct vfs_stamping *);
void vfs_add_current_stamps (void);
void vfs_timeout_handler (void);
void vfs_expire (int);
int vfs_timeouts (void);
void vfs_release_path (const char *dir);
vfsid vfs_getid (struct vfs_class *vclass, const char *path,
		 struct vfs_stamping **parent);
vfsid vfs_ncs_getid (struct vfs_class *nvfs, const char *dir,
		     struct vfs_stamping **par);
void vfs_gc_done (void);

#endif				/* __GC_H */
