#ifndef __VFS_H
#define __VFS_H

#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif


typedef void *vfsid;
struct vfs_stamping;

/* Flags of VFS classes */
#define VFSF_LOCAL 1		/* Class is local (not virtual) filesystem */
#define VFSF_NOLINKS 2		/* Hard links not supported */

typedef void (*fill_names_f) (const char *);

struct vfs_class {
    struct vfs_class *next;
    char *name;			/* "FIles over SHell" */
    int flags;
    char *prefix;		/* "fish:" */
    void *data;			/* this is for filesystem's own use */
    int verrno;			/* can't use errno because glibc2 might define errno as function */

    int (*init) (struct vfs_class *me);
    void (*done) (struct vfs_class *me);
    void (*fill_names) (struct vfs_class *me, fill_names_f);

    int (*which) (struct vfs_class *me, const char *path);

    void *(*open) (struct vfs_class *me, const char *fname, int flags,
		   int mode);
    int (*close) (void *vfs_info);
    int (*read) (void *vfs_info, char *buffer, int count);
    int (*write) (void *vfs_info, /*FIXME:const*/ char *buf, int count);

    void *(*opendir) (struct vfs_class *me, const char *dirname);
    void *(*readdir) (void *vfs_info);
    int (*closedir) (void *vfs_info);

    int (*stat) (struct vfs_class *me, const char *path, struct stat * buf);
    int (*lstat) (struct vfs_class *me, const char *path, struct stat * buf);
    int (*fstat) (void *vfs_info, struct stat * buf);

    int (*chmod) (struct vfs_class *me, /*FIXME:const*/ char *path, int mode);
    int (*chown) (struct vfs_class *me, /*FIXME:const*/ char *path, int owner, int group);
    int (*utime) (struct vfs_class *me, /*FIXME:const*/ char *path,
		  struct utimbuf * times);

    int (*readlink) (struct vfs_class *me, /*FIXME:const*/ char *path, char *buf,
		     int size);
    int (*symlink) (struct vfs_class *me, /*FIXME:const*/ char *n1, /*FIXME:const*/ char *n2);
    int (*link) (struct vfs_class *me, /*FIXME:const*/ char *p1, /*FIXME:const*/ char *p2);
    int (*unlink) (struct vfs_class *me, /*FIXME:const*/ char *path);
    int (*rename) (struct vfs_class *me, /*FIXME:const*/ char *p1, /*FIXME:const*/ char *p2);
    int (*chdir) (struct vfs_class *me, const char *path);
    int (*ferrno) (struct vfs_class *me);
    int (*lseek) (void *vfs_info, off_t offset, int whence);
    int (*mknod) (struct vfs_class *me, /*FIXME:const*/ char *path, int mode, int dev);

    vfsid (*getid) (struct vfs_class *me, const char *path);

    int (*nothingisopen) (vfsid id);
    void (*free) (vfsid id);

    char *(*getlocalcopy) (struct vfs_class *me, const char *filename);
    int (*ungetlocalcopy) (struct vfs_class *me, const char *filename,
			   const char *local, int has_changed);

    int (*mkdir) (struct vfs_class *me, /*FIXME:const*/ char *path, mode_t mode);
    int (*rmdir) (struct vfs_class *me, /*FIXME:const*/ char *path);

    int (*ctl) (void *vfs_info, int ctlop, void *arg);
    int (*setctl) (struct vfs_class *me, /*FIXME:const*/ char *path, int ctlop,
		   void *arg);
#ifdef HAVE_MMAP
    caddr_t (*mmap) (struct vfs_class *me, caddr_t addr, size_t len,
		     int prot, int flags, void *vfs_info, off_t offset);
    int (*munmap) (struct vfs_class *me, caddr_t addr, size_t len,
		   void *vfs_info);
#endif
};

/*
 * This union is used to ensure that there is enough space for the
 * filename (d_name) when the dirent structure is created.
 */
union vfs_dirent {
    struct dirent dent;
    char _extra_buffer[((int) &((struct dirent *) 0)->d_name) +
		       MC_MAXPATHLEN + 1];
};

/* Register a file system class */
int vfs_register_class (struct vfs_class *vfs);

void init_cpiofs (void);
void init_extfs (void);
void init_fish (void);
void init_ftpfs (void);
void init_localfs (void);
void init_mcfs (void);
void init_sfs (void);
void init_smbfs (void);
void init_tarfs (void);
void init_undelfs (void);

void vfs_init (void);
void vfs_shut (void);

struct vfs_class *vfs_get_class (const char *path);
struct vfs_class *vfs_split (char *path, char **inpath, char **op);
char *vfs_path (const char *path);
char *vfs_strip_suffix_from_filename (const char *filename);
char *vfs_canon (const char *path);
char *mc_get_current_wd (char *buffer, int bufsize);
char *vfs_get_current_dir (void);
int vfs_current_is_local (void);
int vfs_file_class_flags (const char *filename);

