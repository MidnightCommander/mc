
/**
 * \file
 * \brief Header: Virtual File System switch code
 */

#ifndef MC_VFS_VFS_H
#define MC_VFS_VFS_H

#include <sys/types.h>
#include <dirent.h>
#include <utime.h>

#ifdef USE_VFS

void vfs_init (void);
void vfs_shut (void);

int vfs_current_is_local (void);

#else /* USE_VFS */

#define vfs_init() do { } while (0)
#define vfs_shut() do { } while (0)
#define vfs_current_is_local() (1)

#endif /* USE_VFS */

char *vfs_strip_suffix_from_filename (const char *filename);
char *vfs_canon (const char *path);
char *mc_get_current_wd (char *buffer, int bufsize);
char *vfs_get_current_dir (void);
int vfs_file_is_local (const char *filename);
/* translate path back to terminal encoding, remove all #enc: 
 * every invalid character is replaced with question mark
 * return static buffer */
char *vfs_translate_path (const char *path);
/* return new string */
char *vfs_translate_path_n (const char *path);
/* return encoding after last #enc: or NULL, if part does not contain #enc:
 * return static buffer */
const char *vfs_get_encoding (const char *path);
/* canonize and translate path, return new string */
char *vfs_canon_and_translate (const char *path);

char *vfs_translate_url (const char *url);

/* Only the routines outside of the VFS module need the emulation macros */

int mc_open (const char *filename, int flags, ...);
int mc_close (int handle);
ssize_t mc_read (int handle, void *buffer, int count);
ssize_t mc_write (int handle, const void *buffer, int count);
off_t mc_lseek (int fd, off_t offset, int whence);
int mc_chdir (const char *path);

DIR *mc_opendir (const char *dirname);
struct dirent *mc_readdir (DIR * dirp);
int mc_closedir (DIR * dir);

int mc_stat (const char *path, struct stat *buf);
int mc_lstat (const char *path, struct stat *buf);
int mc_fstat (int fd, struct stat *buf);

int mc_chmod (const char *path, mode_t mode);
int mc_chown (const char *path, uid_t owner, gid_t group);
int mc_utime (const char *path, struct utimbuf *times);
int mc_readlink (const char *path, char *buf, int bufsiz);
int mc_unlink (const char *path);
int mc_symlink (const char *name1, const char *name2);
int mc_link (const char *name1, const char *name2);
int mc_mknod (const char *, mode_t, dev_t);
int mc_rename (const char *original, const char *target);
int mc_rmdir (const char *path);
int mc_mkdir (const char *path, mode_t mode);

char *mc_getlocalcopy (const char *pathname);
int mc_ungetlocalcopy (const char *pathname, const char *local, int has_changed);
int mc_ctl (int fd, int ctlop, void *arg);
int mc_setctl (const char *path, int ctlop, void *arg);

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

#endif
