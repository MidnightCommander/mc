/* Directory cache support -- so that you do not have copy of this in
 * each and every filesystem.
 *
 * Written at 1998 by Pavel Machek <pavel@ucw.cz>, distribute under LGPL.
 *
 * Very loosely based on tar.c from midnight and archives.[ch] from
 * avfs by Miklos Szeredi (mszeredi@inf.bme.hu)
 *
 * Unfortunately, I was unable to keep all filesystems
 * uniform. tar-like filesystems use tree structure where each
 * directory has pointers to its subdirectories. We can do this
 * because we have full information about our archive.
 *
 * At ftp-like filesystems, situation is a little bit different. When
 * you cd /usr/src/linux/drivers/char, you do _not_ want /usr,
 * /usr/src, /usr/src/linux and /usr/src/linux/drivers to be
 * listed. That means that we do not have complete information, and if
 * /usr is symlink to /4, we will not know. Also we have to time out
 * entries and things would get messy with tree-like approach. So we
 * do different trick: root directory is completely special and
 * completely fake, it contains entries such as 'usr', 'usr/src', ...,
 * and we'll try to use custom find_entry function. 
 *
 * Paths here do _not_ begin with '/', so root directory of
 * archive/site is simply "". Beware. */

static volatile int total_inodes = 0, total_entries = 0;

#include "xdirentry.h"

#define CALL(x) if (MEDATA->x) MEDATA->x

vfs_s_inode *vfs_s_new_inode (vfs *me, vfs_s_super *super, struct stat *initstat)
{
    vfs_s_inode *ino;

    ino = xmalloc(sizeof (vfs_s_inode), "Dcache inode");
    if (!ino) return NULL;


    ino->linkname = ino->localname = NULL;
    ino->subdir = NULL;
    if (initstat)
        ino->st = *initstat;
    ino->super = super;
    ino->ent = NULL;
    ino->flags = 0;
    ino->st.st_nlink = 0;
    ino->st.st_ino = MEDATA->inode_counter++;
    ino->st.st_dev = MEDATA->rdev;

    super->fd_usage++;
    total_inodes++;
    
    CALL(init_inode) (me, ino);

    return ino;
}

vfs_s_entry *vfs_s_new_entry (vfs *me, char *name, vfs_s_inode *inode)
{
    vfs_s_entry *entry;

    entry = (struct vfs_s_entry *) xmalloc (sizeof (struct vfs_s_entry), "Dcache entry");
    total_entries++;

    if (name)
	entry->name = strdup (name);
    else entry->name = NULL;
    entry->dir = NULL;
    entry->next = NULL;
    entry->prevp = NULL;
    entry->ino = inode;
    entry->ino->ent = entry;
    CALL(init_entry) (me, entry);

    return entry;
}

void vfs_s_free_inode (vfs *me, vfs_s_inode *ino)
{
    if (!ino) vfs_die("Don't pass NULL to me");

    /* ==0 can happen if freshly created entry is deleted */
    if(ino->st.st_nlink <= 1) {
	while(ino->subdir) {
	    vfs_s_entry *ent;
	    ent = ino->subdir;
	    vfs_s_free_entry(me, ent);
	}

	CALL(free_inode) (me, ino);
	ifree(ino->linkname);
	if (ino->localname) {
	    unlink(ino->localname);
	    free(ino->localname);
	}
	total_inodes--;
	ino->super->fd_usage--;
	free(ino);
    } else ino->st.st_nlink--;
}

void vfs_s_free_entry (vfs *me, vfs_s_entry *ent)
{
    int is_dot = 0;
    if (ent->prevp) {	/* It is possible that we are deleting freshly created entry */
	*ent->prevp = ent->next;
	if (ent->next) ent->next->prevp = ent->prevp;
    }

    if (ent->name) {
	is_dot = (!strcmp(ent->name, ".")) || (!strcmp(ent->name, ".."));
	free (ent->name);
	ent->name = NULL;
    }
	
    if (!is_dot && ent->ino) {
	ent->ino->ent = NULL;
	vfs_s_free_inode (me, ent->ino);
	ent->ino = NULL;
    }

    total_entries--;
    free(ent);
}

