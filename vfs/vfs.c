/* Virtual File System switch code
   Copyright (C) 1995 The Free Software Foundation
   
   Written by: 1995 Miguel de Icaza
               1995 Jakub Jelinek
	       1998 Pavel Machek
   
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* Warning: funtions like extfs_lstat() have right to destroy any
 * strings you pass to them. This is acutally ok as you strdup what
 * you are passing to them, anyway; still, beware.  */

/* Namespace: exports *many* functions with vfs_ prefix; exports
   parse_ls_lga and friends which do not have that prefix. */

#include <config.h>
#include <glib.h>
#undef MIN
#undef MAX

#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>	/* For atol() */
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <malloc.h>
#include <fcntl.h>
#include <signal.h>
#ifdef SCO_FLAVOR
#include <sys/timeb.h>	/* alex: for struct timeb definition */
#endif /* SCO_FLAVOR */
#include <time.h>
#include <sys/time.h>
#include "../src/fs.h"
#include "../src/mad.h"
#include "../src/dir.h"
#include "../src/util.h"
#include "../src/main.h"
#ifndef VFS_STANDALONE
#include "../src/panel.h"
#include "../src/key.h"		/* Required for the async alarm handler */
#include "../src/layout.h"	/* For get_panel_widget and get_other_index */
#include "../src/dialog.h"
#endif
#include "vfs.h"
#include "extfs.h"		/* FIXME: we should not know anything about our modules */
#include "names.h"
#ifdef USE_NETCODE
#   include "tcputil.h"
#endif

extern int get_other_type (void);

int vfs_timeout = 60; /* VFS timeout in seconds */
int vfs_flags = 0;    /* Flags */

extern int cd_symlinks; /* Defined in main.c */

/* They keep track of the current directory */
static vfs *current_vfs = &vfs_local_ops;
static char *current_dir = NULL;

/*
 * FIXME: this is broken. It depends on mc not crossing border on month!
 */
static int current_mday;
static int current_mon;
static int current_year;

uid_t vfs_uid = 0;
gid_t vfs_gid = 0;

/* FIXME: Open files managed by the vfs layer, should be dynamical */
#define MAX_VFS_FILES 100

static struct {
    void *fs_info;
    vfs  *operations;
} vfs_file_table [MAX_VFS_FILES];

static int
get_bucket (void)
{
    int i;

    /* 0, 1, 2 are reserved file descriptors, while (DIR *) 0 means error */
    for (i = 3; i < MAX_VFS_FILES; i++){
	if (!vfs_file_table [i].fs_info)
	    return i;
    }

    vfs_die ("No more virtual file handles");
    return 0;
}

/* vfs_local_ops needs to be the first one */
static vfs *vfs_list = &vfs_local_ops;

int
vfs_register (vfs *vfs)
{
    int res;

    if (!vfs) vfs_die("You can not register NULL.");
    res = (vfs->init) ? (*vfs->init)(vfs) : 1;

    if (!res) return 0;

    vfs->next = vfs_list;
    vfs_list = vfs;

    return 1;
}

vfs *
vfs_type_from_op (char *path)
{
    vfs *vfs;

    if (!path) vfs_die( "vfs_type_from_op got NULL: impossible" );
    
    for (vfs = vfs_list; vfs != &vfs_local_ops; vfs = vfs->next){
        if (vfs->which) {
	    if ((*vfs->which) (vfs, path) == -1)
		continue;
	    return vfs;
	}
	if (!strncmp (path, vfs->prefix, strlen (vfs->prefix)))
	    return vfs;
    }
    return NULL; /* shut up stupid gcc */
}

/* Strip known vfs suffixes from a filename (possible improvement: strip
   suffix from last path component). 
   Returns a malloced string which has to be freed. */
char *
vfs_strip_suffix_from_filename (char *filename)
{
    vfs *vfs;
    char *semi;
    char *p;

    if (!filename) vfs_die( "vfs_strip_suffix_from_path got NULL: impossible" );
    
    p = strdup (filename);
    if (!(semi = strrchr (p, '#')))
	return p;

    for (vfs = vfs_list; vfs != &vfs_local_ops; vfs = vfs->next){
        if (vfs->which) {
	    if ((*vfs->which) (vfs, semi + 1) == -1)
		continue;
	    *semi = '\0'; /* Found valid suffix */
	    return p;
	}
	if (!strncmp (semi + 1, vfs->prefix, strlen (vfs->prefix))) {
	    *semi = '\0'; /* Found valid suffix */
	    return p;
        }
    }
    return p;
}

static int
path_magic (char *path)
{
    struct stat buf;

    if (vfs_flags & FL_ALWAYS_MAGIC)
        return 1;

    if (!stat(path, &buf))
        return 0;

    return 1;
}

/*
 * Splits path '/p1#op/inpath' into inpath,op; returns which vfs it is.
 * What is left in path is p1. You still want to free(path), you DON'T
 * want to free neither *inpath nor *op
 */
vfs *
vfs_split (char *path, char **inpath, char **op)
{
    char *semi;
    char *slash;
    vfs *ret;

    if (!path) vfs_die("Can not split NULL");
    
    semi = strrchr (path, '#');
    if (!semi || !path_magic(path))
	return NULL;

    slash = strchr (semi, '/');
    *semi = 0;

    if (op)
	*op = NULL;

    if (inpath)
	*inpath = NULL;

    if (slash)
	*slash = 0;

    if ((ret = vfs_type_from_op (semi+1))){
	if (op) 
	    *op = semi + 1;
	if (inpath)
	    *inpath = slash ? slash + 1 : NULL;
	return ret;
    }

    if (slash)
	*slash = '/';
    ret = vfs_split (path, inpath, op);
    *semi = '#';
    return ret;
}

vfs *
vfs_rosplit (char *path)
{
    char *semi;
    char *slash;
    vfs *ret;

    if (!path) vfs_die( "Can not rosplit NULL" );
    semi = strrchr (path, '#');
    
    if (!semi || !path_magic (path))
	return NULL;
    
    slash = strchr (semi, '/');
    *semi = 0;
    if (slash)
	*slash = 0;
    
    ret = vfs_type_from_op (semi+1);

    if (slash)
	*slash = '/';
    if (!ret)
	ret = vfs_rosplit (path);

    *semi = '#';
    return ret;
}

