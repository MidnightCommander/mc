#ifndef __VFS_H
#define __VFS_H

#include <config.h>
#include <sys/stat.h>
#include <dirent.h>

#if 0
#include "src/mad.h"
#endif

#if !defined(SCO_FLAVOR) || !defined(_SYS_SELECT_H)
#	include <sys/time.h>	/* alex: this redefines struct timeval */
#endif /* SCO_FLAVOR */

#ifdef HAVE_UTIME_H
#    include <utime.h>
#else
struct utimbuf {
	time_t actime;
	time_t modtime;
};
#endif

#ifdef USE_VFS
#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif

#ifdef VFS_STANDALONE
#undef USE_EXT2FSLIB
#else
#undef BROKEN_PATHS
/*
 * Define this to allow /any/path/#ftp/ to access ftp tree. Broken, yes.
 */ 
#endif

    /* Our virtual file system layer */
    
    typedef void * vfsid;

    struct vfs_stamping;

/*
 * Notice: Andrej Borsenkow <borsenkow.msk@sni.de> reports system
 * (RelianUNIX), where it is bad idea to define struct vfs. That system
 * has include called <sys/vfs.h>, which contains things like vfs_t.
 */

    typedef struct _vfs vfs;

    struct _vfs {
        vfs   *next;
        char  *name;		/* "FIles over SHell" */
        int   flags;
#define F_EXEC 1
#define F_NET 2
        char  *prefix;		/* "fish:" */
        void  *data;		/* this is for filesystem's own use */
        int   verrno;           /* can't use errno because glibc2 might define errno as function */

        int   (*init)          (vfs *me);
        void  (*done)          (vfs *me);
        void  (*fill_names)    (vfs *me, void (*)(char *));
			       
        int   (*which)         (vfs *me, char *path);
			       
	void  *(*open) 	       (vfs *me, char *fname, int flags, int mode);
	int   (*close) 	       (void *vfs_info);
	int   (*read)  	       (void *vfs_info, char *buffer, int count);
	int   (*write) 	       (void *vfs_info, char *buf, int count);
			       
	void  *(*opendir)      (vfs *me, char *dirname);
	void  *(*readdir)      (void *vfs_info);
	int   (*closedir)      (void *vfs_info);
 	int   (*telldir)       (void *vfs_info);
 	void  (*seekdir)       (void *vfs_info, int offset);
			       
	int  (*stat)           (vfs *me, char *path, struct stat *buf);
	int  (*lstat)  	       (vfs *me, char *path, struct stat *buf);
	int  (*fstat)  	       (void *vfs_info, struct stat *buf);
			       
	int  (*chmod)  	       (vfs *me, char *path, int mode);
	int  (*chown)  	       (vfs *me, char *path, int owner, int group);
	int  (*utime)  	       (vfs *me, char *path, struct utimbuf *times);
			       
	int  (*readlink)       (vfs *me, char *path, char *buf, int size);
	int  (*symlink)        (vfs *me, char *n1, char *n2);
	int  (*link)           (vfs *me, char *p1, char *p2);
	int  (*unlink)         (vfs *me, char *path);
	int  (*rename)         (vfs *me, char *p1, char *p2);
	int  (*chdir)          (vfs *me, char *path);
	int  (*ferrno)         (vfs *me);
	int  (*lseek)          (void *vfs_info, off_t offset, int whence);
	int  (*mknod)          (vfs *me, char *path, int mode, int dev);
	
	vfsid (*getid)         (vfs *me, char *path, struct vfs_stamping **
				parent);
	    
	int  (*nothingisopen)  (vfsid id);
	void (*free)           (vfsid id);
	
	char *(*getlocalcopy)  (vfs *me, char *filename);
	void (*ungetlocalcopy) (vfs *me, char *filename, char *local,
				int has_changed);

	int  (*mkdir)          (vfs *me, char *path, mode_t mode);
	int  (*rmdir)          (vfs *me, char *path);
	
	int  (*ctl)            (void *vfs_info, int ctlop, int arg);
	int  (*setctl)         (vfs *me, char *path, int ctlop, char *arg);
#ifdef HAVE_MMAP
	caddr_t (*mmap)        (vfs *me, caddr_t addr, size_t len, int prot,
				int flags, void *vfs_info, off_t offset);
	int (*munmap)          (vfs *me, caddr_t addr, size_t len,
				void *vfs_info);
#endif	
    };

    /* Other file systems */
    extern vfs vfs_local_ops;
    extern vfs vfs_tarfs_ops;

    extern vfs vfs_ftpfs_ops;
    extern vfs vfs_fish_ops;
    extern vfs vfs_mcfs_ops;
    
    extern vfs vfs_extfs_ops;
    extern vfs vfs_sfs_ops;

    extern vfs vfs_undelfs_ops;

    struct vfs_stamping {
        vfs *v;
        vfsid id;
        struct vfs_stamping *parent; /* At the moment applies to tarfs only */
        struct vfs_stamping *next;
        struct timeval time;
    };

    void vfs_init (void);
    void vfs_shut (void);

    extern int vfs_type_absolute;
    vfs *vfs_type (char *path);
    vfs *vfs_split (char *path, char **inpath, char **op);
    vfsid vfs_ncs_getid (vfs *nvfs, char *dir, struct vfs_stamping **par);
    void vfs_rm_parents (struct vfs_stamping *stamp);
    char *vfs_path (char *path);
    char *vfs_strip_suffix_from_filename (char *filename);
    char *vfs_canon (char *path);
    char *mc_get_current_wd (char *buffer, int bufsize);
    int vfs_current_is_local (void);
