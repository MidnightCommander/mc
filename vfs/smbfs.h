#ifndef MC_VFS_SMBFS_H
#define MC_VFS_SMBFS_H

void init_smbfs (void);
extern void smbfs_set_debug (int arg);
extern void smbfs_set_debugf (const char *filename);

#endif