vfs *
vfs_type (char *path)
{
    vfs *vfs;

    vfs = vfs_rosplit(path);

    if (!vfs)
        vfs = &vfs_local_ops;

    return vfs;
}

static struct vfs_stamping *stamps;

/*
 * Returns the number of seconds remaining to the vfs timeout
 *
 * FIXME: currently this is set to 10 seconds.  We should compute this.
 */
int
vfs_timeouts ()
{
    return stamps ? 10 : 0;
}

void
vfs_addstamp (vfs *v, vfsid id, struct vfs_stamping *parent)
{
    if (v != &vfs_local_ops && id != (vfsid)-1){
        struct vfs_stamping *stamp, *st1;
        
        for (stamp = stamps; stamp != NULL; st1 = stamp, stamp = stamp->next)
            if (stamp->v == v && stamp->id == id){
		gettimeofday(&(stamp->time), NULL);
                return;
	    }
        stamp = xmalloc (sizeof (struct vfs_stamping), "vfs stamping");
        stamp->v = v;
        stamp->id = id;
	if (parent){
	    struct vfs_stamping *st = stamp;
	    for (; parent;){
		st->parent = xmalloc (sizeof (struct vfs_stamping), "vfs stamping");
		*st->parent = *parent;
		parent = parent->parent;
		st = st->parent;
	    }
	    st->parent = 0;
	}
	else
    	    stamp->parent = 0;
	
        gettimeofday (&(stamp->time), NULL);
        stamp->next = 0;

	if (stamps)
	    st1->next = stamp;
	else
    	    stamps = stamp;
    }
}

void
vfs_stamp (vfs *v, vfsid id)
{
    struct vfs_stamping *stamp;

    for (stamp = stamps; stamp != NULL; stamp = stamp->next)
        if (stamp->v == v && stamp->id == id){

            gettimeofday (&(stamp->time), NULL);
            if (stamp->parent != NULL)
                vfs_stamp (stamp->parent->v, stamp->parent->id);

            return;
        }
}

void
vfs_rm_parents (struct vfs_stamping *stamp)
{
    struct vfs_stamping *st2, *st3;

    if (stamp){
	for (st2 = stamp, st3 = st2->parent; st3 != NULL; st2 = st3, st3 = st3->parent)
	    free (st2);

	free (st2);
    }
}

void
vfs_rmstamp (vfs *v, vfsid id, int removeparents)
{
    struct vfs_stamping *stamp, *st1;
    
    for (stamp = stamps, st1 = NULL; stamp != NULL; st1 = stamp, stamp = stamp->next)
        if (stamp->v == v && stamp->id == id){
            if (stamp->parent != NULL){
                if (removeparents)
                    vfs_rmstamp (stamp->parent->v, stamp->parent->id, 1);
		vfs_rm_parents (stamp->parent);
            }
            if (st1 == NULL){
                stamps = stamp->next;
            } else {
            	st1->next = stamp->next;
            }
            free (stamp);

            return;
        }
}

static int
ferrno (vfs *vfs)
{
    return vfs->ferrno ? (*vfs->ferrno)(vfs) : E_UNKNOWN; 
    /* Hope that error message is obscure enough ;-) */
}

int
mc_open (char *file, int flags, ...)
{
    int  handle;
    int  mode;
    void *info;
    vfs  *vfs;
    va_list ap;
    
    file = vfs_canon (file);
    vfs = vfs_type (file);

    /* Get the mode flag */	/* FIXME: should look if O_CREAT is present */
    va_start (ap, flags);
    mode = va_arg (ap, int);
    va_end (ap);
    
    if (!vfs->open)
	vfs_die ("VFS must support open.\n");

    info = (*vfs->open) (vfs, file, flags, mode);	/* open must be supported */
    free (file);
    if (!info){
	errno = ferrno (vfs);
	return -1;
    }
    handle = get_bucket ();
    vfs_file_table [handle].fs_info = info;
    vfs_file_table [handle].operations = vfs;
    
    return handle;
}

#define vfs_op(handle) vfs_file_table [handle].operations 
#define vfs_info(handle) vfs_file_table [handle].fs_info
#define vfs_free_bucket(handle) vfs_info(handle) = 0;

#define MC_OP(name, inarg, callarg, pre, post) \
        int mc_##name inarg \
	{ \
	vfs *vfs; \
	int result; \
	\
	pre \
	result = vfs->name ? (*vfs->name)callarg : -1; \
        post \
	if (result == -1) \
	    errno = vfs->name ? ferrno (vfs) : E_NOTSUPP; \
	return result; \
	}

#define MC_NAMEOP(name, inarg, callarg) \
  MC_OP (name, inarg, callarg, path = vfs_canon (path); vfs = vfs_type (path);, free (path); )
#define MC_HANDLEOP(name, inarg, callarg) \
  MC_OP (name, inarg, callarg, if (handle == -1) return -1; vfs = vfs_op (handle);, )

MC_HANDLEOP(read, (int handle, char *buffer, int count), (vfs_info (handle), buffer, count) );

int
mc_ctl (int handle, int ctlop, int arg)
{
    vfs *vfs;
    int result;

    vfs = vfs_op (handle);
    result = vfs->ctl ? (*vfs->ctl)(vfs_info (handle), ctlop, arg) : 0;
    return result;
}

int
mc_setctl (char *path, int ctlop, char *arg)
{
    vfs *vfs;
    int result;

    if (!path)
	vfs_die( "You don't want to pass NULL to mc_setctl." );
    path = vfs_canon (path);
    vfs = vfs_type (path);    
    result = vfs->setctl ? (*vfs->setctl)(vfs, path, ctlop, arg) : 0;
    free (path);
    return result;
}