#if 0
    int vfs_current_is_extfs (void);
    int vfs_current_is_tarfs (void);
#endif
    int vfs_file_is_local (char *name);
    int vfs_file_is_ftp (char *filename);
    char *vfs_get_current_dir (void);
    
    void vfs_stamp (vfs *, vfsid);
    void vfs_rmstamp (vfs *, vfsid, int);
    void vfs_addstamp (vfs *, vfsid, struct vfs_stamping *);
    void vfs_add_noncurrent_stamps (vfs *, vfsid, struct vfs_stamping *);
    void vfs_add_current_stamps (void);
    void vfs_free_resources(char *path);
    void vfs_timeout_handler ();
    int vfs_timeouts ();

    void vfs_fill_names (void (*)(char *));
    char *vfs_translate_url (char *);

    void ftpfs_set_debug (char *file);
#ifdef USE_NETCODE
    void ftpfs_hint_reread(int reread);
    void ftpfs_flushdir(void);
#else
#   define ftpfs_flushdir()
#   define ftpfs_hint_reread(x) 
#endif
    
    /* Only the routines outside of the VFS module need the emulation macros */

	int   mc_open  (char *file, int flags, ...);
	int   mc_close (int handle);
	int   mc_read  (int handle, char *buffer, int count);
	int   mc_write (int hanlde, char *buffer, int count);
	off_t mc_lseek (int fd, off_t offset, int whence);
	int   mc_chdir (char *);

	DIR  *mc_opendir (char *dirname);
	struct dirent *mc_readdir(DIR *dirp);
	int mc_closedir (DIR *dir);
 	int mc_telldir (DIR *dir);
 	void mc_seekdir (DIR *dir, int offset);

	int mc_stat  (char *path, struct stat *buf);
	int mc_lstat (char *path, struct stat *buf);
	int mc_fstat (int fd, struct stat *buf);

	int mc_chmod    (char *path, int mode);
	int mc_chown    (char *path, int owner, int group);
	int mc_utime    (char *path, struct utimbuf *times);
	int mc_readlink (char *path, char *buf, int bufsiz);
	int mc_unlink   (char *path);
	int mc_symlink  (char *name1, char *name2);
        int mc_link     (char *name1, char *name2);
        int mc_mknod    (char *, int, int);
	int mc_rename   (char *original, char *target);
	int mc_write    (int fd, char *buf, int nbyte);
        int mc_rmdir    (char *path);
        int mc_mkdir    (char *path, mode_t mode);

        char *mc_getlocalcopy (char *filename);
        void mc_ungetlocalcopy (char *filename, char *local, int has_changed);
        char *mc_def_getlocalcopy (vfs *vfs, char *filename);
        void mc_def_ungetlocalcopy (vfs *vfs, char *filename, char *local, int has_changed);
        int mc_ctl (int fd, int ctlop, int arg);
        int mc_setctl (char *path, int ctlop, char *arg);
#ifdef HAVE_MMAP
	    caddr_t mc_mmap (caddr_t, size_t, int, int, int, off_t);
	    int mc_unmap (caddr_t, size_t);
            int mc_munmap (caddr_t addr, size_t len);
#endif /* HAVE_MMAP */

#else

#ifdef USE_NETCODE
#    undef USE_NETCODE
#endif

#    undef USE_NETCODE

#   define vfs_fill_names(x)
#   define vfs_add_current_stamps()
#   define vfs_current_is_local() 1
#   define vfs_file_is_local(x) 1
#   define vfs_file_is_ftp(x) 0
#   define vfs_current_is_tarfs() 0
#   define vfs_current_is_extfs() 0
#   define vfs_path(x) x
#   define vfs_strip_suffix_from_filename (x) strdup(x)
#   define mc_close close
#   define mc_read read
#   define mc_write write
#   define mc_lseek lseek
#   define mc_opendir opendir
#   define mc_readdir readdir
#   define mc_closedir closedir
#   define mc_telldir telldir
#   define mc_seekdir seekdir

#   define mc_get_current_wd(x,size) get_current_wd (x, size)
#   define mc_fstat fstat
#   define mc_lstat lstat

#   define mc_readlink readlink
#   define mc_symlink symlink
#   define mc_rename rename

#ifndef __os2__
#   define mc_open open
#   define mc_utime utime
#   define mc_chmod chmod
#   define mc_chown chown
#   define mc_chdir chdir
#   define mc_unlink unlink
#endif

