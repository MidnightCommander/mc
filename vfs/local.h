#ifndef _VFS_LOCAL_H_
#define _VFS_LOCAL_H_

extern int local_close (void *data);
extern int local_read (void *data, char *buffer, int count);
extern int local_fstat (void *data, struct stat *buf);
extern int local_errno (vfs *me);
extern int local_lseek (void *data, off_t offset, int whence);
#ifdef HAVE_MMAP
extern caddr_t local_mmap (vfs *me, caddr_t addr, size_t len, int prot, int flags, void *data, off_t offset);
extern int local_munmap (vfs *me, caddr_t addr, size_t len, void *data);
#endif

#endif