int
mc_close (int handle)
{
    vfs *vfs;
    int result;

    if (handle == -1 || !vfs_info (handle))
	return -1;
    
    vfs = vfs_op (handle);
    if (handle < 3)
	return close (handle);

    if (!vfs->close)
        vfs_die ("VFS must support close.\n");
    result = (*vfs->close)(vfs_info (handle));
    vfs_free_bucket (handle);
    if (result == -1)
	errno = ferrno (vfs);
    
    return result;
}

DIR *
mc_opendir (char *dirname)
{
    int  handle, *handlep;
    void *info;
    vfs  *vfs;
    char *p = NULL;
    int i = strlen (dirname);

    if (dirname [i - 1] != '/'){ 
    /* We should make possible reading of the root directory in a tar file */
        p = xmalloc (i + 2, "slash");
        strcpy (p, dirname);
        strcpy (p + i, "/");
        dirname = p;
    }
    dirname = vfs_canon (dirname);
    vfs = vfs_type (dirname);

    info = vfs->opendir ? (*vfs->opendir)(vfs, dirname) : NULL;
    free (dirname);
    if (p)
        free (p);
    if (!info){
        errno = vfs->opendir ? ferrno (vfs) : E_NOTSUPP;
	return NULL;
    }
    handle = get_bucket ();
    vfs_file_table [handle].fs_info = info;
    vfs_file_table [handle].operations = vfs;

    handlep = (int *) xmalloc (sizeof (int), "opendir handle");
    *handlep = handle;
    return (DIR *) handlep;
}

/* This should strip the non needed part of a path name */
#define vfs_name(x) x

void
mc_seekdir (DIR *dirp, int offset)
{
    int handle;
    vfs *vfs;

    if (!dirp){
	errno = EFAULT;
	return;
    }
    handle = *(int *) dirp;
    vfs = vfs_op (handle);
    if (vfs->seekdir)
        (*vfs->seekdir) (vfs_info (handle), offset);
    else
        errno = E_NOTSUPP;
}

#define MC_DIROP(name, type, onerr ) \
type mc_##name (DIR *dirp) \
{ \
    int handle; \
    vfs *vfs; \
    type result; \
\
    if (!dirp){ \
	errno = EFAULT; \
	return onerr; \
    } \
    handle = *(int *) dirp; \
    vfs = vfs_op (handle); \
    result = vfs->name ? (*vfs->name) (vfs_info (handle)) : onerr; \
    if (result == onerr) \
        errno = vfs->name ? ferrno(vfs) : E_NOTSUPP; \
    return result; \
}

MC_DIROP (readdir, struct dirent *, NULL)
MC_DIROP (telldir, int, -1)

int
mc_closedir (DIR *dirp)
{
    int handle = *(int *) dirp;
    vfs *vfs = vfs_op (handle);
    int result;

    result = vfs->closedir ? (*vfs->closedir)(vfs_info (handle)) : -1;
    vfs_free_bucket (handle);
    free (dirp);
    return result; 
}

MC_NAMEOP   (stat, (char *path, struct stat *buf), (vfs, vfs_name (path), buf))
MC_NAMEOP   (lstat, (char *path, struct stat *buf), (vfs, vfs_name (path), buf))
MC_HANDLEOP (fstat, (int handle, struct stat *buf), (vfs_info (handle), buf))

/*
 * You must strdup whatever this function returns, static buffers are in use
 */

char *
mc_return_cwd (void)
{
    char *p;
    struct stat my_stat, my_stat2;

    if (!vfs_rosplit (current_dir)){
	static char buffer[MC_MAXPATHLEN];

	p = get_current_wd (buffer, MC_MAXPATHLEN);
	if (!p)  /* One of the directories in the path is not readable */
	    return current_dir;

	/* Otherwise check if it is O.K. to use the current_dir */
	mc_stat (p, &my_stat);
	mc_stat (current_dir, &my_stat2);
	if (my_stat.st_ino != my_stat2.st_ino ||
	    my_stat.st_dev != my_stat2.st_dev ||
	    !cd_symlinks){
	    free (current_dir);
	    current_dir = strdup (p);
	    return p;
	} /* Otherwise we return current_dir below */
    } 
    return current_dir;
}

char *
mc_get_current_wd (char *buffer, int size)
{
    char *cwd = mc_return_cwd();
    
    if (strlen (cwd) > size)
      vfs_die ("Current_dir size overflow.\n");

    strcpy (buffer, cwd);
    return buffer;
}

MC_NAMEOP (chmod, (char *path, int mode), (vfs, vfs_name (path), mode))
MC_NAMEOP (chown, (char *path, int owner, int group), (vfs, vfs_name (path), owner, group))
MC_NAMEOP (utime, (char *path, struct utimbuf *times), (vfs, vfs_name (path), times))
MC_NAMEOP (readlink, (char *path, char *buf, int bufsiz), (vfs, vfs_name (path), buf, bufsiz))
MC_NAMEOP (unlink, (char *path), (vfs, vfs_name (path)))
MC_NAMEOP (symlink, (char *name1, char *path), (vfs, vfs_name (name1), vfs_name (path)))

#define MC_RENAMEOP(name) \
int mc_##name (char *name1, char *name2) \
{ \
    vfs *vfs; \
    int result; \
\
    name1 = vfs_canon (name1); \
    vfs = vfs_type (name1); \
    name2 = vfs_canon (name2); \
    if (vfs != vfs_type (name2)){ \
    	errno = EXDEV; \
    	free (name1); \
    	free (name2); \
	return -1; \
    } \
\
    result = vfs->name ? (*vfs->name)(vfs, vfs_name (name1), vfs_name (name2)) : -1; \
    free (name1); \
    free (name2); \
    if (result == -1) \
        errno = vfs->name ? ferrno (vfs) : E_NOTSUPP; \
    return result; \
}

MC_RENAMEOP (link);
MC_RENAMEOP (rename);

MC_HANDLEOP (write, (int handle, char *buf, int nbyte), (vfs_info (handle), buf, nbyte));

off_t mc_lseek (int fd, off_t offset, int whence)
{
    vfs *vfs;
    int result;

    if (fd == -1)
	return -1;

    vfs = vfs_op (fd);
    result = vfs->lseek ? (*vfs->lseek)(vfs_info (fd), offset, whence) : -1;
    if (result == -1)
        errno = vfs->lseek ? ferrno (vfs) : E_NOTSUPP;
    return result;
}