void vfs_s_insert_entry (vfs *me, vfs_s_inode *dir, vfs_s_entry *ent)
{
    vfs_s_entry **ep;

    for(ep = &dir->subdir; *ep != NULL; ep = &((*ep)->next));
    ent->prevp = ep;
    ent->next = NULL;
    ent->dir = dir;
    *ep = ent;

    ent->ino->st.st_nlink++;
}

struct stat *vfs_s_default_stat (vfs *me, mode_t mode)
{
    static struct stat st;
    int myumask;

    myumask = umask (022);
    umask (myumask);
    mode &= ~myumask;

    st.st_mode = mode;
    st.st_ino = 0;
    st.st_dev = 0;
    st.st_rdev = 0;
    st.st_uid = getuid ();
    st.st_gid = getgid ();
    st.st_size = 0;
    st.st_mtime = st.st_atime = st.st_ctime = time (NULL);

    return &st;
}

void vfs_s_add_dots (vfs *me, vfs_s_inode *dir, vfs_s_inode *parent)
{
    struct vfs_s_entry *dot, *dotdot;

    if (!parent)
        parent = dir;
    dot = vfs_s_new_entry (me, ".", dir);
    dotdot = vfs_s_new_entry (me, "..", parent);
    vfs_s_insert_entry(me, dir, dot);
    vfs_s_insert_entry(me, dir, dotdot);
    dir->st.st_nlink--;	parent->st.st_nlink--; /* We do not count "." and ".." into nlinks */
}

struct vfs_s_entry *vfs_s_generate_entry (vfs *me, char *name, struct vfs_s_inode *parent, mode_t mode)
{
    struct vfs_s_inode *inode;
    struct vfs_s_entry *entry;
    struct stat *st;

    st = vfs_s_default_stat (me, mode);
    inode = vfs_s_new_inode (me, parent->super, st);
    if (S_ISDIR (mode))
        vfs_s_add_dots (me, inode, parent);

    entry = vfs_s_new_entry (me, name, inode);

    return entry;
}

/* We were asked to create entries automagically */
vfs_s_entry *vfs_s_automake(vfs *me, vfs_s_inode *dir, char *path, int flags)
{
    struct vfs_s_entry *res;
    char *sep = strchr( path, DIR_SEP_CHAR );
    if (sep) *sep = 0;
    res = vfs_s_generate_entry(me, path, dir, flags & FL_MKDIR ? (0777 | S_IFDIR) : 0777 );
    vfs_s_insert_entry(me, dir, res);
    if (sep) *sep = DIR_SEP_CHAR;
    return res;
}

/* Follow > 0: follow links, serves as loop protect,
 *       == -1: do not follow links */
vfs_s_entry *vfs_s_find_entry_tree(vfs *me, vfs_s_inode *root, char *path, int follow, int flags)
{
    unsigned int pseg;
    vfs_s_entry* ent = NULL;
    int found;

    while(1) {
	for(pseg = 0; path[pseg] == DIR_SEP_CHAR; pseg++);
	if(!path[pseg]) return ent;
	path += pseg;

	for(pseg = 0; path[pseg] && path[pseg] != DIR_SEP_CHAR; pseg++);

	found = 0;
	for(ent = root->subdir; ent != NULL; ent = ent->next)
	    if(strlen(ent->name) == pseg && (!strncmp(ent->name, path, pseg)))
		/* FOUND! */
		break;

	if (!ent && (flags & (FL_MKFILE | FL_MKDIR)))
	    ent = vfs_s_automake(me, root, path, flags);
	if (!ent) ERRNOR (ENOENT, NULL);
	path += pseg;
	if (!vfs_s_resolve_symlink(me, ent, follow)) return NULL;
	root = ent->ino;
    }
}

