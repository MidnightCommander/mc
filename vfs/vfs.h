#ifndef __VFS_H
#define __VFS_H

/* Flags of VFS classes */
#define VFSF_LOCAL 1		/* Class is local (not virtual) filesystem */
#define VFSF_NOLINKS 2		/* Hard links not supported */

#ifdef USE_VFS
#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif


/* Our virtual file system layer */
typedef void *vfsid;
struct vfs_stamping;

/*
 * Notice: Andrej Borsenkow <borsenkow.msk@sni.de> reports system
 * (RelianUNIX), where it is bad idea to define struct vfs. That system
 * has include called <sys/vfs.h>, which contains things like vfs_t.
 */

typedef struct vfs_class vfs;

struct vfs_class {
    vfs *next;
    char *name;			/* "FIles over SHell" */
    int flags;
    char *prefix;		/* "fish:" */
    void *data;			/* this is for filesystem's own use */
    int verrno;			/* can't use errno because glibc2 might define errno as function */

    int (*init) (vfs *me);
    void (*done) (vfs *me);
    void (*fill_names) (vfs *me, void (*)(char *));

    int (*which) (vfs *me, char *path);

    void *(*open) (vfs *me, char *fname, int flags, int mode);
    int (*close) (void *vfs_info);
    int (*read) (void *vfs_info, char *buffer, int count);
    int (*write) (void *vfs_info, char *buf, int count);

    void *(*opendir) (vfs *me, char *dirname);
    void *(*readdir) (void *vfs_info);
    int (*closedir) (void *vfs_info);
    int (*telldir) (void *vfs_info);
    void (*seekdir) (void *vfs_info, int offset);

    int (*stat) (vfs *me, char *path, struct stat * buf);
    int (*lstat) (vfs *me, char *path, struct stat * buf);
    int (*fstat) (void *vfs_info, struct stat * buf);

    int (*chmod) (vfs *me, char *path, int mode);
    int (*chown) (vfs *me, char *path, int owner, int group);
    int (*utime) (vfs *me, char *path, struct utimbuf * times);

    int (*readlink) (vfs *me, char *path, char *buf, int size);
    int (*symlink) (vfs *me, char *n1, char *n2);
    int (*link) (vfs *me, char *p1, char *p2);
    int (*unlink) (vfs *me, char *path);
    int (*rename) (vfs *me, char *p1, char *p2);
    int (*chdir) (vfs *me, char *path);
    int (*ferrno) (vfs *me);
    int (*lseek) (void *vfs_info, off_t offset, int whence);
    int (*mknod) (vfs *me, char *path, int mode, int dev);

    vfsid (*getid) (vfs *me, const char *path, struct vfs_stamping ** parent);

    int (*nothingisopen) (vfsid id);
    void (*free) (vfsid id);

    char *(*getlocalcopy) (vfs *me, char *filename);
    int (*ungetlocalcopy) (vfs *me, char *filename, char *local,
			   int has_changed);

    int (*mkdir) (vfs *me, char *path, mode_t mode);
    int (*rmdir) (vfs *me, char *path);

