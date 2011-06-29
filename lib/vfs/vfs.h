
/**
 * \file
 * \brief Header: Virtual File System switch code
 */

#ifndef MC__VFS_VFS_H
#define MC__VFS_VFS_H

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <utime.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stddef.h>

#include "lib/global.h"
#include "lib/fs.h"             /* MC_MAXPATHLEN */

#include "path.h"

/*** typedefs(not structures) and defined constants **********************************************/

#if defined (ENABLE_VFS_FTP) || defined (ENABLE_VFS_FISH) || defined (ENABLE_VFS_SMB)
#define ENABLE_VFS_NET 1
#endif

/**
 * This is the type of callback function passed to vfs_fill_names.
 * It gets the name of the virtual file system as its first argument.
 * See also:
 *    vfs_fill_names().
 */

#define VFS_ENCODING_PREFIX "#enc:"

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
#define IS_LINEAR(a) ((a) == (O_RDONLY | O_LINEAR))     /* Return only 0 and 1 ! */
#define NO_LINEAR(a) (((a) == (O_RDONLY | O_LINEAR)) ? O_RDONLY : (a))
#endif

/* O_LINEAR is strange beast, be careful. If you open file asserting
 * O_RDONLY | O_LINEAR, you promise:
 *
 *      a) to read file linearly from beginning to the end
 *      b) not to open another file before you close this one
 *              (this will likely go away in future)
 *      as a special gift, you may
 *      c) lseek() immediately after open(), giving ftpfs chance to
 *         reget. Be warned that this lseek() can fail, and you _have_
 *         to handle that gratefully.
 *
 * O_LINEAR allows filesystems not to create temporary file in some
 * cases (ftp transfer).                                -- pavel@ucw.cz
 */

/* And now some defines for our errors. */

#ifdef ENOSYS
#define E_NOTSUPP ENOSYS        /* for use in vfs when module does not provide function */
#else
#define E_NOTSUPP EFAULT        /* Does this happen? */
#endif

#ifdef ENOMSG
#define E_UNKNOWN ENOMSG        /* if we do not know what error happened */
#else
#define E_UNKNOWN EIO           /* if we do not know what error happened */
#endif

#ifdef EREMOTEIO
#define E_REMOTE EREMOTEIO      /* if other side of ftp/fish reports error */
#else
#define E_REMOTE ENETUNREACH    /* :-( there's no EREMOTEIO on some systems */
#endif

#ifdef EPROTO
#define E_PROTO EPROTO          /* if other side fails to follow protocol */
#else
#define E_PROTO EIO
#endif

typedef void (*fill_names_f) (const char *);

typedef void *vfsid;

/*** enums ***************************************************************************************/

/* Flags of VFS classes */
typedef enum
{
    VFSF_UNKNOWN = 0,
    VFSF_LOCAL = 1 << 0,        /* Class is local (not virtual) filesystem */
    VFSF_NOLINKS = 1 << 1       /* Hard links not supported */
} vfs_class_flags_t;

/* Operations for mc_ctl - on open file */
enum
{
    VFS_CTL_IS_NOTREADY
};

/* Operations for mc_setctl - on path */
enum
{
    VFS_SETCTL_FORGET,
    VFS_SETCTL_RUN,
    VFS_SETCTL_LOGFILE,
    VFS_SETCTL_FLUSH,           /* invalidate directory cache */