static void split_dir_name(vfs *me, char *path, char **dir, char **name, char **save)
{
    char *s;
    s = strrchr(path, DIR_SEP_CHAR);
    if (!s) {
	*save = NULL;
	*name = path;
	*dir = "";
    } else {
	*save = s;
	*dir = path;
	*s++ = 0;
	if (!*s)	/* This can happen if someone does stat("/"); */
	    *name = "";
	else
	    *name = s;
    }
}

vfs_s_entry *vfs_s_find_entry_linear(vfs *me, vfs_s_inode *root, char *path, int follow, int flags)
{
    char *s;
    vfs_s_entry* ent = NULL;

    if (!(flags & FL_DIR)) {
	char *dirname, *name, *save;
	vfs_s_inode *ino;
	vfs_s_entry *ent;
	split_dir_name(me, path, &dirname, &name, &save);
	ino = vfs_s_find_inode(me, root, dirname, follow, flags | FL_DIR);
	if (save)
	    *save = DIR_SEP_CHAR;
	ent = vfs_s_find_entry_tree(me, ino, name, follow, flags);
	return ent;
    }

    for(ent = root->subdir; ent != NULL; ent = ent->next)
	if (!strcmp(ent->name, path))
	    break;

    if (ent && (! (MEDATA->dir_uptodate) (me, ent->ino))) {
#if 1
	message_1s( 1, "Dir cache expired for", path);
#endif
	vfs_s_free_entry (me, ent);
    }

    if (!ent) {
	vfs_s_inode *ino;

	ino = vfs_s_new_inode(me, root->super, vfs_s_default_stat (me, S_IFDIR | 0755));
	ent = vfs_s_new_entry(me, path, ino);
	if ((MEDATA->dir_load) (me, ino, path) == -1) {
	    vfs_s_free_entry(me, ent);
	    return NULL;
	}
	vfs_s_insert_entry(me, root, ent);

	for(ent = root->subdir; ent != NULL; ent = ent->next)
	    if (!strcmp(ent->name, path))
		break;
    }
    if (!ent) 
	vfs_die("find_linear: success but directory is not there\n");

#if 0
    if (!vfs_s_resolve_symlink(me, ent, follow)) return NULL;
#endif
    return ent;
}

vfs_s_inode *vfs_s_find_inode(vfs *me, vfs_s_inode *root, char *path, int follow, int flags)
{
    vfs_s_entry *ent;
    if ((MEDATA->find_entry == vfs_s_find_entry_tree) && (!*path))
	return root;
    ent = (MEDATA->find_entry)(me, root, path, follow, flags);
    if (!ent)
	return NULL;
    return ent->ino;
}

vfs_s_inode *vfs_s_find_root(vfs *me, vfs_s_entry *entry)
{
    vfs_die("Implement me");
    return NULL;
}

vfs_s_entry *vfs_s_resolve_symlink (vfs *me, vfs_s_entry *entry, int follow)
{
    vfs_s_inode *dir;
    if (follow == LINK_NO_FOLLOW)
	return entry;
    if (follow == 0)
	ERRNOR (ELOOP, NULL);
    if (!entry)
	ERRNOR (ENOENT, NULL);
    if (!S_ISLNK(entry->ino->st.st_mode))
	return entry;

    /* We have to handle "/" by ourself; "." and ".." have
       corresponding entries, so there's no problem with them.
       FIXME: no longer true */
    if (*entry->ino->linkname == '/') dir = vfs_s_find_root(me, entry); 
    else dir = entry->dir;
    return (MEDATA->find_entry) (me, dir, entry->ino->linkname, follow-1, 0);
}

/* Ook, these were functions around direcory entries / inodes */
/* -------------------------------- superblock games -------------------------- */

vfs_s_super *vfs_s_new_super (vfs *me)
{
    vfs_s_super *super;

    super = xmalloc( sizeof( struct vfs_s_super ), "Direntry: superblock" );
    bzero(super, sizeof(struct vfs_s_super));
    super->root = NULL;
    super->name = NULL;
    super->fd_usage = 0;
    super->me = me;
    return super;
}