static inline int
vfs_file_is_local (const char *filename)
{
    return vfs_file_class_flags (filename) & VFSF_LOCAL;
}

void vfs_fill_names (fill_names_f);
char *vfs_translate_url (const char *);

#ifdef USE_NETCODE
extern int use_netrc;
#endif

/* Only the routines outside of the VFS module need the emulation macros */

int mc_open (const char *filename, int flags, ...);
int mc_close (int handle);
int mc_read (int handle, char *buffer, int count);
int mc_write (int handle, char *buffer, int count);
off_t mc_lseek (int fd, off_t offset, int whence);
int mc_chdir (const char *path);

DIR *mc_opendir (const char *dirname);
struct dirent *mc_readdir (DIR * dirp);
int mc_closedir (DIR * dir);

int mc_stat (const char *path, struct stat *buf);
int mc_lstat (const char *path, struct stat *buf);
int mc_fstat (int fd, struct stat *buf);

int mc_chmod (char *path, int mode);
int mc_chown (char *path, int owner, int group);
int mc_utime (char *path, struct utimbuf *times);
int mc_readlink (char *path, char *buf, int bufsiz);
int mc_unlink (char *path);
int mc_symlink (char *name1, char *name2);
int mc_link (const char *name1, const char *name2);
int mc_mknod (char *, int, int);
int mc_rename (const char *original, const char *target);
int mc_rmdir (char *path);
int mc_mkdir (char *path, mode_t mode);

char *mc_getlocalcopy (const char *pathname);
int mc_ungetlocalcopy (const char *pathname, const char *local, int has_changed);
int mc_ctl (int fd, int ctlop, void *arg);
int mc_setctl (char *path, int ctlop, void *arg);
#ifdef HAVE_MMAP
caddr_t mc_mmap (caddr_t, size_t, int, int, int, off_t);
int mc_munmap (caddr_t addr, size_t len);
#endif				/* HAVE_MMAP */

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

/* Operations for mc_ctl - on open file */
enum {
    VFS_CTL_IS_NOTREADY
};

/* Operations for mc_setctl - on path */
enum {
    VFS_SETCTL_FORGET,
    VFS_SETCTL_RUN,
    VFS_SETCTL_LOGFILE,
    VFS_SETCTL_FLUSH,	/* invalidate directory cache */

    /* Setting this makes vfs layer give out potentially incorrect data,
       but it also makes some operations much faster. Use with caution. */
    VFS_SETCTL_STALE_DATA
};

#define O_ALL (O_CREAT | O_EXCL | O_NOCTTY | O_NDELAY | O_SYNC | O_WRONLY | O_RDWR | O_RDONLY)
/* Midnight commander code should _not_ use other flags than those
   listed above and O_APPEND */

#if (O_ALL & O_APPEND)
 #warning "Unexpected problem with flags, O_LINEAR disabled, contact pavel@ucw.cz"
#define O_LINEAR 0
#define IS_LINEAR(a) 0
#define NO_LINEAR(a) a
#else
#define O_LINEAR O_APPEND
#define IS_LINEAR(a) ((a) == (O_RDONLY | O_LINEAR))	/* Return only 0 and 1 ! */
#define NO_LINEAR(a) (((a) == (O_RDONLY | O_LINEAR)) ? O_RDONLY : (a))
#endif

/* O_LINEAR is strange beast, be careful. If you open file asserting
 * O_RDONLY | O_LINEAR, you promise:
 *
 *     	a) to read file linearly from beginning to the end
 *	b) not to open another file before you close this one
 *		(this will likely go away in future)
 *	as a special gift, you may
 *	c) lseek() immediately after open(), giving ftpfs chance to
 *	   reget. Be warned that this lseek() can fail, and you _have_
 *         to handle that gratefully.
 *
 * O_LINEAR allows filesystems not to create temporary file in some
 * cases (ftp transfer).				-- pavel@ucw.cz
 */

/* And now some defines for our errors. */

#ifdef ENOSYS
#define E_NOTSUPP ENOSYS	/* for use in vfs when module does not provide function */
#else
#define E_NOTSUPP EFAULT	/* Does this happen? */
#endif

#ifdef ENOMSG
#define E_UNKNOWN ENOMSG	/* if we do not know what error happened */
#else
#define E_UNKNOWN EIO		/* if we do not know what error happened */
#endif

#ifdef EREMOTEIO
#define E_REMOTE EREMOTEIO	/* if other side of ftp/fish reports error */
#else
#define E_REMOTE ENETUNREACH	/* :-( there's no EREMOTEIO on some systems */
#endif

#ifdef EPROTO
#define E_PROTO EPROTO		/* if other side fails to follow protocol */
#else
#define E_PROTO EIO
#endif

#endif				/* __VFS_H */