/*
 * remove //, /./ and /../, local should point to big enough buffer
 */

#define ISSLASH(a) (!a || (a == '/'))

char *
vfs_canon (char *path)
{
    if (!path) vfs_die("Can not canonize NULL");

    /* Tilde expansion */
    if (*path == '~'){ 
    	char *local, *result;

    	local = tilde_expand (path);
	if (local){
	    result = vfs_canon (local);
	    free (local);
	    return result;
	} else 
	    return strdup (path);
    }

    /* Relative to current directory */
    if (*path != '/'){ 
    	char *local, *result;

	if (current_dir [strlen (current_dir) - 1] == '/')
	    local = copy_strings (current_dir, path, NULL);
	else
	    local = copy_strings (current_dir, "/", path, NULL);

	result = vfs_canon (local);
	free (local);
	return result;
    }

    /*
     * So we have path of following form:
     * /p1/p2#op/.././././p3#op/p4. Good luck.
     */
    mad_check( "(pre-canonicalize)", 0);
    canonicalize_pathname (path);
    mad_check( "(post-canonicalize)", 0);

    return strdup (path);
}

vfsid
vfs_ncs_getid (vfs *nvfs, char *dir, struct vfs_stamping **par)
{
    vfsid nvfsid;
    int freeit = 0;

    if (dir [strlen (dir) - 1] != '/'){
        dir = copy_strings (dir, "/", NULL);    
        freeit = 1;

    }
    nvfsid = (*nvfs->getid)(nvfs, dir, par);
    if (freeit)
        free (dir);
    return nvfsid;
}

static int
is_parent (vfs * nvfs, vfsid nvfsid, struct vfs_stamping *parent)
{
    struct vfs_stamping *stamp;

    for (stamp = parent; stamp; stamp = stamp->parent)
	if (stamp->v == nvfs && stamp->id == nvfsid)
	    break;

    return (stamp ? 1 : 0);
}

void
vfs_add_noncurrent_stamps (vfs * oldvfs, vfsid oldvfsid, struct vfs_stamping *parent)
{
#ifndef VFS_STANDALONE
    vfs *nvfs, *n2vfs, *n3vfs;
    vfsid nvfsid, n2vfsid, n3vfsid;
    struct vfs_stamping *par, *stamp;
    int f;

    /* FIXME: As soon as we convert to multiple panels, this stuff
       has to change. It works like this: We do not time out the
       vfs's which are current in any panel and on the other
       side we add the old directory with all its parents which
       are not in any panel (if we find such one, we stop adding
       parents to the time-outing structure. */

    /* There are three directories we have to take care of: current_dir,
       cpanel->cwd and opanel->cwd. Athough most of the time either
       current_dir and cpanel->cwd or current_dir and opanel->cwd are the
       same, it's possible that all three are different -- Norbert */
       
    if (!cpanel)
	return;

    nvfs = vfs_type (current_dir);
    nvfsid = vfs_ncs_getid (nvfs, current_dir, &par);
    vfs_rmstamp (nvfs, nvfsid, 1);

    f = is_parent (oldvfs, oldvfsid, par);
    vfs_rm_parents (par);
    if ((nvfs == oldvfs && nvfsid == oldvfsid) || oldvfsid == (vfsid *)-1 || f){
	return;
    }

    if (get_current_type () == view_listing){
	n2vfs = vfs_type (cpanel->cwd);
	n2vfsid = vfs_ncs_getid (n2vfs, cpanel->cwd, &par);
        f = is_parent (oldvfs, oldvfsid, par);
	vfs_rm_parents (par);
	if ((n2vfs == oldvfs && n2vfsid == oldvfsid) || f) 
	    return;
    } else {
	n2vfs = (vfs *) -1;
	n2vfsid = (vfs *) -1;
    }
    
    if (get_other_type () == view_listing){
	n3vfs = vfs_type (opanel->cwd);
	n3vfsid = vfs_ncs_getid (n3vfs, opanel->cwd, &par);
        f = is_parent (oldvfs, oldvfsid, par);
	vfs_rm_parents (par);
	if ((n3vfs == oldvfs && n3vfsid == oldvfsid) || f)
	    return;
    } else {
	n3vfs = (vfs *)-1;
	n3vfsid = (vfs *)-1;
    }
    
    if ((*oldvfs->nothingisopen) (oldvfsid)){
	if (oldvfs == &vfs_extfs_ops && ((extfs_archive *) oldvfsid)->name == 0){
	    /* Free the resources immediatly when we leave a mtools fs
	       ('cd a:') instead of waiting for the vfs-timeout */
	    (oldvfs->free) (oldvfsid);
	} else
	    vfs_addstamp (oldvfs, oldvfsid, parent);
	for (stamp = parent; stamp != NULL; stamp = stamp->parent){
	    if ((stamp->v == nvfs && stamp->id == nvfsid) ||
		(stamp->v == n2vfs && stamp->id == n2vfsid) ||
		(stamp->v == n3vfs && stamp->id == n3vfsid) ||
		stamp->id == (vfsid) - 1 ||
		!(*stamp->v->nothingisopen) (stamp->id))
		break;
	    if (stamp->v == &vfs_extfs_ops && ((extfs_archive *) stamp->id)->name == 0){
		(stamp->v->free) (stamp->id);
		vfs_rmstamp (stamp->v, stamp->id, 0);
	    } else
		vfs_addstamp (stamp->v, stamp->id, stamp->parent);
	}
    }
#else
    vfs_addstamp (oldvfs, oldvfsid, parent);
#endif
}

static void
vfs_stamp_path (char *path)
{
    vfs *vfs;
    vfsid id;
    struct vfs_stamping *par, *stamp;
    
    vfs = vfs_type (path);
    id  = vfs_ncs_getid (vfs, path, &par);
    vfs_addstamp (vfs, id, par);
    
    for (stamp = par; stamp != NULL; stamp = stamp->parent) 
	vfs_addstamp (stamp->v, stamp->id, stamp->parent);
    vfs_rm_parents (par);
}