void vfs_s_insert_super (vfs *me, vfs_s_super *super)
{
    super->next = MEDATA->supers;
    super->prevp = &MEDATA->supers;

    if (MEDATA->supers != NULL) MEDATA->supers->prevp = &super->next;
    MEDATA->supers = super;
} 

void vfs_s_free_super (vfs *me, vfs_s_super *super)
{
    if (super->root) {
	vfs_s_free_inode (me, super->root);
	super->root = NULL;
    }

#if 1
    /* We currently leak small ammount of memory, sometimes. Fix it if you can. */
    if (super->fd_usage)
	message_1s1d (1, " Direntry warning ", "Super fd_usage is %d, memory leak", super->fd_usage);

    if (super->want_stale)
	message_1s( 1, " Direntry warning ", "Super has want_stale set" );
#endif

    if (super->prevp) {
	*super->prevp = super->next;
	if (super->next) super->next->prevp = super->prevp;
    }

    CALL(free_archive) (me, super);
    ifree(super->name);
    super->name = NULL;
    free(super);
}

/* ------------------------------------------------------------------------= */

static void vfs_s_stamp_me (vfs *me, struct vfs_s_super *psup, char *fs_name)
{
    struct vfs_stamping *parent;
    vfs *v;
 
    v = vfs_type (fs_name);
    if (v == &vfs_local_ops) {
	parent = NULL;
    } else {
	parent = xmalloc (sizeof (struct vfs_stamping), "vfs stamping");
	parent->v = v;
	parent->next = 0;
	parent->id = (*v->getid) (v, fs_name, &(parent->parent));
    }
    vfs_add_noncurrent_stamps (&vfs_tarfs_ops, (vfsid) psup, parent);
    vfs_rm_parents (parent);
}

char *vfs_s_get_path_mangle (vfs *me, char *inname, struct vfs_s_super **archive, int flags)
{
    char *local, *op, *archive_name;
    int result = -1;
    struct vfs_s_super *super;
    void *cookie;
    
    archive_name = inname;
    vfs_split( inname, &local, &op );
    if (!local)
        local = "";

    if (MEDATA->archive_check)
	if (! (cookie = MEDATA->archive_check (me, archive_name, op)))
	    return NULL;

    for (super = MEDATA->supers; super != NULL; super = super->next) {
	int i; /* 0 == other, 1 == same, return it, 2 == other but stop scanning */
	if ((i = MEDATA->archive_same (me, super, archive_name, op, cookie))) {
	    if (i==1) goto return_success;
	    else break;
	}
    }

    if (flags & FL_NO_OPEN) ERRNOR (EIO, NULL);

    super = vfs_s_new_super (me);
    result = MEDATA->open_archive (me, super, archive_name, op);
    if (result == -1) {
	vfs_s_free_super (me, super);
	ERRNOR (EIO, NULL);
    }
    if (!super->name)
	vfs_die( "You have to fill name\n" );
    if (!super->root)
	vfs_die( "You have to fill root inode\n" );

    vfs_s_insert_super (me, super);
    vfs_s_stamp_me (me, super, archive_name);

return_success:
    *archive = super;
    return local;
}

char *vfs_s_get_path (vfs *me, char *inname, struct vfs_s_super **archive, int flags)
{
    char *buf = strdup( inname );
    char *res = vfs_s_get_path_mangle( me, buf, archive, flags );
    char *res2 = NULL;
    if (res)
        res2 = strdup(res);
    free(buf);
    return res2;
}

void vfs_s_invalidate (vfs *me, vfs_s_super *super)
{
    if (!super->want_stale) {
	vfs_s_free_inode(me, super->root);
	super->root = vfs_s_new_inode (me, super, vfs_s_default_stat(me, S_IFDIR | 0755));
    }
}

char *vfs_s_fullpath (vfs *me, vfs_s_inode *ino)
{	/* For now, usable only on filesystems with _linear structure */
    if (MEDATA->find_entry != vfs_s_find_entry_linear)
	vfs_die( "Implement me!" );
    if ((!ino->ent) || (!ino->ent->dir) || (!ino->ent->dir->ent))
	ERRNOR(EAGAIN, NULL);
    return copy_strings( ino->ent->dir->ent->name, "/", ino->ent->name, NULL );
}

