
/**
 * \file
 * \brief Header: Virtual File System switch code
 */

#ifndef MC_VFS_VFS_H
#define MC_VFS_VFS_H

#include <sys/types.h>
#include <dirent.h>
#include <utime.h>
#include <stdio.h>

/* Flags of VFS classes */
#define VFSF_LOCAL 1	/* Class is local (not virtual) filesystem */
#define VFSF_NOLINKS 2	/* Hard links not supported */

#ifdef ENABLE_VFS

void vfs_init (void);
void vfs_shut (void);

int vfs_current_is_local (void);
int vfs_file_is_local (const char *filename);
ssize_t mc_read (int handle, void *buffer, int count);
ssize_t mc_write (int handle, const void *buffer, int count);
int mc_utime (const char *path, struct utimbuf *times);
int mc_readlink (const char *path, char *buf, int bufsiz);
int mc_ungetlocalcopy (const char *pathname, const char *local, int has_changed);
int mc_close (int handle);
off_t mc_lseek (int fd, off_t offset, int whence);
DIR *mc_opendir (const char *dirname);
struct dirent *mc_readdir (DIR * dirp);
int mc_closedir (DIR * dir);
int mc_stat (const char *path, struct stat *buf);
int mc_mknod (const char *, mode_t, dev_t);
int mc_link (const char *name1, const char *name2);
int mc_mkdir (const char *path, mode_t mode);
int mc_rmdir (const char *path);
int mc_fstat (int fd, struct stat *buf);
int mc_lstat (const char *path, struct stat *buf);
int mc_symlink (const char *name1, const char *name2);
int mc_rename (const char *original, const char *target);
int mc_chmod (const char *path, mode_t mode);
int mc_chown (const char *path, uid_t owner, gid_t group);
int mc_chdir (const char *path);
int mc_unlink (const char *path);
int mc_ctl (int fd, int ctlop, void *arg);
int mc_setctl (const char *path, int ctlop, void *arg);
int mc_open (const char *filename, int flags, ...);
char *mc_get_current_wd (char *buffer, int bufsize);
char *vfs_canon (const char *path);
char *mc_getlocalcopy (const char *pathname);
char *vfs_strip_suffix_from_filename (const char *filename);
char *vfs_translate_url (const char *url);

/* return encoding after last #enc: or NULL, if part does not contain #enc:
 * return static buffer */
const char *vfs_get_encoding (const char *path);

/* return new string */
char *vfs_translate_path_n (const char *path);

/* canonize and translate path, return new string */
char *vfs_canon_and_translate (const char *path);

#else /* ENABLE_VFS */

/* Only the routines outside of the VFS module need the emulation macros */

#define vfs_init() do { } while (0)
#define vfs_shut() do { } while (0)
#define vfs_current_is_local() (1)
#define vfs_file_is_local(x) (1)
#define vfs_strip_suffix_from_filename(x) g_strdup(x)
#define vfs_get_class(x) (struct vfs_class *)(NULL)
#define vfs_translate_url(s) g_strdup(s)
#define vfs_file_class_flags(x) (VFSF_LOCAL)
#define vfs_release_path(x)
#define vfs_add_current_stamps() do { } while (0)
#define vfs_timeout_handler() do { } while (0)
#define vfs_timeouts() 0

#define mc_getlocalcopy(x) vfs_canon(x)
#define mc_read read
#define mc_write write
#define mc_utime utime
#define mc_readlink readlink
#define mc_ungetlocalcopy(x,y,z) do { } while (0)
#define mc_close close
#define mc_lseek lseek
#define mc_opendir opendir
#define mc_readdir readdir
#define mc_closedir closedir
#define mc_stat stat
#define mc_mknod mknod
#define mc_link link
#define mc_mkdir mkdir
#define mc_rmdir rmdir
#define mc_fstat fstat
#define mc_lstat lstat
#define mc_symlink symlink
#define mc_rename rename
#define mc_chmod chmod
#define mc_chown chown
#define mc_chdir chdir
#define mc_unlink unlink
#define mc_open open
#define mc_get_current_wd(x,size) get_current_wd (x, size)

static inline int mc_setctl (const char *path, int ctlop, void *arg)
{
    (void) path;
    (void) ctlop;
    (void) arg;
    return 0;
}

static inline int mc_ctl (int fd, int ctlop, void *arg)
{
    (void) fd;
    (void) ctlop;
    (void) arg;
    return 0;
}

static inline const char *vfs_get_encoding (const char *path)
{
    (void) path;
    return NULL;
}

/* return new string */
static inline char *vfs_translate_path_n (const char *path)
{
    return ((path == NULL) ? g_strdup ("") : g_strdup (path));
}

static inline char* vfs_canon_and_translate(const char* path)
{
    char *ret_str;

    if (path == NULL)
	return g_strdup("");

    if (path[0] == PATH_SEP)
    {
	char *curr_dir = g_get_current_dir();
	ret_str = g_strdup_printf("%s" PATH_SEP_STR "%s", curr_dir, path);
	g_free(curr_dir);
    }
    else
	ret_str = g_strdup(path);

    canonicalize_pathname (ret_str);
    return ret_str;
}

static inline char *
vfs_canon (const char *path)
{
    char *p = g_strdup (path);
    canonicalize_pathname(p);
    return p;
}

#endif /* ENABLE_VFS */

char *vfs_get_current_dir (void);
/* translate path back to terminal encoding, remove all #enc: 
 * every invalid character is replaced with question mark
 * return static buffer */
char *vfs_translate_path (const char *path);

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