#ifndef VFS_STANDALONE
void
vfs_add_current_stamps (void)
{
    vfs_stamp_path (current_dir);
    if (get_current_type () == view_listing)
        vfs_stamp_path (cpanel->cwd);
    if (get_other_type () == view_listing)
	vfs_stamp_path (opanel->cwd);
}
#endif

/* This function is really broken */
int
mc_chdir (char *path)
{
    char *a, *b;
    int result;
    char *p = NULL;
    int i = strlen (path);
    vfs *oldvfs;
    vfsid oldvfsid;
    struct vfs_stamping *parent;

    if (path [i - 1] != '/'){
    /* We should make possible reading of the root directory in a tar archive */
        p = xmalloc (i + 2, "slash");
        strcpy (p, path);
        strcpy (p + i, "/");
        path = p;
    }

    a = current_dir; /* Save a copy for case of failure */
    current_dir = vfs_canon (path);
    current_vfs = vfs_type (current_dir);
    b = strdup (current_dir);
    result = (*current_vfs->chdir) ?  (*current_vfs->chdir)(current_vfs, vfs_name (b)) : -1;
    free (b);
    if (result == -1){
	errno = ferrno (current_vfs);
    	free (current_dir);
    	current_vfs = vfs_type (a);
    	current_dir = a;
    } else {
        oldvfs = vfs_type (a);
	oldvfsid = vfs_ncs_getid (oldvfs, a, &parent);
	free (a);
        vfs_add_noncurrent_stamps (oldvfs, oldvfsid, parent);
	vfs_rm_parents (parent);
    }
    if (p)
        free (p);

    if (*current_dir){
	p = strchr (current_dir, 0) - 1;
	if (*p == '/' && p > current_dir)
	    *p = 0; /* Sometimes we assume no trailing slash on cwd */
    }
    return result;
}

int
vfs_current_is_local (void)
{
    return current_vfs == &vfs_local_ops;
}

#if 0
/* External world should not do differences between VFS-s */
int
vfs_current_is_extfs (void)
{
    return current_vfs == &vfs_extfs_ops;
}

int
vfs_current_is_tarfs (void)
{
    return current_vfs == &vfs_tarfs_ops;
}
#endif

int
vfs_file_is_local (char *filename)
{
    vfs *vfs;
    
    filename = vfs_canon (filename);
    vfs = vfs_type (filename);
    free (filename);
    return vfs == &vfs_local_ops;
}

int
vfs_file_is_ftp (char *filename)
{
#ifdef USE_NETCODE
    vfs *vfs;
    
    filename = vfs_canon (filename);
    vfs = vfs_type (filename);
    free (filename);
    return vfs == &vfs_ftpfs_ops;
#else
    return 0;
#endif
}

char *vfs_get_current_dir (void)
{
    return current_dir;
}

static void vfs_setup_wd (void)
{
    current_dir = strdup ("/");
    if (!(vfs_flags & FL_NO_CWDSETUP))
        mc_return_cwd();

    if (strlen(current_dir)>MC_MAXPATHLEN-2)
        vfs_die ("Current dir too long.\n");

    return;
}

MC_NAMEOP (mkdir, (char *path, mode_t mode), (vfs, vfs_name (path), mode))
MC_NAMEOP (rmdir, (char *path), (vfs, vfs_name (path)))
MC_NAMEOP (mknod, (char *path, int mode, int dev), (vfs, vfs_name (path), mode, dev))

#ifdef HAVE_MMAP
static struct mc_mmapping {
    caddr_t addr;
    void *vfs_info;
    vfs *vfs;
    struct mc_mmapping *next;
} *mc_mmaparray = NULL;

caddr_t
mc_mmap (caddr_t addr, size_t len, int prot, int flags, int fd, off_t offset)
{
    vfs *vfs;
    caddr_t result;
    struct mc_mmapping *mcm;

    if (fd == -1)
	return (caddr_t) -1;
    
    vfs = vfs_op (fd);
    result = vfs->mmap ? (*vfs->mmap)(vfs, addr, len, prot, flags, vfs_info (fd), offset) : (caddr_t)-1;
    if (result == (caddr_t)-1){
	errno = ferrno (vfs);
	return (caddr_t)-1;
    }
    mcm = (struct mc_mmapping *) xmalloc (sizeof (struct mc_mmapping), "vfs: mmap handling");
    mcm->addr = result;
    mcm->vfs_info = vfs_info (fd);
    mcm->vfs = vfs;
    mcm->next = mc_mmaparray;
    mc_mmaparray = mcm;
    return result;
}

int
mc_munmap (caddr_t addr, size_t len)
{
    struct mc_mmapping *mcm, *mcm2 = NULL;
    
    for (mcm = mc_mmaparray; mcm != NULL; mcm2 = mcm, mcm = mcm->next){
        if (mcm->addr == addr){
            if (mcm2 == NULL)
            	mc_mmaparray = mcm->next;
            else
            	mcm2->next = mcm->next;
	    if (*mcm->vfs->munmap)
	        (*mcm->vfs->munmap)(mcm->vfs, addr, len, mcm->vfs_info);
            free (mcm);
            return 0;
        }
    }
    return -1;
}

#endif

char *
mc_def_getlocalcopy (vfs *vfs, char *filename)
{
    char *tmp;
    int fdin, fdout, i;
    char buffer[8192];
    struct stat mystat;

    fdin = mc_open (filename, O_RDONLY);
    if (fdin == -1)
        return NULL;
    tmp = tempnam (NULL, "mclocalcopy");
    fdout = open (tmp, O_CREAT|O_WRONLY|O_TRUNC|O_EXCL, 0600);
    if (fdout == -1){
        mc_close (fdin);
	free (tmp);
        return NULL;
    }
    while ((i = mc_read (fdin, buffer, sizeof (buffer))) == sizeof (buffer)){
        write (fdout, buffer, i);
    }
    if (i != -1)
        write (fdout, buffer, i);
    mc_close (fdin);
    close (fdout);
    if (mc_stat (filename, &mystat) != -1){
        chmod (tmp, mystat.st_mode);
    }
    return tmp;
}

