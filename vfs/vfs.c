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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Warning: funtions like extfs_lstat() have right to destroy any
 * strings you pass to them. This is acutally ok as you g_strdup what
 * you are passing to them, anyway; still, beware.  */

/* Namespace: exports *many* functions with vfs_ prefix; exports
   parse_ls_lga and friends which do not have that prefix. */

#include <config.h>

#ifndef NO_SYSLOG_H
#  include <syslog.h>
#endif

#include <stdio.h>
#include <stdlib.h>	/* For atol() */
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>

#include "utilvfs.h"

#include "../src/panel.h"	/* get_current_panel() */
#include "../src/layout.h"	/* get_current_type() */
#include "../src/wtools.h"	/* input_dialog() */

#include "xdirentry.h"
#include "vfs.h"
#include "extfs.h"		/* FIXME: we should not know anything about our modules */
#include "names.h"
#ifdef USE_NETCODE
#   include "tcputil.h"
#endif

int vfs_timeout = 60; /* VFS timeout in seconds */
static int vfs_flags = 0;    /* Flags */

/* They keep track of the current directory */
static vfs *current_vfs = &vfs_local_ops;
static char *current_dir = NULL;

/*
 * FIXME: this is broken. It depends on mc not crossing border on month!
 */
static int current_mday;
static int current_mon;
static int current_year;

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

static int
vfs_register (vfs *vfs)
{
    if (!vfs)
	vfs_die("You can not register NULL.");
    
    if (vfs->init)		/* vfs has own initialization function */
	if (!(*vfs->init)(vfs))	/* but it failed */
	    return 0;

    vfs->next = vfs_list;
    vfs_list = vfs;

    return 1;
}