/* Support of archives */
/* ------------------------ readdir & friends ----------------------------- */

vfs_s_super *vfs_s_super_from_path (vfs *me, char *name)
{
    struct vfs_s_super *super;

    if (!vfs_s_get_path_mangle (me, name, &super, 0))
	return NULL;
    return super;
}

vfs_s_inode *vfs_s_inode_from_path (vfs *me, char *name, int flags)
{
    struct vfs_s_super *super;
    struct vfs_s_inode *ino;
    char *q;

    if (!(q = vfs_s_get_path_mangle (me, name, &super, 0))) 
	return NULL;

    ino = vfs_s_find_inode (me, super->root, q, flags & FL_FOLLOW ? LINK_FOLLOW : LINK_NO_FOLLOW, flags & ~FL_FOLLOW);
    if (ino) return ino;

    return ino;

}

struct dirhandle {
    vfs_s_entry *cur;
    vfs_s_inode *dir;
};

void * vfs_s_opendir (vfs *me, char *dirname)
{
    struct vfs_s_inode *dir;
    struct dirhandle *info;

    dir = vfs_s_inode_from_path (me, dirname, FL_DIR | FL_FOLLOW);
    if (!dir) return NULL;
    if (!S_ISDIR (dir->st.st_mode)) ERRNOR (ENOTDIR, NULL);

    dir->st.st_nlink++;
#if 0
    if (!dir->subdir)	/* This can actually happen if we allow empty directories */
	ERRNOR(EAGAIN, NULL);
#endif
    info = (struct dirhandle *) xmalloc (sizeof (struct dirhandle), "Shared opendir");
    info->cur = dir->subdir;
    info->dir = dir;

    return info;
}

void * vfs_s_readdir (void *data)
{
    static struct {
	struct dirent dir; 
#ifdef NEED_EXTRA_DIRENT_BUFFER
	char extra_buffer [MC_MAXPATHLEN];
#endif
    } dir;

    struct dirhandle *info = (struct dirhandle *) data;

    if (!(info->cur))
    	return NULL;

    if (info->cur->name)
	strcpy (&(dir.dir.d_name [0]), info->cur->name);
    else
	vfs_die( "Null in structure-can not happen");
    
#ifndef DIRENT_LENGTH_COMPUTED
    dir.d_namlen = strlen (dir.dir.d_name);
#endif
    info->cur = info->cur->next;
    
    return (void *)&dir;
}

int vfs_s_telldir (void *data)
{
    struct dirhandle *info = (struct dirhandle *) data;
    struct vfs_s_entry *cur;
    int num = 0;

    cur = info->dir->subdir;
    while (cur!=NULL) {
        if (cur == info->cur) return num;
	num++;
	cur = cur->next;
    }
    return -1;
}

void vfs_s_seekdir (void *data, int offset)
{
    struct dirhandle *info = (struct dirhandle *) data;
    int i;
    info->cur = info->dir->subdir;
    for (i=0; i<offset; i++)
        vfs_s_readdir( data );
}

int vfs_s_closedir (void *data)
{
    struct dirhandle *info = (struct dirhandle *) data;
    struct vfs_s_inode *dir = info->dir;

    vfs_s_free_inode(dir->super->me, dir);
    free (data);
    return 0;
}

int vfs_s_chdir (vfs *me, char *path)
{
    void *data;
    if (!(data = vfs_s_opendir( me, path )))
	return -1;
    vfs_s_closedir(data);
    return 0;
}

/* --------------------------- stat and friends ---------------------------- */

static int vfs_s_internal_stat (vfs *me, char *path, struct stat *buf, int flag)
{
    struct vfs_s_inode *ino;

    if (!(ino = vfs_s_inode_from_path( me, path, flag ))) return -1;
    *buf = ino->st;
    return 0;
}

int vfs_s_stat (vfs *me, char *path, struct stat *buf)
{
    return vfs_s_internal_stat (me, path, buf, FL_FOLLOW);
}