char *
mc_getlocalcopy (char *path)
{
    vfs *vfs;
    char *result;

    path = vfs_canon (path);
    vfs = vfs_type (path);    
    result = vfs->getlocalcopy ? (*vfs->getlocalcopy)(vfs, vfs_name (path)) :
                                 mc_def_getlocalcopy (vfs, vfs_name (path));
    free (path);
    if (!result)
	errno = ferrno (vfs);
    return result;
}

void
mc_def_ungetlocalcopy (vfs *vfs, char *filename, char *local, int has_changed)
{
    if (has_changed){
        int fdin, fdout, i;
        char buffer [8192];
    
        fdin = open (local, O_RDONLY);
        if (fdin == -1){
            unlink (local);
            free (local);
            return;
        }
        fdout = mc_open (filename, O_WRONLY | O_TRUNC);
        if (fdout == -1){
            close (fdin);
            unlink (local);
            free (local);
            return;
        }
        while ((i = read (fdin, buffer, sizeof (buffer))) == sizeof (buffer)){
            mc_write (fdout, buffer, i);
        }
        if (i != -1)
            mc_write (fdout, buffer, i);
        close (fdin);
        mc_close (fdout);
    }
    unlink (local);
    free (local);
}

void
mc_ungetlocalcopy (char *path, char *local, int has_changed)
{
    vfs *vfs;

    path = vfs_canon (path);
    vfs = vfs_type (path);
    vfs->ungetlocalcopy ? (*vfs->ungetlocalcopy)(vfs, vfs_name (path), local, has_changed) :
                          mc_def_ungetlocalcopy (vfs, vfs_name (path), local, has_changed);
    free (path);
}

/*
 * Hmm, as timeout is minute or so, do we need to care about usecs?
 */
inline int
timeoutcmp (struct timeval *t1, struct timeval *t2)
{
    return ((t1->tv_sec < t2->tv_sec)
	    || ((t1->tv_sec == t2->tv_sec) && (t1->tv_usec <= t2->tv_usec)));
}

void
vfs_timeout_handler (void)
{
    struct timeval time;
    struct vfs_stamping *stamp, *st;

    gettimeofday (&time, NULL);
    time.tv_sec -= vfs_timeout;

    for (stamp = stamps; stamp != NULL;){
        if (timeoutcmp (&stamp->time, &time)){
            st = stamp->next;
            (*stamp->v->free) (stamp->id);
	    vfs_rmstamp (stamp->v, stamp->id, 0);
	    stamp = st;
        } else
            stamp = stamp->next;
    }
}

void
vfs_init (void)
{
    time_t current_time;
    struct tm *t;
    
    memset (vfs_file_table, 0, sizeof (vfs_file_table));
    current_time = time (NULL);
    t = localtime (&current_time);
    current_mday = t->tm_mday;
    current_mon  = t->tm_mon;
    current_year = t->tm_year;

    /* We do not want to register vfs_local_ops */

#ifdef USE_NETCODE
    tcp_init();
    vfs_register (&vfs_ftpfs_ops);
    vfs_register (&vfs_mcfs_ops);
#endif

    vfs_register (&vfs_fish_ops);
    vfs_register (&vfs_extfs_ops);
    vfs_register (&vfs_sfs_ops);
    vfs_register (&vfs_tarfs_ops);

#ifdef USE_EXT2FSLIB
    vfs_register (&vfs_undelfs_ops);
#endif

    vfs_setup_wd ();
}

void
vfs_free_resources (char *path)
{
    vfs *vfs;
    vfsid vid;
    struct vfs_stamping *parent;
    
    vfs = vfs_type (path);
    vid = vfs_ncs_getid (vfs, path, &parent);
    if (vid != (vfsid) -1)
	(*vfs->free)(vid);
    vfs_rm_parents (parent);
}

#if 0
/* Shutdown a vfs given a path name */
void
vfs_shut_path (char *p)
{
    vfs *the_vfs;
    struct vfs_stamping *par;

    the_vfs = vfs_type (p);
    vfs_ncs_getid (the_vfs, p, &par);
    (*par->v->free)(par->id);
    vfs_rm_parents (par);
}
#endif

void
vfs_shut (void)
{
    struct vfs_stamping *stamp, *st;
    vfs *vfs;

    for (stamp = stamps, stamps = 0; stamp != NULL;){
	(*stamp->v->free)(stamp->id);
	st = stamp->next;
	free (stamp);
	stamp = st;
    }

    if (stamps)
	vfs_rmstamp (stamps->v, stamps->id, 1);
    
    if (current_dir)
	free (current_dir);

    for (vfs=vfs_list; vfs; vfs=vfs->next)
        if (vfs->done)
	     (*vfs->done) (vfs);
}

/*
 * These ones grab information from the VFS
 *  and handles them to an upper layer
 */
void
vfs_fill_names (void (*func)(char *))
{
    vfs *vfs;

    for (vfs=vfs_list; vfs; vfs=vfs->next)
        if (vfs->fill_names)
	    (*vfs->fill_names) (vfs, func);
}

/* Following stuff (parse_ls_lga) is used by ftpfs and extfs */
#define MAXCOLS 30

static char *columns [MAXCOLS];	/* Points to the string in column n */
static int   column_ptr [MAXCOLS]; /* Index from 0 to the starting positions of the columns */

int
vfs_split_text (char *p)
{
    char *original = p;
    int  numcols;


    for (numcols = 0; *p && numcols < MAXCOLS; numcols++){
	while (*p == ' ' || *p == '\r' || *p == '\n'){
	    *p = 0;
	    p++;
	}
	columns [numcols] = p;
	column_ptr [numcols] = p - original;
	while (*p && *p != ' ' && *p != '\r' && *p != '\n')
	    p++;
    }
    return numcols;
}

static int
is_num (int idx)
{
    if (!columns [idx] || columns [idx][0] < '0' || columns [idx][0] > '9')
	return 0;
    return 1;
}

