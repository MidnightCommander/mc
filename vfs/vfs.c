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

#include <stdio.h>
#include <stdlib.h>	/* For atol() */
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>
#include <ctype.h>	/* is_digit() */

#include "utilvfs.h"
#include "gc.h"

#include "vfs.h"
#ifdef USE_NETCODE
#   include "tcputil.h"
#endif

/* They keep track of the current directory */
static struct vfs_class *current_vfs;
static char *current_dir;

struct vfs_openfile {
    int handle;
    struct vfs_class *vclass;
    void *fsinfo;
};

static GSList *vfs_openfiles;
#define VFS_FIRST_HANDLE 100

static struct vfs_class *localfs_class;

/* Create new VFS handle and put it to the list */
static int
vfs_new_handle (struct vfs_class *vclass, void *fsinfo)
{
    static int vfs_handle_counter = VFS_FIRST_HANDLE;
    struct vfs_openfile *h;

    h = g_new (struct vfs_openfile, 1);
    h->handle = vfs_handle_counter++;
    h->fsinfo = fsinfo;
    h->vclass = vclass;
    vfs_openfiles = g_slist_prepend (vfs_openfiles, h);
    return h->handle;
}

/* Function to match handle, passed to g_slist_find_custom() */
static gint
vfs_cmp_handle (gconstpointer a, gconstpointer b)
{
    if (!a)
	return 1;
    return ((struct vfs_openfile *) a)->handle != (int) b;
}

/* Find VFS class by file handle */
static inline struct vfs_class *
vfs_op (int handle)
{
    GSList *l;
    struct vfs_openfile *h;

    l = g_slist_find_custom (vfs_openfiles, (void *) handle,
			     vfs_cmp_handle);
    if (!l)
	return NULL;
    h = (struct vfs_openfile *) l->data;
    if (!h)
	return NULL;
    return h->vclass;
}

/* Find private file data by file handle */
static inline void *
vfs_info (int handle)
{
    GSList *l;
    struct vfs_openfile *h;

    l = g_slist_find_custom (vfs_openfiles, (void *) handle,
			     vfs_cmp_handle);
    if (!l)
	return NULL;
    h = (struct vfs_openfile *) l->data;
    if (!h)
	return NULL;
    return h->fsinfo;
}

/* Free open file data for given file handle */
static inline void
vfs_free_handle (int handle)
{
    GSList *l;

    l = g_slist_find_custom (vfs_openfiles, (void *) handle,
			     vfs_cmp_handle);
    vfs_openfiles = g_slist_delete_link (vfs_openfiles, l);
}

static struct vfs_class *vfs_list;

int
vfs_register_class (struct vfs_class *vfs)
{
    if (vfs->init)		/* vfs has own initialization function */
	if (!(*vfs->init)(vfs))	/* but it failed */
	    return 0;

    vfs->next = vfs_list;
    vfs_list = vfs;

    return 1;
}

/* Return VFS class for the given prefix */
static struct vfs_class *
vfs_prefix_to_class (char *prefix)
{
    struct vfs_class *vfs;

    for (vfs = vfs_list; vfs; vfs = vfs->next) {
	if (vfs->which) {
	    if ((*vfs->which) (vfs, prefix) == -1)
		continue;
	    return vfs;
	}
	if (vfs->prefix
	    && !strncmp (prefix, vfs->prefix, strlen (vfs->prefix)))
	    return vfs;
    }
    return NULL;
}

/* Strip known vfs suffixes from a filename (possible improvement: strip
   suffix from last path component). 
   Returns a malloced string which has to be freed. */
char *
vfs_strip_suffix_from_filename (const char *filename)
{
    struct vfs_class *vfs;
    char *semi;
    char *p;

    if (!filename)
	vfs_die ("vfs_strip_suffix_from_path got NULL: impossible");

    p = g_strdup (filename);
    if (!(semi = strrchr (p, '#')))
	return p;

    /* Avoid last class (localfs) that would accept any prefix */
    for (vfs = vfs_list; vfs->next; vfs = vfs->next) {
	if (vfs->which) {
	    if ((*vfs->which) (vfs, semi + 1) == -1)
		continue;
	    *semi = '\0';	/* Found valid suffix */
	    return p;
	}
	if (vfs->prefix
	    && !strncmp (semi + 1, vfs->prefix, strlen (vfs->prefix))) {
	    *semi = '\0';	/* Found valid suffix */
	    return p;
	}
    }
    return p;
}