    /* Setting this makes vfs layer give out potentially incorrect data,
       but it also makes some operations much faster. Use with caution. */
    VFS_SETCTL_STALE_DATA
};

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct vfs_class
{
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

    void *(*open) (const vfs_path_t * vpath, int flags, mode_t mode);
    int (*close) (void *vfs_info);
      ssize_t (*read) (void *vfs_info, char *buffer, size_t count);
      ssize_t (*write) (void *vfs_info, const char *buf, size_t count);

    void *(*opendir) (const vfs_path_t * vpath);
    void *(*readdir) (void *vfs_info);
    int (*closedir) (void *vfs_info);

    int (*stat) (const vfs_path_t * vpath, struct stat * buf);
    int (*lstat) (const vfs_path_t * vpath, struct stat * buf);
    int (*fstat) (void *vfs_info, struct stat * buf);

    int (*chmod) (const vfs_path_t * vpath, mode_t mode);
    int (*chown) (const vfs_path_t * vpath, uid_t owner, gid_t group);
    int (*utime) (const vfs_path_t * vpath, struct utimbuf * times);

    int (*readlink) (const vfs_path_t * vpath, char *buf, size_t size);
    int (*symlink) (const vfs_path_t * vpath1, const vfs_path_t * vpath2);
    int (*link) (const vfs_path_t * vpath1, const vfs_path_t * vpath2);
    int (*unlink) (const vfs_path_t * vpath);
    int (*rename) (const vfs_path_t * vpath1, const vfs_path_t * vpath2);
    int (*chdir) (const vfs_path_t * vpath);
    int (*ferrno) (struct vfs_class * me);
      off_t (*lseek) (void *vfs_info, off_t offset, int whence);
    int (*mknod) (const vfs_path_t * vpath, mode_t mode, dev_t dev);

      vfsid (*getid) (const vfs_path_t * vpath);

    int (*nothingisopen) (vfsid id);
    void (*free) (vfsid id);

    char *(*getlocalcopy) (const vfs_path_t * vpath);
    int (*ungetlocalcopy) (const vfs_path_t * vpath, const char *local, int has_changed);

    int (*mkdir) (const vfs_path_t * vpath, mode_t mode);
    int (*rmdir) (const vfs_path_t * vpath);

    int (*ctl) (void *vfs_info, int ctlop, void *arg);
    int (*setctl) (const vfs_path_t * vpath, int ctlop, void *arg);
} vfs_class;

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

extern int vfs_timeout;

#ifdef ENABLE_VFS_NET
extern int use_netrc;
#endif

/*** declarations of public functions ************************************************************/

/* lib/vfs/direntry.c: */
void *vfs_s_open (const vfs_path_t * vpath, int flags, mode_t mode);

vfsid vfs_getid (const vfs_path_t * vpath);

void vfs_init (void);
void vfs_shut (void);
/* Register a file system class */
gboolean vfs_register_class (struct vfs_class *vfs);

void vfs_setup_work_dir (void);

void vfs_timeout_handler (void);
int vfs_timeouts (void);
void vfs_expire (int now);

char *vfs_get_current_dir (void);
vfs_path_t *vfs_get_raw_current_dir (void);
void vfs_set_raw_current_dir (const vfs_path_t * vpath);

gboolean vfs_current_is_local (void);
gboolean vfs_file_is_local (const vfs_path_t * vpath);

char *vfs_strip_suffix_from_filename (const char *filename);

vfs_class_flags_t vfs_file_class_flags (const vfs_path_t * vpath);

/* translate path back to terminal encoding, remove all #enc:
 * every invalid character is replaced with question mark
 * return static buffer */
char *vfs_translate_path (const char *path);
/* return new string */
char *vfs_translate_path_n (const char *path);

void vfs_stamp_path (const char *path);

void vfs_release_path (const char *dir);

void vfs_fill_names (fill_names_f);

void vfs_print_message (const char *msg, ...) __attribute__ ((format (__printf__, 1, 2)));

int vfs_ferrno (struct vfs_class *vfs);

int vfs_new_handle (struct vfs_class *vclass, void *fsinfo);

struct vfs_class *vfs_class_find_by_handle (int handle);

void *vfs_class_data_find_by_handle (int handle);

void vfs_free_handle (int handle);

char *_vfs_get_cwd (void);

vfs_path_t *vfs_change_encoding (vfs_path_t * vpath, const char *encoding);

/**
 * Interface functions described in interface.c
 */
ssize_t mc_read (int handle, void *buffer, size_t count);
ssize_t mc_write (int handle, const void *buffer, size_t count);
int mc_utime (const char *path, struct utimbuf *times);
int mc_readlink (const char *path, char *buf, size_t bufsiz);
int mc_close (int handle);
off_t mc_lseek (int fd, off_t offset, int whence);
DIR *mc_opendir (const char *dirname);
struct dirent *mc_readdir (DIR * dirp);
int mc_closedir (DIR * dir);
int mc_stat (const char *path, struct stat *buf);
int mc_mknod (const char *path, mode_t mode, dev_t dev);
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
char *mc_get_current_wd (char *buffer, size_t bufsize);
char *mc_getlocalcopy (const char *pathname);
int mc_ungetlocalcopy (const char *pathname, const char *local, int has_changed);


/*** inline functions ****************************************************************************/
#endif /* MC_VFS_VFS_H */