static int
is_time (char *str, struct tm *tim)
{
    char *p, *p2;

    if ((p=strchr(str, ':')) && (p2=strrchr(str, ':'))) {
	if (p != p2) {
    	    if (sscanf (str, "%2d:%2d:%2d", &tim->tm_hour, &tim->tm_min, &tim->tm_sec) != 3)
		return (0);
	}
	else {
	    if (sscanf (str, "%2d:%2d", &tim->tm_hour, &tim->tm_min) != 2)
		return (0);
	}
    }
    else 
        return (0);
    
    return (1);
}

static int is_year(char *str, struct tm *tim)
{
    long year;

    if (strchr(str,':'))
        return 0;

    if (strlen(str)!=4)
        return 0;

    if (sscanf(str, "%ld", &year) != 1)
        return 0;

    if (year < 1900 || year > 3000)
        return 0;

    tim->tm_year = (int) (year - 1900);

    return 1;
}

/*
 * FIXME: this is broken. Consider following entry:
 * -rwx------   1 root     root            1 Aug 31 10:04 2904 1234
 * where "2904 1234" is filename. Well, this code decodes it as year :-(.
 */

int
vfs_parse_filetype (char c)
{
    switch (c){
        case 'd': return S_IFDIR; 
        case 'b': return S_IFBLK;
        case 'c': return S_IFCHR;
        case 'l': return S_IFLNK;
        case 's':
#ifdef IS_IFSOCK /* And if not, we fall through to IFIFO, which is pretty close */
	          return S_IFSOCK;
#endif
        case 'p': return S_IFIFO;
        case 'm': case 'n':		/* Don't know what these are :-) */
        case '-': case '?': return S_IFREG;
        default: return -1;
    }
}

int vfs_parse_filemode (char *p)
{	/* converts rw-rw-rw- into 0666 */
    int res = 0;
    switch (*(p++)){
	case 'r': res |= 0400; break;
	case '-': break;
	default: return -1;
    }
    switch (*(p++)){
	case 'w': res |= 0200; break;
	case '-': break;
	default: return -1;
    }
    switch (*(p++)){
	case 'x': res |= 0100; break;
	case 's': res |= 0100 | S_ISUID; break;
	case 'S': res |= S_ISUID; break;
	case '-': break;
	default: return -1;
    }
    switch (*(p++)){
	case 'r': res |= 0040; break;
	case '-': break;
	default: return -1;
    }
    switch (*(p++)){
	case 'w': res |= 0020; break;
	case '-': break;
	default: return -1;
    }
    switch (*(p++)){
	case 'x': res |= 0010; break;
	case 's': res |= 0010 | S_ISGID; break;
	case 'S': res |= S_ISGID; break;
	case '-': break;
	default: return -1;
    }
    switch (*(p++)){
	case 'r': res |= 0004; break;
	case '-': break;
	default: return -1;
    }
    switch (*(p++)){
	case 'w': res |= 0002; break;
	case '-': break;
	default: return -1;
    }
    switch (*(p++)){
	case 'x': res |= 0001; break;
	case 't': res |= 0001 | S_ISVTX; break;
	case 'T': res |= S_ISVTX; break;
	case '-': break;
	default: return -1;
    }
    return res;
}

int vfs_parse_filedate(int idx, time_t *t)
{	/* This thing parses from idx in columns[] array */
    static char *month = "JanFebMarAprMayJunJulAugSepOctNovDec";
    static char *week = "SunMonTueWedThuFriSat";
    char *p, *pos;
    int extfs_format_date = 0;
    struct tm tim;

    /* Let's setup default time values */
    tim.tm_year = current_year;
    tim.tm_mon  = current_mon;
    tim.tm_mday = current_mday;
    tim.tm_hour = 0;
    tim.tm_min  = 0;
    tim.tm_sec  = 0;
    tim.tm_isdst = 0;
    
    p = columns [idx++];
    
    if((pos=strstr(week, p)) != NULL){
        tim.tm_wday = (pos - week)/3;
	p = columns [idx++];
	}

    if((pos=strstr(month, p)) != NULL)
        tim.tm_mon = (pos - month)/3;
    else {
        /* This case should not normaly happen, but in extfs we allow these
           date formats:
           Mon DD hh:mm
           Mon DD YYYY
           Mon DD YYYY hh:mm
	   Wek Mon DD hh:mm:ss YYYY
           MM-DD-YY hh:mm
           where Mon is Jan-Dec, DD, MM, YY two digit day, month, year,
           YYYY four digit year, hh, mm two digit hour and minute. */

        if (strlen (p) == 8 && p [2] == '-' && p [5] == '-'){
            p [2] = 0;
            p [5] = 0;
            tim.tm_mon = (int) atol (p);
            if (!tim.tm_mon)
                return 0;
            else
                tim.tm_mon--;
            tim.tm_mday = (int) atol (p + 3);
            tim.tm_year = (int) atol (p + 6);
            if (tim.tm_year < 70)
                tim.tm_year += 70;
            extfs_format_date = 1;
        } else
            return 0;
    }

    if (!extfs_format_date){
        if (!is_num (idx))
	    return 0;
        tim.tm_mday = (int)atol (columns [idx++]);
    }
    
    if (is_num (idx)) {
        if(is_time(columns[idx], &tim) || is_year(columns[idx], &tim)) {
	idx++;
	 
	if(is_num (idx) && 
	    (is_year(columns[idx], &tim) || is_time(columns[idx], &tim)))
	    idx++; /* time & year or reverse */
	 } /* only time or date */
    }
    else 
        return 0; /* Nor time or date */

    if ((*t = mktime(&tim)) ==-1)
        *t = 0;
    return idx;
}