#   define mc_mmap mmap
#   define mc_munmap munmap

#   define mc_ctl(a,b,c) 0
#   define mc_setctl(a,b,c)

#   define mc_stat stat
#   define mc_mknod mknod
#   define mc_link link
#   define mc_mkdir mkdir
#   define mc_rmdir rmdir
#   define is_special_prefix(x) 0
#   define vfs_type(x) (vfs *)(NULL)
#   define vfs_init()
#   define vfs_shut()
#   define vfs_canon(p) strdup (canonicalize_pathname(p))
#   define vfs_free_resources()
#   define vfs_timeout_handler()
#   define vfs_timeouts() 0
#   define vfs_force_expire()

    typedef int vfs;
    
#   define mc_getlocalcopy(x) NULL
#   define mc_ungetlocalcopy(x,y,z)

#   define ftpfs_hint_reread(x) 
#   define ftpfs_flushdir()

#ifdef _OS_NT
#   undef mc_rmdir
#endif

#ifdef OS2_NT
#   undef mc_ctl
#   undef mc_unlink
#   define mc_ctl(a,b,c) 0
#   ifndef __EMX__
#      undef mc_mkdir
#      define mc_mkdir(a,b) mkdir(a)
#   endif
#endif

#endif /* USE_VFS */

#define mc_errno errno

/* These functions are meant for use by vfs modules */

extern int vfs_parse_ls_lga (char *p, struct stat *s, char **filename, char **linkname);
extern int vfs_split_text (char *p);
extern int vfs_parse_filetype (char c);
extern int vfs_parse_filemode (char *p);
extern int vfs_parse_filedate(int idx, time_t *t);

extern void vfs_die (char *msg);
extern char *vfs_get_password (char *msg);
extern char *vfs_split_url (char *path, char **host, char **user, int *port, char **pass,
					int default_port, int flags);
#define URL_DEFAULTANON 1
#define URL_NOSLASH 2
extern void vfs_print_stats (char *fs_name, char *action, char *file_name, int have, int need);

/* Dont use values 0..4 for a while -- 10/98, pavel@ucw.cz */
#define MCCTL_REMOVELOCALCOPY   5
#define MCCTL_IS_NOTREADY	6
#define MCCTL_FORGET_ABOUT	7
#define MCCTL_EXTFS_RUN		8
/* These two make vfs layer give out potentially incorrect data, but
   they also make some operation 100 times faster. Use with caution. */
#define MCCTL_WANT_STALE_DATA	9
#define MCCTL_NO_STALE_DATA    10

extern int vfs_flags;
extern uid_t vfs_uid;
extern gid_t vfs_gid;

#define FL_ALWAYS_MAGIC 1
#define FL_NO_MCFS 2
#define FL_NO_FTPFS 4
#define FL_NO_UNDELFS 8
#define FL_NO_TARFS 16
#define FL_NO_EXTFS 32
#define FL_NO_SFS 64
#define FL_NO_FISH 128

#define FL_NO_CWDSETUP	0x40000000

#ifdef VFS_STANDALONE
extern void mc_vfs_init( void );
extern void mc_vfs_done( void );
#endif

#define O_ALL (O_CREAT | O_EXCL | O_NOCTTY | O_NDELAY | O_SYNC | O_WRONLY | O_RDWR | O_RDONLY)
/* Midnight commander code should _not_ use other flags than those
   listed above and O_APPEND */

#if (O_ALL & O_APPEND)
#warning Unexpected problem with flags, O_LINEAR disabled, contact pavel@ucw.cz
#define O_LINEAR 0
#define IS_LINEAR(a) 0
#define NO_LINEAR(a) a
#else
#define O_LINEAR O_APPEND
#define IS_LINEAR(a) ((a) == (O_RDONLY | O_LINEAR))	/* Return only 0 and 1 ! */
#define NO_LINEAR(a) (((a) == (O_RDONLY | O_LINEAR)) ? O_RDONLY : (a))
#endif

/* O_LINEAR is strange beast, be carefull. If you open file asserting
 * O_RDONLY | O_LINEAR, you promise:
 *
 *     	a) to read file linearily from beggining to the end
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

#define VFS_MIN(a,b) ((a)<(b) ? (a) : (b))
#define VFS_MAX(a,b) ((a)<(b) ? (b) : (a))

#ifdef HAVE_MMAP
#define MMAPNULL , NULL, NULL
#else
#define MMAPNULL
#endif

#define DIR_SEP_CHAR '/'

/* And now some defines for our errors. */

#ifdef ENOSYS
#define E_NOTSUPP ENOSYS	/* for use in vfs when module does not provide function */
#else
#define E_NOTSUPP EFAULT	/* Does this happen? */
#endif

#ifdef ENOMSG
#define E_UNKNOWN ENOMSG	/* if we do not know what error happened */
#else
#define E_UNKNOWN EIO	/* if we do not know what error happened */
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

#endif /* __VFS_H */
