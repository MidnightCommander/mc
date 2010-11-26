
/**
 * \file
 * \brief Header: VFS implemntation (?)
 */

#ifndef MC__VFS_IMPL_H
#define MC__VFS_IMPL_H

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stddef.h>
#include <utime.h>

#include "vfs.h"
#include "lib/fs.h"             /* MC_MAXPATHLEN */

/*** typedefs(not structures) and defined constants **********************************************/

typedef void *vfsid;

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

struct vfs_stamping;

struct vfs_class
{
    struct vfs_class *next;
    const char *name;           /* "FIles over SHell" */
    vfs_class_flags_t flags;
    const char *prefix;         /* "fish:" */
    void *data;                 /* this is for filesystem's own use */
    int verrno;                 /* can't use errno because glibc2 might define errno as function */

    int (*init) (struct vfs_class * me);
    void (*done) (struct vfs_class * me);

    /**
     * The fill_names method shall call the callback function for every
     * filesystem name that this vfs module supports.
     */
    void (*fill_names) (struct vfs_class * me, fill_names_f);

    /**
     * The which() method shall return the index of the vfs subsystem
     * or -1 if this vfs cannot handle the given pathname.
     */
    int (*which) (struct vfs_class * me, const char *path);

    void *(*open) (struct vfs_class * me, const char *fname, int flags, mode_t mode);
    int (*close) (void *vfs_info);
      ssize_t (*read) (void *vfs_info, char *buffer, size_t count);
      ssize_t (*write) (void *vfs_info, const char *buf, size_t count);

    void *(*opendir) (struct vfs_class * me, const char *dirname);
    void *(*readdir) (void *vfs_info);
    int (*closedir) (void *vfs_info);

    int (*stat) (struct vfs_class * me, const char *path, struct stat * buf);
    int (*lstat) (struct vfs_class * me, const char *path, struct stat * buf);
    int (*fstat) (void *vfs_info, struct stat * buf);

    int (*chmod) (struct vfs_class * me, const char *path, int mode);
    int (*chown) (struct vfs_class * me, const char *path, uid_t owner, gid_t group);
    int (*utime) (struct vfs_class * me, const char *path, struct utimbuf * times);

    int (*readlink) (struct vfs_class * me, const char *path, char *buf, size_t size);
    int (*symlink) (struct vfs_class * me, const char *n1, const char *n2);
    int (*link) (struct vfs_class * me, const char *p1, const char *p2);
    int (*unlink) (struct vfs_class * me, const char *path);
    int (*rename) (struct vfs_class * me, const char *p1, const char *p2);
    int (*chdir) (struct vfs_class * me, const char *path);
    int (*ferrno) (struct vfs_class * me);
      off_t (*lseek) (void *vfs_info, off_t offset, int whence);
    int (*mknod) (struct vfs_class * me, const char *path, mode_t mode, dev_t dev);

      vfsid (*getid) (struct vfs_class * me, const char *path);

    int (*nothingisopen) (vfsid id);
    void (*free) (vfsid id);

    char *(*getlocalcopy) (struct vfs_class * me, const char *filename);
    int (*ungetlocalcopy) (struct vfs_class * me, const char *filename,
                           const char *local, int has_changed);

    int (*mkdir) (struct vfs_class * me, const char *path, mode_t mode);
    int (*rmdir) (struct vfs_class * me, const char *path);

    int (*ctl) (void *vfs_info, int ctlop, void *arg);
    int (*setctl) (struct vfs_class * me, const char *path, int ctlop, void *arg);
};

/*
 * This union is used to ensure that there is enough space for the
 * filename (d_name) when the dirent structure is created.
 */
union vfs_dirent
{
    struct dirent dent;
    char _extra_buffer[offsetof (struct dirent, d_name) + MC_MAXPATHLEN + 1];
};

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/


/* Register a file system class */
int vfs_register_class (struct vfs_class *vfs);

struct vfs_class *vfs_split (char *path, char **inpath, char **op);
char *vfs_path (const char *path);

/* vfs/direntry.c: */
void *vfs_s_open (struct vfs_class *me, const char *file, int flags, mode_t mode);

vfsid vfs_getid (struct vfs_class *vclass, const char *dir);

#ifdef ENABLE_VFS_CPIO
void init_cpiofs (void);
#endif
#ifdef ENABLE_VFS_TAR
void init_tarfs (void);
#endif
#ifdef ENABLE_VFS_SFS
void init_sfs (void);
#endif
#ifdef ENABLE_VFS_EXTFS
void init_extfs (void);
#endif
#ifdef ENABLE_VFS_UNDELFS
void init_undelfs (void);
#endif
#ifdef ENABLE_VFS_FTP
void init_ftpfs (void);
#endif
#ifdef ENABLE_VFS_FISH
void init_fish (void);
#endif

/*** inline functions ****************************************************************************/
#endif /* MC_VFS_IMPL_H */