int
vfs_parse_ls_lga (char *p, struct stat *s, char **filename, char **linkname)
{
    int idx, idx2, num_cols, isconc = 0;
    int i;
    char *p_copy;
    
    if (strncmp (p, "total", 5) == 0)
        return 0;

    p_copy = strdup(p);
    if ((i = vfs_parse_filetype(*(p++))) == -1)
        goto error;

    s->st_mode = i;
    if (*p == '['){
	if (strlen (p) <= 8 || p [8] != ']')
	    goto error;
	/* Should parse here the Notwell permissions :) */
	if (S_ISDIR (s->st_mode))
	    s->st_mode |= (S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IXUSR | S_IXGRP | S_IXOTH);
	else
	    s->st_mode |= (S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
	p += 9;
    } else {
	if ((i = vfs_parse_filemode(p)) == -1)
	    goto error;
        s->st_mode |= i;
	p += 9;

        /* This is for an extra ACL attribute (HP-UX) */
        if (*p == '+')
            p++;
    }

    free(p_copy);
    p_copy = strdup(p);
    num_cols = vfs_split_text (p);

    s->st_nlink = atol (columns [0]);
    if (s->st_nlink <= 0)
        goto error;

    if (!is_num (1))
	s->st_uid = finduid (columns [1]);
    else
        s->st_uid = (uid_t) atol (columns [1]);

    /* Mhm, the ls -lg did not produce a group field */
    for (idx = 3; idx <= 5; idx++) 
        if ((*columns [idx] >= 'A' && *columns [idx] <= 'S' &&
            strlen (columns[idx]) == 3) || (strlen (columns[idx])==8 &&
            columns [idx][2] == '-' && columns [idx][5] == '-'))
            break;
    if (idx == 6 || (idx == 5 && !S_ISCHR (s->st_mode) && !S_ISBLK (s->st_mode)))
        goto error;
    if (idx < 5){
        char *p = strchr(columns [idx - 1], ',');
        if (p && p[1] >= '0' && p[1] <= '9')
            isconc = 1;
    }
    if (idx == 3 || (idx == 4 && !isconc && (S_ISCHR(s->st_mode) || S_ISBLK (s->st_mode))))
        idx2 = 2;
    else {
	if (is_num (2))
	    s->st_gid = (gid_t) atol (columns [2]);
	else
	    s->st_gid = findgid (columns [2]);
	idx2 = 3;
    }

    if (S_ISCHR (s->st_mode) || S_ISBLK (s->st_mode)){
        char *p;
	if (!is_num (idx2))
	    goto error;
#ifdef HAVE_ST_RDEV
	s->st_rdev = (atol (columns [idx2]) & 0xff) << 8;
#endif
	if (isconc){
	    p = strchr (columns [idx2], ',');
	    if (!p || p [1] < '0' || p [1] > '9')
	        goto error;
	    p++;
	} else {
	    p = columns [idx2 + 1];
	    if (!is_num (idx2+1))
	        goto error;
	}
	
#ifdef HAVE_ST_RDEV
	s->st_rdev |= (atol (p) & 0xff);
#endif
	s->st_size = 0;
    } else {
	if (!is_num (idx2))
	    goto error;
	
	s->st_size = (size_t) atol (columns [idx2]);
#ifdef HAVE_ST_RDEV
	s->st_rdev = 0;
#endif
    }

    idx = vfs_parse_filedate(idx, &s->st_mtime);
    if (!idx)
        goto error;
    /* Use resulting time value */
    s->st_atime = s->st_ctime = s->st_mtime;
    s->st_dev = 0;
    s->st_ino = 0;
#ifdef HAVE_ST_BLKSIZE
    s->st_blksize = 512;
#endif
#ifdef HAVE_ST_BLOCKS
    s->st_blocks = (s->st_size + 511) / 512;
#endif

    for (i = idx + 1, idx2 = 0; i < num_cols; i++ ) 
	if (strcmp (columns [i], "->") == 0){
	    idx2 = i;
	    break;
	}
    
    if (((S_ISLNK (s->st_mode) || 
        (num_cols == idx + 3 && s->st_nlink > 1))) /* Maybe a hardlink? (in extfs) */
        && idx2){
	int p;
	char *s;
	    
	if (filename){
	    p = column_ptr [idx2] - column_ptr [idx];
	    s = xmalloc (p, "filename");
	    strncpy (s, p_copy + column_ptr [idx], p - 1);
	    s[p - 1] = '\0';
	    *filename = s;
	}
	if (linkname){
	    s = strdup (p_copy + column_ptr [idx2+1]);
	    p = strlen (s);
	    if (s [p-1] == '\r' || s [p-1] == '\n')
		s [p-1] = 0;
	    if (s [p-2] == '\r' || s [p-2] == '\n')
		s [p-2] = 0;
		
	    *linkname = s;
	}
    } else {
	/* Extract the filename from the string copy, not from the columns
	 * this way we have a chance of entering hidden directories like ". ."
	 */
	if (filename){
	    /* 
	    *filename = strdup (columns [idx++]);
	    */
	    int p;
	    char *s;
	    
	    s = strdup (p_copy + column_ptr [idx++]);
	    p = strlen (s);
	    if (s [p-1] == '\r' || s [p-1] == '\n')
	        s [p-1] = 0;
	    if (s [p-2] == '\r' || s [p-2] == '\n')
		s [p-2] = 0;
	    
	    *filename = s;
	}
	if (linkname)
	    *linkname = NULL;
    }
    free (p_copy);
    return 1;

error:
    message_1s (1, "Could not parse:", p_copy);
    if (p_copy != p)		/* Carefull! */
	free (p_copy);
    return 0;
}

void
vfs_die (char *m)
{
    message_1s (1, "Internal error:", m);
    exit (1);
}

void
vfs_print_stats (char *fs_name, char *action, char *file_name, int have, int need)
{
    if (need) 
        print_vfs_message ("%s: %s: %s %3d%% (%ld bytes transfered)", 
			   fs_name, action, file_name, have*100/need, have);
    else
        print_vfs_message ("%s: %s: %s %ld bytes transfered",
			   fs_name, action, file_name, have);
}

#ifndef VFS_STANDALONE
char *
vfs_get_password (char *msg)
{
    return (char *) input_dialog (msg, _("Password:"), "");
}
#endif

/*
 * Returns vfs path corresponding to given url. If passed string is
 * not recognized as url, strdup(url) is returned.
 */
char *
vfs_translate_url (char *url)
{
    if (strncmp (url, "ftp://", 6) == 0)
        return copy_strings ("/#ftp:", url + 6, 0);
    else
        return strdup (url);
}
