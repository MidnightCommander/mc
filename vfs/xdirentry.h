#ifndef DIRENTRY_H
#define DIRENTRY_H

#include <config.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include "../src/fs.h"
#include "../src/util.h"
#include "../src/mem.h"
#include "../src/mad.h"
#include "vfs.h"


#define FOLLOW 15
#define NO_FOLLOW -1

/* For vfs_s_find_entry */
#define FL_NONE 0
#define FL_MKDIR 1
#define FL_MKFILE 2

/* For open_super */
#define FL_NO_OPEN 1

/* For vfs_s_entry_from_path */
#define FL_FOLLOW 1
#define FL_DIR 2

typedef struct vfs_s_entry {
    struct vfs_s_entry **prevp, *next;
    struct vfs_s_inode *dir;	/* Directory we are in - FIXME: is this needed? */
    char *name;			/* Name of this entry */
    struct vfs_s_inode *ino;	/* ... and its inode */
} vfs_s_entry;

typedef struct vfs_s_inode {
    struct vfs_s_entry *subdir;
    struct stat st;		/* Parameters of this inode */
    char *linkname;		/* Symlink's contents */
    char *localname;		/* filename of local file, if we have one */

    union {
	struct {
	    long data_offset;
	} tar;
    } u;
} vfs_s_inode;

typedef struct vfs_s_super {
    struct vfs_s_super **prevp, *next;
    vfs *me;
    vfs_s_inode *root;
    char *name;		/* My name, whatever it means */
    int fd_usage;	/* Usage count of this superblock */

    union {
	struct {
	    int fd;
	    struct stat tarstat;
	} tar;
    } u;
} vfs_s_super;

typedef struct vfs_s_fh {
    struct vfs_s_super *super;	/* Is this field needed? */
    struct vfs_s_inode *ino;
    long pos;			/* This is for module's use */
    int handle;	/* This is for module's use, but if != -1, will be mc_close()d */
} vfs_s_fh;

struct vfs_s_data {
    struct vfs_s_super *supers;
    int inode_counter;
    dev_t rdev;

    int (*init_inode) (vfs *me, vfs_s_inode *ino);
    void (*free_inode) (vfs *me, vfs_s_inode *ino);
    int (*init_entry) (vfs *me, vfs_s_entry *entry);

    void* (*archive_check) (vfs *me, char *name, char *op);
    int (*archive_same) (vfs *me, vfs_s_super *psup, char *archive_name, void *cookie);
    int (*open_archive) (vfs *me, vfs_s_super *psup, char *archive_name, char *op);
    void (*free_archive) (vfs *me, vfs_s_super *psup);

    int (*load_dir) (vfs *me, vfs_s_super *super, char *q);

    int (*fh_open) (vfs *me, vfs_s_fh *fh);
    int (*fh_close) (vfs *me, vfs_s_fh *fh);
};

/* entries and inodes */
vfs_s_inode *vfs_s_new_inode (vfs *me, struct stat *initstat);
vfs_s_entry *vfs_s_new_entry (vfs *me, char *name, vfs_s_inode *inode);
void vfs_s_free_entry (vfs *me, vfs_s_entry *ent);
void vfs_s_free_tree (vfs *me, vfs_s_inode *dir, vfs_s_inode *parentdir);
void vfs_s_insert_entry (vfs *me, vfs_s_inode *dir, vfs_s_entry *ent);
struct stat *vfs_s_default_stat (vfs *me, mode_t mode);
void vfs_s_add_dots (vfs *me, vfs_s_inode *dir, vfs_s_inode *parent);
struct vfs_s_entry *vfs_s_generate_entry (vfs *me, char *name, struct vfs_s_inode *parent, mode_t mode);
vfs_s_entry *vfs_s_automake(vfs *me, vfs_s_inode *dir, char *path, int flags);
vfs_s_entry *vfs_s_find_entry(vfs *me, vfs_s_inode *root, char *path, int follow, int flags);
vfs_s_inode *vfs_s_find_inode(vfs *me, vfs_s_inode *root, char *path, int follow, int flags);
vfs_s_inode *vfs_s_find_root(vfs *me, vfs_s_entry *entry);
struct vfs_s_entry *vfs_s_resolve_symlink (vfs *me, struct vfs_s_entry *entry, int follow);
/* superblock games */
vfs_s_super *vfs_s_new_super (vfs *me);
void vfs_s_free_super (vfs *me, vfs_s_super *super); 
/* outside interface */
char *vfs_s_get_path_mangle (vfs *me, char *inname, struct vfs_s_super **archive, int flags);
char *vfs_s_get_path (vfs *me, char *inname, struct vfs_s_super **archive, int flags);
/* readdir & friends */
vfs_s_inode *vfs_s_inode_from_path (vfs *me, char *name, int flags);
void * vfs_s_opendir (vfs *me, char *dirname);
void * vfs_s_readdir (void *data);
int vfs_s_telldir (void *data);
void vfs_s_seekdir (void *data, int offset);
int vfs_s_closedir (void *data);
int vfs_s_chdir (vfs *me, char *path);
/* stat & friends */
int vfs_s_stat (vfs *me, char *path, struct stat *buf);
int vfs_s_lstat (vfs *me, char *path, struct stat *buf);
int vfs_s_fstat (void *fh, struct stat *buf);
int vfs_s_readlink (vfs *me, char *path, char *buf, int size);
void *vfs_s_open (vfs *me, char *file, int flags, int mode);
int vfs_s_lseek (void *fh, off_t offset, int whence);
int vfs_s_close (void *fh);
/* mc support */
void vfs_s_fill_names (vfs *me, void (*func)(char *));
int vfs_s_ferrno(vfs *me);
void vfs_s_dump(vfs *me, char *prefix, vfs_s_inode *ino);
char *vfs_s_getlocalcopy (vfs *me, char *path);
/* stamping support */
vfsid vfs_s_getid (vfs *me, char *path, struct vfs_stamping **parent);
int vfs_s_nothingisopen (vfsid id);
void vfs_s_free (vfsid id);

/* If non-null, FREE */
#define ifree(ptr) do { if (ptr) free(ptr); } while (0)

#define MEDATA ((struct vfs_s_data *) me->data)
#define ERRNOR(a, b) do { me->verrno = a; return b; } while (0)
#define FH ((struct vfs_s_fh *) fh)

#endif