    int (*ctl) (void *vfs_info, int ctlop, int arg);
    int (*setctl) (vfs *me, char *path, int ctlop, char *arg);
#ifdef HAVE_MMAP
    caddr_t (*mmap) (vfs *me, caddr_t addr, size_t len, int prot,
		     int flags, void *vfs_info, off_t offset);
    int (*munmap) (vfs *me, caddr_t addr, size_t len, void *vfs_info);
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
void init_fish (void);

extern vfs vfs_local_ops;
extern vfs vfs_tarfs_ops;
extern vfs vfs_ftpfs_ops;
extern vfs vfs_smbfs_ops;
extern vfs vfs_mcfs_ops;
extern vfs vfs_extfs_ops;
extern vfs vfs_sfs_ops;
extern vfs vfs_undelfs_ops;

struct vfs_stamping {
    vfs *v;
    vfsid id;
    struct vfs_stamping *parent;	/* At the moment applies to tarfs only */
    struct vfs_stamping *next;
    struct timeval time;
};

void vfs_init (void);
void vfs_shut (void);

struct vfs_class *vfs_get_class (const char *path);
vfs *vfs_split (const char *path, char **inpath, char **op);
void vfs_rm_parents (struct vfs_stamping *stamp);
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

extern int vfs_timeout;

void vfs_stamp (vfs *, vfsid);
void vfs_rmstamp (vfs *, vfsid, int);
void vfs_add_noncurrent_stamps (vfs *, vfsid, struct vfs_stamping *);
void vfs_add_current_stamps (void);
void vfs_timeout_handler (void);
void vfs_expire (int);
int vfs_timeouts (void);
void vfs_release_path (const char *dir);

void vfs_fill_names (void (*)(char *));
char *vfs_translate_url (const char *);

void ftpfs_set_debug (const char *file);
#ifdef USE_NETCODE
void ftpfs_hint_reread (int reread);
void ftpfs_flushdir (void);
extern int use_netrc;
#else
#   define ftpfs_flushdir()
#   define ftpfs_hint_reread(x)
#endif

/* Only the routines outside of the VFS module need the emulation macros */

int mc_open (const char *filename, int flags, ...);
int mc_close (int handle);
int mc_read (int handle, char *buffer, int count);
int mc_write (int handle, char *buffer, int count);
off_t mc_lseek (int fd, off_t offset, int whence);
int mc_chdir (char *);

DIR *mc_opendir (char *dirname);
struct dirent *mc_readdir (DIR * dirp);
int mc_closedir (DIR * dir);
int mc_telldir (DIR * dir);
void mc_seekdir (DIR * dir, int offset);

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
int mc_ungetlocalcopy (const char *pathname, char *local, int has_changed);
char *mc_def_getlocalcopy (vfs *vfs, char *filename);
int mc_def_ungetlocalcopy (vfs *vfs, char *filename, char *local,
			   int has_changed);
int mc_ctl (int fd, int ctlop, int arg);
int mc_setctl (char *path, int ctlop, char *arg);
#ifdef HAVE_MMAP
caddr_t mc_mmap (caddr_t, size_t, int, int, int, off_t);
int mc_munmap (caddr_t addr, size_t len);
#endif				/* HAVE_MMAP */

#else

#undef USE_NETCODE

#define vfs_fill_names(x) do { } while (0)
#define vfs_add_current_stamps() do { } while (0)
#define vfs_current_is_local() 1
#define vfs_file_is_local(x) 1
#define vfs_file_class_flags(x) (VFSF_LOCAL)
#define vfs_path(x) x
#define vfs_strip_suffix_from_filename(x) g_strdup(x)
#define vfs_release_path(x)
#define mc_close close
#define mc_read read
#define mc_write write
#define mc_lseek lseek
#define mc_opendir opendir
#define mc_readdir readdir
#define mc_closedir closedir
#define mc_telldir telldir
#define mc_seekdir seekdir

#define mc_get_current_wd(x,size) get_current_wd (x, size)
#define mc_fstat fstat
#define mc_lstat lstat

#define mc_readlink readlink
#define mc_symlink symlink
#define mc_rename rename

#define mc_open open
#define mc_utime utime
#define mc_chmod chmod
#define mc_chown chown
#define mc_chdir chdir
#define mc_unlink unlink

#define mc_mmap mmap
#define mc_munmap munmap

#define mc_ctl(a,b,c) 0
static inline int
mc_setctl (char *path, int ctlop, char *arg)
{
    return 0;
}

#define vfs_translate_url(s) g_strdup(s)

#define mc_stat stat
#define mc_mknod mknod
#define mc_link link
#define mc_mkdir mkdir
#define mc_rmdir rmdir
#define vfs_get_class(x) (struct vfs_class *)(NULL)
#define vfs_init() do { } while (0)
#define vfs_shut() do { } while (0)
#define vfs_canon(p) g_strdup (canonicalize_pathname(p))
#define vfs_timeout_handler() do { } while (0)
#define vfs_timeouts() 0
#define vfs_force_expire() do { } while (0)

#define mc_getlocalcopy(x) NULL
#define mc_ungetlocalcopy(x,y,z) do { } while (0)

#define ftpfs_hint_reread(x) do { } while (0)
#define ftpfs_flushdir() do { } while (0)

#endif				/* USE_VFS */

/* These functions are meant for use by vfs modules */

extern int vfs_parse_ls_lga (const char *p, struct stat *s,
			     char **filename, char **linkname);
extern int vfs_split_text (char *p);
extern int vfs_parse_filetype (char c);
extern int vfs_parse_filemode (const char *p);
extern int vfs_parse_filedate (int idx, time_t * t);

extern void vfs_die (const char *msg);
extern char *vfs_get_password (char *msg);

/* Flags for vfs_split_url() */
#define URL_ALLOW_ANON 1
#define URL_NOSLASH 2

extern char *vfs_split_url (const char *path, char **host, char **user,
			    int *port, char **pass, int default_port,
			    int flags);

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

extern void vfs_print_stats (const char *fs_name, const char *action,
			     const char *file_name, off_t have,
			     off_t need);

/* Don't use values 0..4 for a while -- 10/98, pavel@ucw.cz */
#define MCCTL_REMOVELOCALCOPY   5
#define MCCTL_IS_NOTREADY	6
#define MCCTL_FORGET_ABOUT	7
#define MCCTL_EXTFS_RUN		8
/* These two make vfs layer give out potentially incorrect data, but
   they also make some operation 100 times faster. Use with caution. */
#define MCCTL_WANT_STALE_DATA	9
#define MCCTL_NO_STALE_DATA    10

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

#ifdef HAVE_MMAP
#define MMAPNULL , NULL, NULL
#else
#define MMAPNULL
#endif

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
