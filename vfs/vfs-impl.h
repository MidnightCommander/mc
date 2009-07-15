
/**
 * \file
 * \brief Header: VFS implemntation (?)
 */

#ifndef MC_VFS_IMPL_H
#define MC_VFS_IMPL_H

#ifdef USE_VFS

#include <sys/types.h>
#include <dirent.h>

#include "../src/fs.h"		/* MC_MAXPATHLEN */

typedef void *vfsid;
struct vfs_stamping;

/* Flags of VFS classes */
#define VFSF_LOCAL 1		/* Class is local (not virtual) filesystem */
#define VFSF_NOLINKS 2		/* Hard links not supported */

/**
 * This is the type of callback function passed to vfs_fill_names.
 * It gets the name of the virtual file system as its first argument.
 * See also:
 *    vfs_fill_names().
 */
typedef void (*fill_names_f) (const char *);

struct vfs_class {
    struct vfs_class *next;
    const char *name;			/* "FIles over SHell" */
    int flags;
    const char *prefix;		/* "fish:" */
    void *data;			/* this is for filesystem's own use */
    int verrno;			/* can't use errno because glibc2 might define errno as function */

    int (*init) (struct vfs_class *me);
    void (*done) (struct vfs_class *me);
    
    /**
     * The fill_names method shall call the callback function for every
     * filesystem name that this vfs module supports.
     */
    void (*fill_names) (struct vfs_class *me, fill_names_f);

    /**
     * The which() method shall return the index of the vfs subsystem
     * or -1 if this vfs cannot handle the given pathname.
     */
    int (*which) (struct vfs_class *me, const char *path);

    void *(*open) (struct vfs_class *me, const char *fname, int flags,
		   int mode);
    int (*close) (void *vfs_info);
    ssize_t (*read) (void *vfs_info, char *buffer, int count);
    ssize_t (*write) (void *vfs_info, const char *buf, int count);

    void *(*opendir) (struct vfs_class *me, const char *dirname);
    void *(*readdir) (void *vfs_info);
    int (*closedir) (void *vfs_info);

    int (*stat) (struct vfs_class *me, const char *path, struct stat * buf);
    int (*lstat) (struct vfs_class *me, const char *path, struct stat * buf);
    int (*fstat) (void *vfs_info, struct stat * buf);

    int (*chmod) (struct vfs_class *me, const char *path, int mode);
    int (*chown) (struct vfs_class *me, const char *path, int owner, int group);
    int (*utime) (struct vfs_class *me, const char *path,
		  struct utimbuf * times);

    int (*readlink) (struct vfs_class *me, const char *path, char *buf,
		     size_t size);
    int (*symlink) (struct vfs_class *me, const char *n1, const char *n2);
    int (*link) (struct vfs_class *me, const char *p1, const char *p2);
    int (*unlink) (struct vfs_class *me, const char *path);
    int (*rename) (struct vfs_class *me, const char *p1, const char *p2);
    int (*chdir) (struct vfs_class *me, const char *path);
    int (*ferrno) (struct vfs_class *me);
    off_t (*lseek) (void *vfs_info, off_t offset, int whence);
    int (*mknod) (struct vfs_class *me, const char *path, int mode, int dev);

    vfsid (*getid) (struct vfs_class *me, const char *path);

    int (*nothingisopen) (vfsid id);
    void (*free) (vfsid id);

    char *(*getlocalcopy) (struct vfs_class *me, const char *filename);
    int (*ungetlocalcopy) (struct vfs_class *me, const char *filename,
			   const char *local, int has_changed);

    int (*mkdir) (struct vfs_class *me, const char *path, mode_t mode);
    int (*rmdir) (struct vfs_class *me, const char *path);

    int (*ctl) (void *vfs_info, int ctlop, void *arg);
    int (*setctl) (struct vfs_class *me, const char *path, int ctlop,
		   void *arg);
};

/*
 * This union is used to ensure that there is enough space for the
 * filename (d_name) when the dirent structure is created.
 */
union vfs_dirent {
    struct dirent dent;
    char _extra_buffer[offsetof(struct dirent, d_name) + MC_MAXPATHLEN + 1];
};

/* Register a file system class */
int vfs_register_class (struct vfs_class *vfs);

#ifdef WITH_SMBFS
/* Interface for requesting SMB credentials.  */
struct smb_authinfo {
    char *host;
    char *share;
    char *domain;
    char *user;
    char *password;
};

struct smb_authinfo *vfs_smb_get_authinfo (const char *host,
					   const char *share,
					   const char *domain,
					   const char *user);
#endif				/* WITH_SMBFS */

struct vfs_class *vfs_get_class (const char *path);
struct vfs_class *vfs_split (char *path, char **inpath, char **op);
char *vfs_path (const char *path);
int vfs_file_class_flags (const char *filename);

void vfs_fill_names (fill_names_f);
char *vfs_translate_url (const char *);

/* vfs/direntry.c: */
void *vfs_s_open (struct vfs_class *, const char *, int, int);

#ifdef USE_NETCODE
extern int use_netrc;
#endif

void init_cpiofs (void);
void init_extfs (void);
void init_fish (void);
void init_sfs (void);
void init_tarfs (void);
void init_undelfs (void);

#endif /* USE_VFS */

#endif /* MC_VFS_IMPL_H */
