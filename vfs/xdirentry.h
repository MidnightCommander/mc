#ifndef DIRENTRY_H
#define DIRENTRY_H

#include "vfs.h"


#define LINK_FOLLOW 15
#define LINK_NO_FOLLOW -1

/* For vfs_s_find_entry */
#define FL_NONE 0
#define FL_MKDIR 1
#define FL_MKFILE 2
#define FL_DIR 4

/* For open_super */
#define FL_NO_OPEN 1

/* For vfs_s_entry_from_path */
#define FL_FOLLOW 1
#define FL_DIR 4


/* Single connection or archive */
struct vfs_s_super {
    struct vfs_s_super **prevp, *next;
    struct vfs_class *me;
    struct vfs_s_inode *root;
    char *name;			/* My name, whatever it means */
    int fd_usage;		/* Number of open files */
    int ino_usage;		/* Usage count of this superblock */
    int want_stale;		/* If set, we do not flush cache properly */

    union {
	struct {
	    int sockr, sockw;
	    char *cwdir;
	    char *host, *user;
	    char *password;
	    int flags;
	} fish;
	struct {
	    int sock;
	    char *cwdir;
	    char *host, *user;
	    char *password;
	    int port;

	    char *proxy;		/* proxy server, NULL if no proxy */
	    int failed_on_login;	/* used to pass the failure reason to upper levels */
	    int use_source_route;
	    int use_passive_connection;
	    int remote_is_amiga;	/* No leading slash allowed for AmiTCP (Amiga) */
	    int isbinary;
	    int cwd_deferred;	/* current_directory was changed but CWD command hasn't
				   been sent yet */
	    int strict;		/* ftp server doesn't understand 
				   "LIST -la <path>"; use "CWD <path>"/
				   "LIST" instead */
	    int ctl_connection_busy;
	} ftp;
	struct {
	    int fd;
	    struct stat st;
	    int type;		/* Type of the archive */
	    struct defer_inode *deferred;	/* List of inodes for which another entries may appear */
	} arch;
    } u;
};

/*
 * Single virtual file - directory entry.  The same inode can have many
 * entries (i.e. hard links), but usually has only one.
 */
struct vfs_s_entry {
    struct vfs_s_entry **prevp, *next;	/* Pointers in the entry list */
    struct vfs_s_inode *dir;	/* Directory we are in, i.e. our parent */
    char *name;			/* Name of this entry */
    struct vfs_s_inode *ino;	/* ... and its inode */
};

/* Single virtual file - inode */
struct vfs_s_inode {
    struct vfs_s_super *super;	/* Archive the file is on */
    struct vfs_s_entry *ent;	/* Our entry in the parent directory -
				   use only for directories because they
				   cannot be hardlinked */
    struct vfs_s_entry *subdir; /* If this is a directory, its entry */
    struct stat st;		/* Parameters of this inode */
    char *linkname;		/* Symlink's contents */
    char *localname;		/* Filename of local file, if we have one */
    struct timeval timestamp;	/* Subclass specific */
    long data_offset;		/* Subclass specific */
};

/* Data associated with an open file */
struct vfs_s_fh {
    struct vfs_s_inode *ino;
    long pos;			/* This is for module's use */
    int handle;			/* This is for module's use, but if != -1, will be mc_close()d */
    int changed;		/* Did this file change? */
    int linear;			/* Is that file open with O_LINEAR? */
    union {
	struct {
	    int got, total, append;
	} fish;
	struct {
	    int sock, append;
	} ftp;
    } u;
};

/*
 * One of our subclasses (tar, cpio, fish, ftpfs) with data and methods.
 * Extends vfs_class.  Stored in the "data" field of vfs_class.
 */
struct vfs_s_subclass {
    struct vfs_s_super *supers;
    int inode_counter;
    dev_t rdev;
    FILE *logfile;
    int flush;		/* if set to 1, invalidate directory cache */

    int (*init_inode) (struct vfs_class *me, struct vfs_s_inode *ino);	/* optional */
    void (*free_inode) (struct vfs_class *me, struct vfs_s_inode *ino);	/* optional */
    int (*init_entry) (struct vfs_class *me, struct vfs_s_entry *entry);	/* optional */