static vfs *
vfs_type_from_op (char *path)
{
    vfs *vfs;

    if (!path)
	vfs_die ("vfs_type_from_op got NULL: impossible");
    
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
vfs_strip_suffix_from_filename (const char *filename)
{
    vfs *vfs;
    char *semi;
    char *p;

    if (!filename)
	vfs_die("vfs_strip_suffix_from_path got NULL: impossible");
    
    p = g_strdup (filename);
    if (!(semi = strrchr (p, '#')))
	return p;

    for (vfs = vfs_list; vfs != &vfs_local_ops; vfs = vfs->next){
        if (vfs->which){
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
path_magic (const char *path)
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
 * What is left in path is p1. You still want to g_free(path), you DON'T
 * want to free neither *inpath nor *op
 */
vfs *
vfs_split (char *path, char **inpath, char **op)
{
    char *semi;
    char *slash;
    vfs *ret;

    if (!path)
	vfs_die("Can not split NULL");
    
    semi = strrchr (path, '#');
    if (!semi || !path_magic(path))
	return NULL;

    slash = strchr (semi, PATH_SEP);
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
	*slash = PATH_SEP;
    ret = vfs_split (path, inpath, op);
    *semi = '#';
    return ret;
}

static vfs *
vfs_rosplit (char *path)
{
    char *semi;
    char *slash;
    vfs *ret;

    g_return_val_if_fail(path, NULL);

    semi = strrchr (path, '#');
    if (!semi || !path_magic (path))
	return NULL;
    
    slash = strchr (semi, PATH_SEP);
    *semi = 0;
    if (slash)
	*slash = 0;
    
    ret = vfs_type_from_op (semi+1);
    if (!ret && (vfs_flags & FL_NO_LOCALHASH))
	return &vfs_nil_ops;

    if (slash)
	*slash = PATH_SEP;
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

static void
vfs_addstamp (vfs *v, vfsid id, struct vfs_stamping *parent)
{
    if (v != &vfs_local_ops && id != (vfsid)-1){
        struct vfs_stamping *stamp;
	struct vfs_stamping *last_stamp = NULL;
        
        for (stamp = stamps; stamp != NULL; stamp = stamp->next) {
            if (stamp->v == v && stamp->id == id){
		gettimeofday(&(stamp->time), NULL);
                return;
	    }
	    last_stamp = stamp;
	}
        stamp = g_new (struct vfs_stamping, 1);
        stamp->v = v;
        stamp->id = id;
	if (parent){
	    struct vfs_stamping *st = stamp;
	    while (parent){
		st->parent = g_new (struct vfs_stamping, 1);
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

	if (stamps) {
	    /* Add to the end */
	    last_stamp->next = stamp;
	} else {
	    /* Add first element */
    	    stamps = stamp;
	}
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
    struct vfs_stamping *parent;

    while (stamp) {
	parent = stamp->parent;
	g_free (stamp);
	stamp = parent;
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
            g_free (stamp);

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
mc_open (const char *filename, int flags, ...)
{
    int  handle;
    int  mode;
    void *info;
    va_list ap;

    char *file = vfs_canon (filename);
    vfs  *vfs  = vfs_type (file);

    /* Get the mode flag */	/* FIXME: should look if O_CREAT is present */
    va_start (ap, flags);
    mode = va_arg (ap, int);
    va_end (ap);
    
    if (!vfs->open) {
	errno = -EOPNOTSUPP;
	return -1;
    }

    info = (*vfs->open) (vfs, file, flags, mode);	/* open must be supported */
    g_free (file);
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
  MC_OP (name, inarg, callarg, path = vfs_canon (path); vfs = vfs_type (path);, g_free (path); )
#define MC_HANDLEOP(name, inarg, callarg) \
  MC_OP (name, inarg, callarg, if (handle == -1) return -1; vfs = vfs_op (handle);, ;)

MC_HANDLEOP(read, (int handle, char *buffer, int count), (vfs_info (handle), buffer, count) )

int
mc_ctl (int handle, int ctlop, int arg)
{
    vfs *vfs = vfs_op (handle);

    return vfs->ctl ? (*vfs->ctl)(vfs_info (handle), ctlop, arg) : 0;
}

int
mc_setctl (char *path, int ctlop, char *arg)
{
    vfs *vfs;
    int result;

    if (!path)
	vfs_die("You don't want to pass NULL to mc_setctl.");
    
    path = vfs_canon (path);
    vfs = vfs_type (path);    
    result = vfs->setctl ? (*vfs->setctl)(vfs, path, ctlop, arg) : 0;
    g_free (path);
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

    dirname = vfs_canon (dirname);
    vfs = vfs_type (dirname);

    info = vfs->opendir ? (*vfs->opendir)(vfs, dirname) : NULL;
    g_free (dirname);
    if (!info){
        errno = vfs->opendir ? ferrno (vfs) : E_NOTSUPP;
	return NULL;
    }
    handle = get_bucket ();
    vfs_file_table [handle].fs_info = info;
    vfs_file_table [handle].operations = vfs;

    handlep = g_new (int, 1);
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
    g_free (dirp);
    return result; 
}

int mc_stat (char *path, struct stat *buf) {
    vfs *vfs;
    int result;

    path = vfs_canon (path); vfs = vfs_type (path);
    result = vfs->stat ? (*vfs->stat) (vfs, vfs_name (path), buf) : -1;
    g_free (path);
    if (result == -1)
	errno = vfs->name ? ferrno (vfs) : E_NOTSUPP;
    return result;
}

int mc_lstat (char *path, struct stat *buf) {
    vfs *vfs;
    int result;

    path = vfs_canon (path); vfs = vfs_type (path);
    result = vfs->lstat ? (*vfs->lstat) (vfs, vfs_name (path), buf) : -1;
    g_free (path);
    if (result == -1)
	errno = vfs->name ? ferrno (vfs) : E_NOTSUPP;
    return result;
}

int mc_fstat (int handle, struct stat *buf) {
    vfs *vfs;
    int result;

    if (handle == -1)
	return -1;
    vfs = vfs_op (handle);
    result = vfs->fstat ? (*vfs->fstat) (vfs_info (handle), buf) : -1;
    if (result == -1)
	errno = vfs->name ? ferrno (vfs) : E_NOTSUPP;
    return result;
}

/*
 * You must g_strdup whatever this function returns.
 */

static const char *
mc_return_cwd (void)
{
    char *p;
    struct stat my_stat, my_stat2;

    if (!vfs_rosplit (current_dir)){
	p = g_get_current_dir ();
	if (!p)  /* One of the directories in the path is not readable */
	    return current_dir;

	/* Otherwise check if it is O.K. to use the current_dir */
	if (!cd_symlinks ||
	    mc_stat (p, &my_stat) || 
	    mc_stat (current_dir, &my_stat2) ||
	    my_stat.st_ino != my_stat2.st_ino ||
	    my_stat.st_dev != my_stat2.st_dev){
	    g_free (current_dir);
	    current_dir = p;
	    return p;
	} /* Otherwise we return current_dir below */
	g_free (p);
    } 
    return current_dir;
}

char *
mc_get_current_wd (char *buffer, int size)
{
    const char *cwd = mc_return_cwd();

    strncpy (buffer, cwd, size - 1);
    buffer [size - 1] = 0;
    return buffer;
}

MC_NAMEOP (chmod, (char *path, int mode), (vfs, vfs_name (path), mode))
MC_NAMEOP (chown, (char *path, int owner, int group), (vfs, vfs_name (path), owner, group))
MC_NAMEOP (utime, (char *path, struct utimbuf *times), (vfs, vfs_name (path), times))
MC_NAMEOP (readlink, (char *path, char *buf, int bufsiz), (vfs, vfs_name (path), buf, bufsiz))
MC_NAMEOP (unlink, (char *path), (vfs, vfs_name (path)))
MC_NAMEOP (symlink, (char *name1, char *path), (vfs, vfs_name (name1), vfs_name (path)))

#define MC_RENAMEOP(name) \
int mc_##name (const char *fname1, const char *fname2) \
{ \
    vfs *vfs; \
    int result; \
\
    char *name2, *name1 = vfs_canon (fname1); \
    vfs = vfs_type (name1); \
    name2 = vfs_canon (fname2); \
    if (vfs != vfs_type (name2)){ \
    	errno = EXDEV; \
    	g_free (name1); \
    	g_free (name2); \
	return -1; \
    } \
\
    result = vfs->name ? (*vfs->name)(vfs, vfs_name (name1), vfs_name (name2)) : -1; \
    g_free (name1); \
    g_free (name2); \
    if (result == -1) \
        errno = vfs->name ? ferrno (vfs) : E_NOTSUPP; \
    return result; \
}

MC_RENAMEOP (link)
MC_RENAMEOP (rename)

MC_HANDLEOP (write, (int handle, char *buf, int nbyte), (vfs_info (handle), buf, nbyte))

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
vfs_canon (const char *path)
{
    if (!path)
	vfs_die("Cannot canonicalize NULL");

    /* Tilde expansion */
    if (*path == '~'){ 
    	char *local, *result;

    	local = tilde_expand (path);
	if (local){
	    result = vfs_canon (local);
	    g_free (local);
	    return result;
	}
    }

    /* Relative to current directory */
    if (*path != PATH_SEP){ 
    	char *local, *result;

	local = concat_dir_and_file (current_dir, path);

	result = vfs_canon (local);
	g_free (local);
	return result;
    }

    /*
     * So we have path of following form:
     * /p1/p2#op/.././././p3#op/p4. Good luck.
     */
    {
	char *result = g_strdup (path);
	canonicalize_pathname (result);
        return result;
    }
}

vfsid
vfs_ncs_getid (vfs *nvfs, char *dir, struct vfs_stamping **par)
{
    vfsid nvfsid;

    dir = concat_dir_and_file (dir, "");

    nvfsid = (*nvfs->getid)(nvfs, dir, par);
    
    g_free (dir);
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

void
vfs_add_current_stamps (void)
{
    vfs_stamp_path (current_dir);

    if (cpanel) {
	if (get_current_type () == view_listing)
	    vfs_stamp_path (cpanel->cwd);
    }

    if (opanel) {
	if (get_other_type () == view_listing)
	    vfs_stamp_path (opanel->cwd);
    }
}

/* This function is really broken */
int
mc_chdir (char *path)
{
    char *a, *b;
    int result;
    char *p = NULL;
    vfs *oldvfs;
    vfsid oldvfsid;
    struct vfs_stamping *parent;

    a = current_dir; /* Save a copy for case of failure */
    current_dir = vfs_canon (path);
    current_vfs = vfs_type (current_dir);
    b = g_strdup (current_dir);
    result = (*current_vfs->chdir) ?  (*current_vfs->chdir)(current_vfs, vfs_name (b)) : -1;
    g_free (b);
    if (result == -1){
	errno = ferrno (current_vfs);
    	g_free (current_dir);
    	current_vfs = vfs_type (a);
    	current_dir = a;
    } else {
        oldvfs = vfs_type (a);
	oldvfsid = vfs_ncs_getid (oldvfs, a, &parent);
	g_free (a);
        vfs_add_noncurrent_stamps (oldvfs, oldvfsid, parent);
	vfs_rm_parents (parent);
    }

    if (*current_dir){
	p = strchr (current_dir, 0) - 1;
	if (*p == PATH_SEP && p > current_dir)
	    *p = 0; /* Sometimes we assume no trailing slash on cwd */
    }
    return result;
}

int
vfs_current_is_local (void)
{
    return current_vfs == &vfs_local_ops;
}

int
vfs_file_is_local (const char *file)
{
    char *filename = vfs_canon (file);
    vfs *vfs = vfs_type (filename);
    
    g_free (filename);
    return vfs == &vfs_local_ops;
}

int
vfs_file_is_ftp (char *filename)
{
#ifdef USE_NETCODE
    vfs *vfs;
    
    filename = vfs_canon (filename);
    vfs = vfs_type (filename);
    g_free (filename);
    return vfs == &vfs_ftpfs_ops;
#else
    return 0;
#endif
}

int
vfs_file_is_smb (char *filename)
{
#ifdef WITH_SMBFS
#ifdef USE_NETCODE
    vfs *vfs;
    
    filename = vfs_canon (filename);
    vfs = vfs_type (filename);
    g_free (filename);
    return vfs == &vfs_smbfs_ops;
#endif /* USE_NETCODE */
#endif /* WITH_SMBFS */
    return 0;
}

char *vfs_get_current_dir (void)
{
    return current_dir;
}

static void vfs_setup_wd (void)
{
    current_dir = g_strdup (PATH_SEP_STR);
    if (!(vfs_flags & FL_NO_CWDSETUP))
        mc_return_cwd();

    if (strlen(current_dir)>MC_MAXPATHLEN-2)
        vfs_die ("Current dir too long.\n");
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
    mcm =g_new (struct mc_mmapping, 1);
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
	    if (mcm->vfs->munmap)
	        (*mcm->vfs->munmap)(mcm->vfs, addr, len, mcm->vfs_info);
            g_free (mcm);
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
    char *ext = NULL;
    char *ptr;

    fdin = mc_open (filename, O_RDONLY);
    if (fdin == -1)
        return NULL;

    /* Try to preserve existing extension */
    for (ptr = filename + strlen(filename) - 1; ptr >= filename; ptr--) {
	if (*ptr == '.') {
	    ext = ptr;
	    break;
	}

	if (!isalnum((unsigned char) *ptr))
	    break;
    }

    fdout = mc_mkstemps (&tmp, "mclocalcopy", ext);
    if (fdout == -1)
	goto fail;
    while ((i = mc_read (fdin, buffer, sizeof (buffer))) > 0){
	if (write (fdout, buffer, i) != i)
	    goto fail;
    }
    if (i == -1)
	goto fail;
    i = mc_close (fdin);
    fdin = -1;
    if (i==-1)
	goto fail;
    if (close (fdout)==-1)
	goto fail;

    if (mc_stat (filename, &mystat) != -1){
        chmod (tmp, mystat.st_mode);
    }
    return tmp;

 fail:
    if (fdout) close(fdout);
    if (fdin) mc_close (fdin);
    g_free (tmp);
    return NULL;
}

char *
mc_getlocalcopy (const char *pathname)
{
    char *result;
    char *path = vfs_canon (pathname);
    vfs  *vfs  = vfs_type (path);    

    result = vfs->getlocalcopy ? (*vfs->getlocalcopy)(vfs, vfs_name (path)) :
                                 mc_def_getlocalcopy (vfs, vfs_name (path));
    g_free (path);
    if (!result)
	errno = ferrno (vfs);
    return result;
}

int
mc_def_ungetlocalcopy (vfs *vfs, char *filename, char *local, int has_changed)
{	/* Dijkstra probably hates me... But he should teach me how to do this nicely. */
    int fdin = -1, fdout = -1, i;
    if (has_changed){
        char buffer [8192];
    
        fdin = open (local, O_RDONLY);
        if (fdin == -1)
	    goto failed;
        fdout = mc_open (filename, O_WRONLY | O_TRUNC);
        if (fdout == -1)
	    goto failed;
	while ((i = read (fdin, buffer, sizeof (buffer))) > 0){
	    if (mc_write (fdout, buffer, i) != i)
		goto failed;
	}
	if (i == -1)
	    goto failed;

        if (close (fdin)==-1) {
	    fdin = -1;
	    goto failed;
	}
	fdin = -1;
        if (mc_close (fdout)==-1) {
	    fdout = -1;
	    goto failed;
	}
    }
    unlink (local);
    g_free (local);
    return 0;

 failed:
    message_1s (1, _("Changes to file lost"), filename);
    if (fdout!=-1) mc_close(fdout);
    if (fdin!=-1) close(fdin);
    unlink (local);
    g_free (local);
    return -1;
}

int
mc_ungetlocalcopy (const char *pathname, char *local, int has_changed)
{
    int return_value = 0;
    char *path = vfs_canon (pathname);
    vfs *vfs = vfs_type (path);

    return_value = vfs->ungetlocalcopy ? 
            (*vfs->ungetlocalcopy)(vfs, vfs_name (path), local, has_changed) :
            mc_def_ungetlocalcopy (vfs, vfs_name (path), local, has_changed);
    g_free (path);
    return return_value;
}

/*
 * Hmm, as timeout is minute or so, do we need to care about usecs?
 */
static inline int
timeoutcmp (struct timeval *t1, struct timeval *t2)
{
    return ((t1->tv_sec < t2->tv_sec)
	    || ((t1->tv_sec == t2->tv_sec) && (t1->tv_usec <= t2->tv_usec)));
}

/* This is called from timeout handler with now = 0, or can be called
   with now = 1 to force freeing all filesystems that are not in use */

void
vfs_expire (int now)
{
    static int locked = 0;
    struct timeval time;
    struct vfs_stamping *stamp, *st;

    /* Avoid recursive invocation, e.g. when one of the free functions
       calls message_1s */
    if (locked)
	return;
    locked = 1;

    gettimeofday (&time, NULL);
    time.tv_sec -= vfs_timeout;

    for (stamp = stamps; stamp != NULL;){
        if (now || (timeoutcmp (&stamp->time, &time))){
            st = stamp->next;
            (*stamp->v->free) (stamp->id);
	    vfs_rmstamp (stamp->v, stamp->id, 0);
	    stamp = st;
        } else
            stamp = stamp->next;
    }
    locked = 0;
}

void
vfs_timeout_handler (void)
{
    vfs_expire (0);
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
    vfs_register (&vfs_fish_ops);
#ifdef WITH_SMBFS
    vfs_register (&vfs_smbfs_ops);
#endif /* WITH_SMBFS */
#ifdef WITH_MCFS
    vfs_register (&vfs_mcfs_ops);
#endif /* WITH_SMBFS */
#endif /* USE_NETCODE */

    vfs_register (&vfs_extfs_ops);
    vfs_register (&vfs_sfs_ops);
    vfs_register (&vfs_tarfs_ops);
    vfs_register (&vfs_cpiofs_ops);

#ifdef USE_EXT2FSLIB
    vfs_register (&vfs_undelfs_ops);
#endif /* USE_EXT2FSLIB */

    vfs_setup_wd ();
}

void
vfs_shut (void)
{
    struct vfs_stamping *stamp, *st;
    vfs *vfs;

    for (stamp = stamps, stamps = 0; stamp != NULL;){
	(*stamp->v->free)(stamp->id);
	st = stamp->next;
	g_free (stamp);
	stamp = st;
    }

    if (stamps)
	vfs_rmstamp (stamps->v, stamps->id, 1);
    
    if (current_dir)
	g_free (current_dir);

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
#define MAXCOLS		30

static char *columns [MAXCOLS];	/* Points to the string in column n */
static int   column_ptr [MAXCOLS]; /* Index from 0 to the starting positions of the columns */

int
vfs_split_text (char *p)
{
    char *original = p;
    int  numcols;

    memset (columns, 0, sizeof (columns));

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
    char *column = columns[idx];

    if (!column || column[0] < '0' || column[0] > '9')
	return 0;

    return 1;
}

static int
is_dos_date (char *str)
{
    if (!str)
	return 0;

    if (strlen (str) == 8 && str[2] == str[5]
	&& strchr ("\\-/", (int) str[2]) != NULL)
	return 1;

    return 0;
}

static int
is_week (char *str, struct tm *tim)
{
    static const char *week = "SunMonTueWedThuFriSat";
    char *pos;

    if (!str)
	return 0;

    if ((pos = strstr (week, str)) != NULL) {
	if (tim != NULL)
	    tim->tm_wday = (pos - week) / 3;
	return 1;
    }
    return 0;
}

static int
is_month (char *str, struct tm *tim)
{
    static const char *month = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char *pos;

    if (!str)
	return 0;

    if ((pos = strstr (month, str)) != NULL) {
	if (tim != NULL)
	    tim->tm_mon = (pos - month) / 3;
	return 1;
    }
    return 0;
}

static int
is_time (char *str, struct tm *tim)
{
    char *p, *p2;

    if (!str)
	return 0;

    if ((p = strchr (str, ':')) && (p2 = strrchr (str, ':'))) {
	if (p != p2) {
	    if (sscanf
		(str, "%2d:%2d:%2d", &tim->tm_hour, &tim->tm_min,
		 &tim->tm_sec) != 3)
		return 0;
	} else {
	    if (sscanf (str, "%2d:%2d", &tim->tm_hour, &tim->tm_min) != 2)
		return 0;
	}
    } else
	return 0;

    return 1;
}

static int is_year (char *str, struct tm *tim)
{
    long year;

    if (!str)
	return 0;

    if (strchr (str, ':'))
	return 0;

    if (strlen (str) != 4)
	return 0;

    if (sscanf (str, "%ld", &year) != 1)
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
    switch (c) {
	case 'd': return S_IFDIR; 
	case 'b': return S_IFBLK;
	case 'c': return S_IFCHR;
	case 'l': return S_IFLNK;
	case 's': /* Socket */
#ifdef S_IFSOCK
		return S_IFSOCK;
#else
		/* If not supported, we fall through to IFIFO */
		return S_IFIFO;
#endif
	case 'D': /* Solaris door */
#ifdef S_IFDOOR
		return S_IFDOOR;
#else
		return S_IFIFO;
#endif
	case 'p': return S_IFIFO;
	case 'm': case 'n':		/* Don't know what these are :-) */
	case '-': case '?': return S_IFREG;
	default: return -1;
    }
}

int vfs_parse_filemode (const char *p)
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
        case 'l': /* Solaris produces these */
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

/* This function parses from idx in the columns[] array */
int
vfs_parse_filedate (int idx, time_t *t)
{
    char *p;
    struct tm tim;
    int d[3];
    int got_year = 0;

    /* Let's setup default time values */
    tim.tm_year = current_year;
    tim.tm_mon = current_mon;
    tim.tm_mday = current_mday;
    tim.tm_hour = 0;
    tim.tm_min = 0;
    tim.tm_sec = 0;
    tim.tm_isdst = -1;		/* Let mktime() try to guess correct dst offset */

    p = columns[idx++];

    /* We eat weekday name in case of extfs */
    if (is_week (p, &tim))
	p = columns[idx++];

    /* Month name */
    if (is_month (p, &tim)) {
	/* And we expect, it followed by day number */
	if (is_num (idx))
	    tim.tm_mday = (int) atol (columns[idx++]);
	else
	    return 0;		/* No day */

    } else {
	/* We usually expect:
	   Mon DD hh:mm
	   Mon DD  YYYY
	   But in case of extfs we allow these date formats:
	   Mon DD YYYY hh:mm
	   Mon DD hh:mm YYYY
	   Wek Mon DD hh:mm:ss YYYY
	   MM-DD-YY hh:mm
	   where Mon is Jan-Dec, DD, MM, YY two digit day, month, year,
	   YYYY four digit year, hh, mm, ss two digit hour, minute or second. */

	/* Here just this special case with MM-DD-YY */
	if (is_dos_date (p)) {
	    p[2] = p[5] = '-';

	    if (sscanf (p, "%2d-%2d-%2d", &d[0], &d[1], &d[2]) == 3) {
		/*  We expect to get:
		   1. MM-DD-YY
		   2. DD-MM-YY
		   3. YY-MM-DD
		   4. YY-DD-MM  */

		/* Hmm... maybe, next time :) */

		/* At last, MM-DD-YY */
		d[0]--;		/* Months are zerobased */
		/* Y2K madness */
		if (d[2] < 70)
		    d[2] += 100;

		tim.tm_mon = d[0];
		tim.tm_mday = d[1];
		tim.tm_year = d[2];
		got_year = 1;
	    } else
		return 0;	/* sscanf failed */
	} else
	    return 0;		/* unsupported format */
    }

    /* Here we expect to find time and/or year */

    if (is_num (idx)) {
	if (is_time (columns[idx], &tim)
	    || (got_year = is_year (columns[idx], &tim))) {
	    idx++;

	    /* This is a special case for ctime() or Mon DD YYYY hh:mm */
	    if (is_num (idx) && (columns[idx + 1][0])) {
		if (got_year) {
		    if (is_time (columns[idx], &tim))
			idx++;	/* time also */
		} else {
		    if ((got_year = is_year (columns[idx], &tim)))
			idx++;	/* year also */
		}
	    }
	}			/* only time or date */
    } else
	return 0;		/* Nor time or date */

    /*
     * If the date is less than 6 months in the past, it is shown without year
     * other dates in the past or future are shown with year but without time
     * This does not check for years before 1900 ... I don't know, how
     * to represent them at all
     */
    if (!got_year && current_mon < 6 && current_mon < tim.tm_mon
	&& tim.tm_mon - current_mon >= 6)

	tim.tm_year--;

    if ((*t = mktime (&tim)) < 0)
	*t = 0;
    return idx;
}

int
vfs_parse_ls_lga (const char *p, struct stat *s, char **filename, char **linkname)
{
    int idx, idx2, num_cols;
    int i;
    char *p_copy = NULL;
    char *t = NULL;
    const char *line = p;

    if (strncmp (p, "total", 5) == 0)
        return 0;

    if ((i = vfs_parse_filetype(*(p++))) == -1)
        goto error;

    s->st_mode = i;
    if (*p == ' ')	/* Notwell 4 */
        p++;
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

    p_copy = g_strdup(p);
    num_cols = vfs_split_text (p_copy);

    s->st_nlink = atol (columns [0]);
    if (s->st_nlink <= 0)
        goto error;

    if (!is_num (1))
	s->st_uid = finduid (columns [1]);
    else
        s->st_uid = (uid_t) atol (columns [1]);

    /* Mhm, the ls -lg did not produce a group field */
    for (idx = 3; idx <= 5; idx++) 
        if (is_month(columns [idx], NULL) || is_week(columns [idx], NULL) || is_dos_date(columns[idx]))
            break;

    if (idx == 6 || (idx == 5 && !S_ISCHR (s->st_mode) && !S_ISBLK (s->st_mode)))
	goto error;

    /* We don't have gid */	
    if (idx == 3 || (idx == 4 && (S_ISCHR(s->st_mode) || S_ISBLK (s->st_mode))))
        idx2 = 2;
    else { 
	/* We have gid field */
	if (is_num (2))
	    s->st_gid = (gid_t) atol (columns [2]);
	else
	    s->st_gid = findgid (columns [2]);
	idx2 = 3;
    }

    /* This is device */
    if (S_ISCHR (s->st_mode) || S_ISBLK (s->st_mode)){
	int maj, min;
	
	if (!is_num (idx2) || sscanf(columns [idx2], " %d,", &maj) != 1)
	    goto error;
	
	if (!is_num (++idx2) || sscanf(columns [idx2], " %d", &min) != 1)
	    goto error;
	
#ifdef HAVE_ST_RDEV
	s->st_rdev = ((maj & 0xff) << 8) | (min & 0xffff00ff);
#endif
	s->st_size = 0;
	
    } else {
	/* Common file size */
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
    /* s->st_dev and s->st_ino must be initialized by vfs_s_new_inode () */
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
	    
	if (filename){
	    *filename = g_strndup (p + column_ptr [idx], column_ptr [idx2] - column_ptr [idx] - 1);
	}
	if (linkname){
	    t = g_strdup (p + column_ptr [idx2+1]);
	    *linkname = t;
	}
    } else {
	/* Extract the filename from the string copy, not from the columns
	 * this way we have a chance of entering hidden directories like ". ."
	 */
	if (filename){
	    /* 
	    *filename = g_strdup (columns [idx++]);
	    */

	    t = g_strdup (p + column_ptr [idx]);
	    *filename = t;
	}
	if (linkname)
	    *linkname = NULL;
    }

    if (t) {
	int p = strlen (t);
	if ((--p > 0) && (t [p] == '\r' || t [p] == '\n'))
	    t [p] = 0;
	if ((--p > 0) && (t [p] == '\r' || t [p] == '\n'))
	    t [p] = 0;
    }

    g_free (p_copy);
    return 1;

error:
    {
      static int errorcount = 0;

      if (++errorcount < 5) {
	message_1s (1, _("Cannot parse:"), (p_copy && *p_copy) ? p_copy : line);
      } else if (errorcount == 5)
	message_1s (1, _("Error"), _("More parsing errors will be ignored."));
    }

    g_free (p_copy);
    return 0;
}

void
vfs_die (const char *m)
{
    message_1s (1, _("Internal error:"), m);
    exit (1);
}

void
vfs_print_stats (const char *fs_name, const char *action, const char *file_name, off_t have, off_t need)
{
    static char *i18n_percent_transf_format = NULL, *i18n_transf_format = NULL;

    if (i18n_percent_transf_format == NULL) {
	i18n_percent_transf_format = _("%s: %s: %s %3d%% (%lu bytes transferred)");
	i18n_transf_format = _("%s: %s: %s %lu bytes transferred");
    }

    if (need) 
	print_vfs_message (i18n_percent_transf_format, fs_name, action,
			   file_name, (int)((double)have*100/need), (unsigned long) have);
    else
	print_vfs_message (i18n_transf_format,
			   fs_name, action, file_name, (unsigned long) have);
}

char *
vfs_get_password (char *msg)
{
    return (char *) input_dialog (msg, _("Password:"), INPUT_PASSWORD);
}

/*
 * Returns vfs path corresponding to given url. If passed string is
 * not recognized as url, g_strdup(url) is returned.
 */
char *
vfs_translate_url (char *url)
{
    if (strncmp (url, "ftp://", 6) == 0)
        return  g_strconcat ("/#ftp:", url + 6, NULL);
    else if (strncmp (url, "a:", 2) == 0)
        return  g_strdup ("/#a");
    else
        return g_strdup (url);
}