int vfs_s_lstat (vfs *me, char *path, struct stat *buf)
{
    return vfs_s_internal_stat (me, path, buf, FL_NONE);
}

int vfs_s_fstat (void *fh, struct stat *buf)
{
    *buf = FH->ino->st;
    return 0;
}

int vfs_s_readlink (vfs *me, char *path, char *buf, int size)
{
    struct vfs_s_inode *ino;

    ino = vfs_s_inode_from_path(me, path, 0);
    if (!ino) return -1;

    if (!S_ISLNK (ino->st.st_mode)) ERRNOR (EINVAL, -1);
    strncpy (buf, ino->linkname, size);
    *(buf+size-1) = 0;
    return strlen(buf);
}

void *vfs_s_open (vfs *me, char *file, int flags, int mode)
{
    int was_changed = 0;
    struct vfs_s_fh *fh;
    vfs_s_super *super;
    char *q;
    struct vfs_s_inode *ino;

    if ((q = vfs_s_get_path_mangle (me, file, &super, 0)) == NULL)
	return NULL;
    ino = vfs_s_find_inode (me, super->root, q, LINK_FOLLOW, FL_NONE);
    if (ino && ((flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL)))
	ERRNOR (EEXIST, NULL);
    if (!ino) { 
	char *dirname, *name, *save;
	vfs_s_entry *ent;
	vfs_s_inode *dir;
	if (!(flags & O_CREAT))
	    return NULL;

	split_dir_name(me, q, &dirname, &name, &save);
/* FIXME: if vfs_s_find_inode returns NULL, this will do rather bad
   things. */
	dir = vfs_s_find_inode(me, super->root, dirname, LINK_FOLLOW, FL_DIR);
	if (save)
	    *save = DIR_SEP_CHAR;
	ent = vfs_s_generate_entry (me, name, dir, 0755);
	ino = ent->ino;
	vfs_s_insert_entry(me, dir, ent);
	ino->localname = tempnam (NULL, me->name);
	was_changed = 1;
    }

    if (S_ISDIR (ino->st.st_mode)) ERRNOR (EISDIR, NULL);
    
    fh = (struct vfs_s_fh *) xmalloc (sizeof (struct vfs_s_fh), "Direntry: filehandle");
    fh->pos = 0;
    fh->ino = ino;
    fh->handle = -1;
    fh->changed = was_changed;
    fh->linear = 0;
    if (MEDATA->fh_open)
	if (MEDATA->fh_open (me, fh, flags, mode)) {
	    free(fh);
	    return NULL;
	}

    if (fh->ino->localname) {
	fh->handle = open(fh->ino->localname, flags, mode);
	if (fh->handle == -1) {
	    free(fh);
	    ERRNOR(errno, NULL);
	}
    }

     /* i.e. we had no open files and now we have one */
    vfs_rmstamp (&vfs_tarfs_ops, (vfsid) super, 1);
    super->fd_usage++;
    fh->ino->st.st_nlink++;
    return fh;
}

int vfs_s_read (void *fh, char *buffer, int count)
{
    int n;
    vfs *me = FH_SUPER->me;
    
    if (FH->linear == LS_LINEAR_CLOSED) {
        print_vfs_message ("Starting linear transfer...");
	if (!MEDATA->linear_start (me, FH, 0))
	    return -1;
    }

    if (FH->linear == LS_LINEAR_CLOSED)
        vfs_die ("linear_start() did not set linear_state!");

    if (FH->linear == LS_LINEAR_OPEN)
        return MEDATA->linear_read (me, FH, buffer, count);
        
    if (FH->handle) {
	n = read (FH->handle, buffer, count);
	if (n < 0)
	    me->verrno = errno;
	return n;
    }
    vfs_die( "vfs_s_read: This should not happen\n" );
}