    void *(*archive_check) (struct vfs_class *me, char *name, char *op);	/* optional */
    int (*archive_same) (struct vfs_class *me, struct vfs_s_super *psup,
			 char *archive_name, char *op, void *cookie);
    int (*open_archive) (struct vfs_class *me, struct vfs_s_super *psup,
			 char *archive_name, char *op);
    void (*free_archive) (struct vfs_class *me,
			  struct vfs_s_super *psup);

    int (*fh_open) (struct vfs_class *me, struct vfs_s_fh *fh, int flags,
		    int mode);
    int (*fh_close) (struct vfs_class *me, struct vfs_s_fh *fh);

    struct vfs_s_entry *(*find_entry) (struct vfs_class *me,
				       struct vfs_s_inode *root,
				       char *path, int follow, int flags);
    int (*dir_load) (struct vfs_class *me, struct vfs_s_inode *ino,
		     char *path);
    int (*dir_uptodate) (struct vfs_class *me, struct vfs_s_inode *ino);
    int (*file_store) (struct vfs_class *me, struct vfs_s_fh *fh,
		       char *path, char *localname);

    int (*linear_start) (struct vfs_class *me, struct vfs_s_fh *fh,
			 int from);
    int (*linear_read) (struct vfs_class *me, struct vfs_s_fh *fh,
			void *buf, int len);
    void (*linear_close) (struct vfs_class *me, struct vfs_s_fh *fh);
};


/* entries and inodes */
struct vfs_s_inode *vfs_s_new_inode (struct vfs_class *me,
				     struct vfs_s_super *super,
				     struct stat *initstat);
struct vfs_s_entry *vfs_s_new_entry (struct vfs_class *me, char *name,
				     struct vfs_s_inode *inode);
void vfs_s_free_entry (struct vfs_class *me, struct vfs_s_entry *ent);
void vfs_s_insert_entry (struct vfs_class *me, struct vfs_s_inode *dir,
			 struct vfs_s_entry *ent);
struct stat *vfs_s_default_stat (struct vfs_class *me, mode_t mode);

void vfs_s_add_dots (struct vfs_class *me, struct vfs_s_inode *dir,
		     struct vfs_s_inode *parent);
struct vfs_s_entry *vfs_s_generate_entry (struct vfs_class *me, char *name,
					  struct vfs_s_inode *parent,
					  mode_t mode);
struct vfs_s_entry *vfs_s_find_entry_tree (struct vfs_class *me,
					   struct vfs_s_inode *root,
					   char *path, int follow,
					   int flags);
struct vfs_s_entry *vfs_s_find_entry_linear (struct vfs_class *me,
					     struct vfs_s_inode *root,
					     char *path, int follow,
					     int flags);
struct vfs_s_inode *vfs_s_find_inode (struct vfs_class *me,
				      struct vfs_s_inode *root, char *path,
				      int follow, int flags);
struct vfs_s_inode *vfs_s_find_root (struct vfs_class *me,
				     struct vfs_s_entry *entry);

/* outside interface */
void vfs_s_init_class (struct vfs_class *vclass);
char *vfs_s_get_path_mangle (struct vfs_class *me, char *inname,
			     struct vfs_s_super **archive, int flags);
void vfs_s_invalidate (struct vfs_class *me, struct vfs_s_super *super);
char *vfs_s_fullpath (struct vfs_class *me, struct vfs_s_inode *ino);

/* network filesystems support */
int vfs_s_select_on_two (int fd1, int fd2);
int vfs_s_get_line (struct vfs_class *me, int sock, char *buf, int buf_len,
		    char term);
int vfs_s_get_line_interruptible (struct vfs_class *me, char *buffer,
				  int size, int fd);

/* misc */
int vfs_s_retrieve_file (struct vfs_class *me, struct vfs_s_inode *ino);

#define ERRNOR(a, b) do { me->verrno = a; return b; } while (0)

#define MEDATA ((struct vfs_s_subclass *) me->data)

#define FH ((struct vfs_s_fh *) fh)
#define FH_SUPER FH->ino->super

#define LS_NOT_LINEAR 0
#define LS_LINEAR_CLOSED 1
#define LS_LINEAR_OPEN 2

#endif