static int
path_magic (const char *path)
{
    struct stat buf;

    if (!stat(path, &buf))
        return 0;

    return 1;
}

/*
 * Splits path '/p1#op/inpath' into inpath,op; returns which vfs it is.
 * What is left in path is p1. You still want to g_free(path), you DON'T
 * want to free neither *inpath nor *op
 */
struct vfs_class *
vfs_split (char *path, char **inpath, char **op)
{
    char *semi;
    char *slash;
    struct vfs_class *ret;

    if (!path)
	vfs_die("Cannot split NULL");
    
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

    if ((ret = vfs_prefix_to_class (semi+1))){
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

static struct vfs_class *
_vfs_get_class (const char *path)
{
    char *semi;
    char *slash;
    struct vfs_class *ret;

    g_return_val_if_fail(path, NULL);

    semi = strrchr (path, '#');
    if (!semi || !path_magic (path))
	return NULL;
    
    slash = strchr (semi, PATH_SEP);
    *semi = 0;
    if (slash)
	*slash = 0;
    
    ret = vfs_prefix_to_class (semi+1);

    if (slash)
	*slash = PATH_SEP;
    if (!ret)
	ret = _vfs_get_class (path);

    *semi = '#';
    return ret;
}

struct vfs_class *
vfs_get_class (const char *path)
{
    struct vfs_class *vfs;

    vfs = _vfs_get_class(path);

    if (!vfs)
	vfs = localfs_class;

    return vfs;
}

static int
ferrno (struct vfs_class *vfs)
{
    return vfs->ferrno ? (*vfs->ferrno)(vfs) : E_UNKNOWN; 
    /* Hope that error message is obscure enough ;-) */
}

int
mc_open (const char *filename, int flags, ...)
{
    int  mode;
    void *info;
    va_list ap;

    char *file = vfs_canon (filename);
    struct vfs_class *vfs = vfs_get_class (file);

    /* Get the mode flag */	/* FIXME: should look if O_CREAT is present */
    va_start (ap, flags);
    mode = va_arg (ap, int);
    va_end (ap);
    
    if (!vfs->open) {
	g_free (file);
	errno = -EOPNOTSUPP;
	return -1;
    }

    info = (*vfs->open) (vfs, file, flags, mode);	/* open must be supported */
    g_free (file);
    if (!info){
	errno = ferrno (vfs);
	return -1;
    }

    return vfs_new_handle (vfs, info);
}


#define MC_NAMEOP(name, inarg, callarg) \
int mc_##name inarg \
{ \
    struct vfs_class *vfs; \
    int result; \
    path = vfs_canon (path); \
    vfs = vfs_get_class (path); \
    result = vfs->name ? (*vfs->name)callarg : -1; \
    g_free (path); \
    if (result == -1) \
	errno = vfs->name ? ferrno (vfs) : E_NOTSUPP; \
    return result; \
}

MC_NAMEOP (chmod, (char *path, int mode), (vfs, path, mode))
MC_NAMEOP (chown, (char *path, int owner, int group), (vfs, path, owner, group))
MC_NAMEOP (utime, (char *path, struct utimbuf *times), (vfs, path, times))
MC_NAMEOP (readlink, (char *path, char *buf, int bufsiz), (vfs, path, buf, bufsiz))
MC_NAMEOP (unlink, (char *path), (vfs, path))
MC_NAMEOP (symlink, (char *name1, char *path), (vfs, name1, path))
MC_NAMEOP (mkdir, (char *path, mode_t mode), (vfs, path, mode))
MC_NAMEOP (rmdir, (char *path), (vfs, path))
MC_NAMEOP (mknod, (char *path, int mode, int dev), (vfs, path, mode, dev))


#define MC_HANDLEOP(name, inarg, callarg) \
int mc_##name inarg \
{ \
    struct vfs_class *vfs; \
    int result; \
    if (handle == -1) \
	return -1; \
    vfs = vfs_op (handle); \
    result = vfs->name ? (*vfs->name)callarg : -1; \
    if (result == -1) \
	errno = vfs->name ? ferrno (vfs) : E_NOTSUPP; \
    return result; \
}

MC_HANDLEOP(read, (int handle, char *buffer, int count), (vfs_info (handle), buffer, count) )
MC_HANDLEOP (write, (int handle, char *buf, int nbyte), (vfs_info (handle), buf, nbyte))


#define MC_RENAMEOP(name) \
int mc_##name (const char *fname1, const char *fname2) \
{ \
    struct vfs_class *vfs; \
    int result; \
    char *name2, *name1 = vfs_canon (fname1); \
    vfs = vfs_get_class (name1); \
    name2 = vfs_canon (fname2); \
    if (vfs != vfs_get_class (name2)){ \
    	errno = EXDEV; \
    	g_free (name1); \
    	g_free (name2); \
	return -1; \
    } \
    result = vfs->name ? (*vfs->name)(vfs, name1, name2) : -1; \
    g_free (name1); \
    g_free (name2); \
    if (result == -1) \
        errno = vfs->name ? ferrno (vfs) : E_NOTSUPP; \
    return result; \
}

MC_RENAMEOP (link)
MC_RENAMEOP (rename)


int
mc_ctl (int handle, int ctlop, void *arg)
{
    struct vfs_class *vfs = vfs_op (handle);

    return vfs->ctl ? (*vfs->ctl)(vfs_info (handle), ctlop, arg) : 0;
}

int
mc_setctl (char *path, int ctlop, void *arg)
{
    struct vfs_class *vfs;
    int result;

    if (!path)
	vfs_die("You don't want to pass NULL to mc_setctl.");
    
    path = vfs_canon (path);
    vfs = vfs_get_class (path);    
    result = vfs->setctl ? (*vfs->setctl)(vfs, path, ctlop, arg) : 0;
    g_free (path);
    return result;
}

int
mc_close (int handle)
{
    struct vfs_class *vfs;
    int result;

    if (handle == -1 || !vfs_info (handle))
	return -1;
    
    vfs = vfs_op (handle);
    if (handle < 3)
	return close (handle);

    if (!vfs->close)
        vfs_die ("VFS must support close.\n");
    result = (*vfs->close)(vfs_info (handle));
    vfs_free_handle (handle);
    if (result == -1)
	errno = ferrno (vfs);
    
    return result;
}

DIR *
mc_opendir (const char *dirname)
{
    int  handle, *handlep;
    void *info;
    struct vfs_class *vfs;
    char *dname;

    dname = vfs_canon (dirname);
    vfs = vfs_get_class (dname);

    info = vfs->opendir ? (*vfs->opendir)(vfs, dname) : NULL;
    g_free (dname);
    if (!info){
        errno = vfs->opendir ? ferrno (vfs) : E_NOTSUPP;
	return NULL;
    }
    handle = vfs_new_handle (vfs, info);

    handlep = g_new (int, 1);
    *handlep = handle;
    return (DIR *) handlep;
}

struct dirent *
mc_readdir (DIR *dirp)
{
    int handle;
    struct vfs_class *vfs;
    struct dirent *result = NULL;

    if (!dirp) {
	errno = EFAULT;
	return NULL;
    }
    handle = *(int *) dirp;
    vfs = vfs_op (handle);
    if (vfs->readdir)
	result = (*vfs->readdir) (vfs_info (handle));
    if (!result)
	errno = vfs->readdir ? ferrno (vfs) : E_NOTSUPP;
    return result;
}

int
mc_closedir (DIR *dirp)
{
    int handle = *(int *) dirp;
    struct vfs_class *vfs = vfs_op (handle);
    int result;

    result = vfs->closedir ? (*vfs->closedir)(vfs_info (handle)) : -1;
    vfs_free_handle (handle);
    g_free (dirp);
    return result; 
}

int mc_stat (const char *filename, struct stat *buf) {
    struct vfs_class *vfs;
    int result;
    char *path;
    path = vfs_canon (filename); vfs = vfs_get_class (path);
    result = vfs->stat ? (*vfs->stat) (vfs, path, buf) : -1;
    g_free (path);
    if (result == -1)
	errno = vfs->name ? ferrno (vfs) : E_NOTSUPP;
    return result;
}

int mc_lstat (const char *filename, struct stat *buf) {
    struct vfs_class *vfs;
    int result;
    char *path;
    path = vfs_canon (filename); vfs = vfs_get_class (path);
    result = vfs->lstat ? (*vfs->lstat) (vfs, path, buf) : -1;
    g_free (path);
    if (result == -1)
	errno = vfs->name ? ferrno (vfs) : E_NOTSUPP;
    return result;
}

int mc_fstat (int handle, struct stat *buf) {
    struct vfs_class *vfs;
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
 * Return current directory.  If it's local, reread the current directory
 * from the OS.  You must g_strdup whatever this function returns.
 */
static const char *
_vfs_get_cwd (void)
{
    char *p;
    struct stat my_stat, my_stat2;

    if (!_vfs_get_class (current_dir)) {
	p = g_get_current_dir ();
	if (!p)			/* One of the directories in the path is not readable */
	    return current_dir;

	/* Otherwise check if it is O.K. to use the current_dir */
	if (!cd_symlinks || mc_stat (p, &my_stat)
	    || mc_stat (current_dir, &my_stat2)
	    || my_stat.st_ino != my_stat2.st_ino
	    || my_stat.st_dev != my_stat2.st_dev) {
	    g_free (current_dir);
	    current_dir = p;
	    return p;
	}			/* Otherwise we return current_dir below */
	g_free (p);
    }
    return current_dir;
}

static void
vfs_setup_wd (void)
{
    current_dir = g_strdup (PATH_SEP_STR);
    _vfs_get_cwd ();

    if (strlen (current_dir) > MC_MAXPATHLEN - 2)
	vfs_die ("Current dir too long.\n");

    current_vfs = vfs_get_class (current_dir);
}

/*
 * Return current directory.  If it's local, reread the current directory
 * from the OS.  Put directory to the provided buffer.
 */
char *
mc_get_current_wd (char *buffer, int size)
{
    const char *cwd = _vfs_get_cwd ();

    g_strlcpy (buffer, cwd, size - 1);
    buffer[size - 1] = 0;
    return buffer;
}

/*
 * Return current directory without any OS calls.
 */
char *
vfs_get_current_dir (void)
{
    return current_dir;
}

off_t mc_lseek (int fd, off_t offset, int whence)
{
    struct vfs_class *vfs;
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

/*
 * VFS chdir.
 * Return 0 on success, -1 on failure.
 */
int
mc_chdir (const char *path)
{
    char *new_dir;
    struct vfs_class *old_vfs, *new_vfs;
    vfsid old_vfsid;
    int result;

    new_dir = vfs_canon (path);
    new_vfs = vfs_get_class (new_dir);
    if (!new_vfs->chdir)
	return -1;

    result = (*new_vfs->chdir) (new_vfs, new_dir);

    if (result == -1) {
	errno = ferrno (new_vfs);
	g_free (new_dir);
	return -1;
    }

    old_vfsid = vfs_getid (current_vfs, current_dir);
    old_vfs = current_vfs;

    /* Actually change directory */
    g_free (current_dir);
    current_dir = new_dir;
    current_vfs = new_vfs;

    /* This function uses the new current_dir implicitly */
    vfs_stamp_create (old_vfs, old_vfsid);

    /* Sometimes we assume no trailing slash on cwd */
    if (*current_dir) {
	char *p;
	p = strchr (current_dir, 0) - 1;
	if (*p == PATH_SEP && p > current_dir)
	    *p = 0;
    }

    return 0;
}

/* Return 1 is the current VFS class is local */
int
vfs_current_is_local (void)
{
    return (current_vfs->flags & VFSF_LOCAL) != 0;
}

/* Return flags of the VFS class of the given filename */
int
vfs_file_class_flags (const char *filename)
{
    struct vfs_class *vfs;
    char *fname;

    fname = vfs_canon (filename);
    vfs = vfs_get_class (fname);
    g_free (fname);
    return vfs->flags;
}

#ifdef HAVE_MMAP
static struct mc_mmapping {
    caddr_t addr;
    void *vfs_info;
    struct vfs_class *vfs;
    struct mc_mmapping *next;
} *mc_mmaparray = NULL;

caddr_t
mc_mmap (caddr_t addr, size_t len, int prot, int flags, int fd, off_t offset)
{
    struct vfs_class *vfs;
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

static char *
mc_def_getlocalcopy (struct vfs_class *vfs, const char *filename)
{
    char *tmp;
    int fdin, fdout, i;
    char buffer[8192];
    struct stat mystat;

    fdin = mc_open (filename, O_RDONLY | O_LINEAR);
    if (fdin == -1)
	return NULL;

    fdout = vfs_mkstemps (&tmp, "vfs", filename);

    if (fdout == -1)
	goto fail;
    while ((i = mc_read (fdin, buffer, sizeof (buffer))) > 0) {
	if (write (fdout, buffer, i) != i)
	    goto fail;
    }
    if (i == -1)
	goto fail;
    i = mc_close (fdin);
    fdin = -1;
    if (i == -1)
	goto fail;
    if (close (fdout) == -1)
	goto fail;

    if (mc_stat (filename, &mystat) != -1) {
	chmod (tmp, mystat.st_mode);
    }
    return tmp;

  fail:
    if (fdout)
	close (fdout);
    if (fdin)
	mc_close (fdin);
    g_free (tmp);
    return NULL;
}

char *
mc_getlocalcopy (const char *pathname)
{
    char *result;
    char *path = vfs_canon (pathname);
    struct vfs_class *vfs = vfs_get_class (path);    

    result = vfs->getlocalcopy ? (*vfs->getlocalcopy)(vfs, path) :
                                 mc_def_getlocalcopy (vfs, path);
    g_free (path);
    if (!result)
	errno = ferrno (vfs);
    return result;
}

static int
mc_def_ungetlocalcopy (struct vfs_class *vfs, const char *filename,
		       const char *local, int has_changed)
{
    int fdin = -1, fdout = -1, i;
    if (has_changed) {
	char buffer[8192];

	if (!vfs->write)
	    goto failed;

	fdin = open (local, O_RDONLY);
	if (fdin == -1)
	    goto failed;
	fdout = mc_open (filename, O_WRONLY | O_TRUNC);
	if (fdout == -1)
	    goto failed;
	while ((i = read (fdin, buffer, sizeof (buffer))) > 0) {
	    if (mc_write (fdout, buffer, i) != i)
		goto failed;
	}
	if (i == -1)
	    goto failed;

	if (close (fdin) == -1) {
	    fdin = -1;
	    goto failed;
	}
	fdin = -1;
	if (mc_close (fdout) == -1) {
	    fdout = -1;
	    goto failed;
	}
    }
    unlink (local);
    return 0;

  failed:
    message (1, _("Changes to file lost"), filename);
    if (fdout != -1)
	mc_close (fdout);
    if (fdin != -1)
	close (fdin);
    unlink (local);
    return -1;
}

int
mc_ungetlocalcopy (const char *pathname, char *local, int has_changed)
{
    int return_value = 0;
    char *path = vfs_canon (pathname);
    struct vfs_class *vfs = vfs_get_class (path);

    return_value = vfs->ungetlocalcopy ? 
            (*vfs->ungetlocalcopy)(vfs, path, local, has_changed) :
            mc_def_ungetlocalcopy (vfs, path, local, has_changed);
    g_free (path);
    g_free (local);
    return return_value;
}


void
vfs_init (void)
{
    /* localfs needs to be the first one */
    init_localfs();
    /* fallback value for vfs_get_class() */
    localfs_class = vfs_list;

    init_extfs ();
    init_sfs ();
    init_tarfs ();
    init_cpiofs ();

#ifdef USE_EXT2FSLIB
    init_undelfs ();
#endif /* USE_EXT2FSLIB */

#ifdef USE_NETCODE
    tcp_init();
    init_ftpfs ();
    init_fish ();
#ifdef WITH_SMBFS
    init_smbfs ();
#endif /* WITH_SMBFS */
#ifdef WITH_MCFS
    init_mcfs ();
#endif /* WITH_SMBFS */
#endif /* USE_NETCODE */

    vfs_setup_wd ();
}

void
vfs_shut (void)
{
    struct vfs_class *vfs;

    vfs_gc_done ();

    if (current_dir)
	g_free (current_dir);

    for (vfs = vfs_list; vfs; vfs = vfs->next)
	if (vfs->done)
	    (*vfs->done) (vfs);

    g_slist_free (vfs_openfiles);
}

/*
 * These ones grab information from the VFS
 *  and handles them to an upper layer
 */
void
vfs_fill_names (void (*func)(char *))
{
    struct vfs_class *vfs;

    for (vfs=vfs_list; vfs; vfs=vfs->next)
        if (vfs->fill_names)
	    (*vfs->fill_names) (vfs, func);
}

/*
 * Returns vfs path corresponding to given url. If passed string is
 * not recognized as url, g_strdup(url) is returned.
 */
char *
vfs_translate_url (const char *url)
{
    if (strncmp (url, "ftp://", 6) == 0)
	return g_strconcat ("/#ftp:", url + 6, NULL);
    else if (strncmp (url, "a:", 2) == 0)
	return g_strdup ("/#a");
    else
	return g_strdup (url);
}