int vfs_s_write (void *fh, char *buffer, int count)
{
    int n;
    vfs *me = FH_SUPER->me;
    
    if (FH->linear)
	vfs_die ("no writing to linear files, please" );
        
    FH->changed = 1;
    if (FH->handle) {
	n = write (FH->handle, buffer, count);
	if (n < 0)
	    me->verrno = errno;
	return n;
    }
    vfs_die( "vfs_s_write: This should not happen\n" );
}

int vfs_s_lseek (void *fh, off_t offset, int whence)
{
    off_t size = FH->ino->st.st_size;

    if (FH->handle != -1) {	/* If we have local file opened, we want to work with it */
	int retval = lseek(FH->handle, offset, whence);
	if (retval == -1)
	    FH->ino->super->me->verrno = errno;
	return retval;
    }

    switch (whence) {
    	case SEEK_CUR:
    	    offset += FH->pos; break;
    	case SEEK_END:
    	    offset += size; break;
    }
    if (offset < 0)
    	FH->pos = 0;
    else if (offset < size)
    	FH->pos = offset;
    else
    	FH->pos = size;
    return FH->pos;
}

int vfs_s_close (void *fh)
{
    int res = 0;
    vfs *me = FH_SUPER->me;

    FH_SUPER->fd_usage--;
    if (!FH_SUPER->fd_usage) {
        struct vfs_stamping *parent;
        vfs *v;
        
	v = vfs_type (FH_SUPER->name);
	if (v == &vfs_local_ops) {
	    parent = NULL;
	} else {
	    parent = xmalloc (sizeof (struct vfs_stamping), "vfs stamping");
	    parent->v = v;
	    parent->next = 0;
	    parent->id = (*v->getid) (v, FH_SUPER->name, &(parent->parent));
	}
        vfs_add_noncurrent_stamps (&vfs_tarfs_ops, (vfsid) (FH_SUPER), parent);
	vfs_rm_parents (parent);
    }
    if (FH->linear == LS_LINEAR_OPEN)
	MEDATA->linear_close (me, fh);
    if (MEDATA->fh_close)
	res = MEDATA->fh_close (me, fh);
    if (FH->changed && MEDATA->file_store) {
	char *s = vfs_s_fullpath( me, FH->ino );
	if (!s) res = -1;
	   else res = MEDATA->file_store (me, FH_SUPER, s, FH->ino->localname);
	vfs_s_invalidate(me, FH_SUPER);
    }
    if (FH->handle)
	close(FH->handle);
	
    vfs_s_free_inode (me, FH->ino);
    free (fh);
    return res;
}

/* ------------------------------- mc support ---------------------------- */

void vfs_s_fill_names (vfs *me, void (*func)(char *))
{
    struct vfs_s_super *a = MEDATA->supers;
    char *name;
    
    while (a){
	name = copy_strings ( a->name, "#", me->prefix, "/",
			      /* a->current_dir->name, */ 0);
	(*func)(name);
	free (name);
	a = a->next;
    }
}

int
vfs_s_ferrno(vfs *me)
{
    return me->verrno;
}

void
vfs_s_dump(vfs *me, char *prefix, vfs_s_inode *ino)
{
    printf( "%s %s %d ", prefix, S_ISDIR(ino->st.st_mode) ? "DIR" : "FILE", ino->st.st_mode );
    if (!ino->subdir) printf ("FILE\n");
    else
    {
	struct vfs_s_entry *ent;
	ent = ino->subdir;
	while(ent) {
	    char *s;
	    s = copy_strings(prefix, "/", ent->name, NULL );
	    if (ent->name[0] == '.')
		printf("%s IGNORED\n", s);
	    else
		vfs_s_dump(me, s, ent->ino);
	    free(s);
	    ent = ent->next;
	}
    }
}

char *vfs_s_getlocalcopy (vfs *me, char *path)
{
    struct vfs_s_inode *ino;
    char buf[MC_MAXPATHLEN];

    strcpy( buf, path );
    ino = vfs_s_inode_from_path (me, path, FL_FOLLOW | FL_NONE);

    if (!ino->localname)
	ino->localname = mc_def_getlocalcopy (me, buf);
    return ino->localname;
}

int 
vfs_s_setctl (vfs *me, char *path, int ctlop, char *arg)
{
    vfs_s_inode *ino = vfs_s_inode_from_path(me, path, 0);
    if (!ino)
	return 0;
    switch (ctlop) {
        case MCCTL_WANT_STALE_DATA:
	    ino->super->want_stale = 1;
	    return 1;
        case MCCTL_NO_STALE_DATA:
	    ino->super->want_stale = 0;
	    vfs_s_invalidate(me, ino->super);
	    return 1;
#if 0	/* FIXME: We should implement these */
	case MCCTL_REMOVELOCALCOPY:
	    return remove_temp_file (path);
        case MCCTL_FORGET_ABOUT:
	    my_forget(path);
	    return 0;
#endif
    }
    return 0;
}


/* ----------------------------- Stamping support -------------------------- */

vfsid vfs_s_getid (vfs *me, char *path, struct vfs_stamping **parent)
{
    vfs_s_super *archive;
    vfs *v;
    char *p;
    vfsid id;
    struct vfs_stamping *par;

    *parent = NULL;
    if (!(p = vfs_s_get_path (me, path, &archive, FL_NO_OPEN)))
	return (vfsid) -1;
    free(p);
    v = vfs_type (archive->name);
    id = (*v->getid) (v, archive->name, &par);
    if (id != (vfsid)-1) {
        *parent = xmalloc (sizeof (struct vfs_stamping), "vfs stamping");
        (*parent)->v = v;
        (*parent)->id = id;
        (*parent)->parent = par;
        (*parent)->next = NULL;
    }
    return (vfsid) archive;    
}

int vfs_s_nothingisopen (vfsid id)
{
    if (((vfs_s_super *)id)->fd_usage <= 0)
    	return 1;
    else
    	return 0;
}

void vfs_s_free (vfsid id)
{
    vfs_s_free_super (((vfs_s_super *)id)->me, (vfs_s_super *)id);
}

/* ----------- Utility functions for networked filesystems  -------------- */

int
vfs_s_select_on_two (int fd1, int fd2)
{
    fd_set set;
    struct timeval timeout;
    int v;
    int maxfd = (fd1 > fd2 ? fd1 : fd2) + 1;

    timeout.tv_sec  = 1;
    timeout.tv_usec = 0;
    FD_ZERO(&set);
    FD_SET(fd1, &set);
    FD_SET(fd2, &set);
    v = select (maxfd, &set, 0, 0, &timeout);
    if (v <= 0)
	return v;
    if (FD_ISSET (fd1, &set))
	return 1;
    if (FD_ISSET (fd2, &set))
	return 2;
    return -1;
}

int
vfs_s_get_line (vfs *me, int sock, char *buf, int buf_len, char term)
{
    FILE *logfile = MEDATA->logfile;
    int i, status;
    char c;

    for (i = 0; i < buf_len; i++, buf++) {
	if (read(sock, buf, sizeof(char)) <= 0)
	    return 0;
	if (logfile){
	    fwrite (buf, 1, 1, logfile);
	    fflush (logfile);
	}
	if (*buf == term) {
	    *buf = 0;
	    return 1;
	}
    }
    *buf = 0;
    while ((status = read(sock, &c, sizeof(c))) > 0){
	if (logfile){
	    fwrite (&c, 1, 1, logfile);
	    fflush (logfile);
	}
	if (c == '\n')
	    return 1;
    }
    return 0;
}

int
vfs_s_get_line_interruptible (vfs *me, char *buffer, int size, int fd)
{
    int n;
    int i = 0;

    enable_interrupt_key();
    for (i = 0; i < size-1; i++) {
	n = read (fd, buffer+i, 1);
	disable_interrupt_key();
	if (n == -1 && errno == EINTR){
	    buffer [i] = 0;
	    return EINTR;
	}
	if (n == 0){
	    buffer [i] = 0;
	    return 0;
	}
	if (buffer [i] == '\n'){
	    buffer [i] = 0;
	    return 1;
	}
    }
    buffer [size-1] = 0;
    return 0;
}
