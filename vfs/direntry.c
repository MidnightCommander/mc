/* Directory cache support -- so that you do not have copy of this in
 * each and every filesystem.
 *
 * Written at 1998 by Pavel Machek <pavel@ucw.cz>, distribute under LGPL.
 *
 * Based on tar.c from midnight and archives.[ch] from avfs by Miklos
 * Szeredi (mszeredi@inf.bme.hu) */

#include "direntry.h"

#define CALL(x) if (MEDATA->x) MEDATA->x

vfs_s_inode *vfs_s_new_inode (vfs *me, struct stat *initstat)
{
    vfs_s_inode *ino;

    ino = xmalloc(sizeof (vfs_s_inode), "Dcache inode");
    if (!ino) return NULL;

    ino->linkname = ino->localname = NULL;
    ino->subdir = NULL;
    if (initstat)
        ino->st = *initstat;
    ino->st.st_nlink = 0;

    CALL(init_inode) (me, ino);

    return ino;
}

vfs_s_entry *vfs_s_new_entry (vfs *me, char *name, vfs_s_inode *inode)
{
    vfs_s_entry *entry;

    entry = (struct vfs_s_entry *) xmalloc (sizeof (struct vfs_s_entry), "Dcache entry");

    entry->name = strdup (name);
    entry->dir = NULL;
    entry->next = NULL;
    entry->prevp = NULL;
    entry->ino = inode;
    CALL(init_entry) (me, entry);

    return entry;
}

void vfs_s_free_entry (vfs *me, vfs_s_entry *ent)
{
    *ent->prevp = ent->next;
    if (ent->next) ent->next->prevp = ent->prevp;

    printf( "Freeing : %s\n", ent->name);
    free(ent->name);
    ent->name = NULL;
    if(ent->ino->st.st_nlink == 1) {
	CALL(free_inode) (me, ent->ino);
	ifree(ent->ino->linkname);
	ifree(ent->ino->localname);
	ifree(ent->ino);
	ent->ino = NULL;
    } else ent->ino->st.st_nlink--;

    free(ent);
}

void vfs_s_free_tree (vfs *me, vfs_s_inode *dir, vfs_s_inode *parentdir)
{
    vfs_s_entry *ent;

    while(dir->subdir) {
	ent = dir->subdir;
	if(ent->ino != dir && ent->ino != parentdir) vfs_s_free_tree(me, ent->ino, dir);
	vfs_s_free_entry(me, ent);
    }
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
    st.st_ino = MEDATA->inode_counter++;
    st.st_dev = MEDATA->rdev;
    st.st_rdev = 0;
    st.st_uid = getuid ();
    st.st_gid = getgid ();
    st.st_size = 0;
    st.st_mtime = st.st_atime = st.st_ctime = time (NULL);
    st.st_nlink = 1;

    return &st;
}

void vfs_s_add_dots (vfs *me, vfs_s_inode *dir, vfs_s_inode *parent)
{
    struct vfs_s_entry *dot, *dotdot;

    if (!parent)
        parent = dir;
    dot = vfs_s_new_entry (me, ".", dir);
    dot->ino = dir;
    dotdot = vfs_s_new_entry (me, "..", parent);
    dot->ino = parent;
    vfs_s_insert_entry(me, dir, dot);
    vfs_s_insert_entry(me, dir, dotdot);
    dir->st.st_nlink += 2;
}

struct vfs_s_entry *vfs_s_generate_entry (vfs *me, char *name, struct vfs_s_inode *parent, mode_t mode)
{
    struct vfs_s_inode *inode;
    struct vfs_s_entry *entry;
    struct stat *st;

    st = vfs_s_default_stat (me, mode);
    inode = vfs_s_new_inode (me, st);
    if (S_ISDIR (mode))
        vfs_s_add_dots (me, inode, parent);

    entry = vfs_s_new_entry (me, name, inode);

    return entry;
}

/* We were asked to create entries automagically
 */
vfs_s_entry *vfs_s_automake(vfs *me, vfs_s_inode *dir, char *path, int flags)
{
    struct vfs_s_entry *res;
    char *sep = strchr( path, DIR_SEP_CHAR );
    if (sep) *sep = 0;
    res = vfs_s_generate_entry(me, path, dir, flags & FL_MKDIR ? (0777 | S_IFDIR) : 0777 );
    if (sep) *sep = DIR_SEP_CHAR;
    return res;
}

/* Follow > 0: follow links, serves as loop protect,
 *       == -1: do not follow links
 */
vfs_s_entry *vfs_s_find_entry(vfs *me, vfs_s_inode *root, char *path, int follow, int flags)
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
	    if(strlen(ent->name) == pseg && strncmp(ent->name, path, pseg) == 0)
		/* FOUND! */
		break;

	if (!ent && (flags & (FL_MKFILE | FL_MKDIR)))
	    ent = vfs_s_automake(me, root, path+pseg, flags);
	if (!ent) ERRNOR (ENOENT, NULL);
	path += pseg;
	if (!vfs_s_resolve_symlink(me, ent, follow)) return NULL;
	root = ent->ino;
    }
}

vfs_s_inode *vfs_s_find_inode(vfs *me, vfs_s_inode *root, char *path, int follow, int flags)
{
    vfs_s_entry *ent;
    if (!path || !path[0])
	return root;
    ent = vfs_s_find_entry(me, root, path, follow, flags);
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
    if (follow == -1)
	return entry;
    if (follow == 0)
	ERRNOR (ELOOP, NULL);
    if (!entry)
	ERRNOR (ENOENT, NULL);
    if (!S_ISLNK(entry->ino->st.st_mode))
	return entry;

    /* We have to handle "/" by ourself; "." and ".." have
       corresponding entries, so there's no problem with them. */
    if (*entry->ino->linkname == '/') dir = vfs_s_find_root(me, entry); 
    else dir = entry->dir;
    return vfs_s_find_entry (me, dir, entry->ino->linkname, follow-1, 0);
}

/* Ook, these were functions around direcory entries / inodes */
/* -------------------------------- superblock games -------------------------- */

vfs_s_super *vfs_s_new_super (vfs *me)
{
    vfs_s_super *super;

    super = xmalloc( sizeof( struct vfs_s_super ), "Direntry: superblock" );
    super->root = NULL;
    super->name = NULL;
    super->fd_usage = 0;
    super->me = me;

    super->next = MEDATA->supers;
    super->prevp = &MEDATA->supers;

    if (MEDATA->supers != NULL) MEDATA->supers->prevp = &super->next;
    MEDATA->supers = super;
    return super;
} 

void vfs_s_free_super (vfs *me, vfs_s_super *super)
{
    if (super->root)
	vfs_s_free_tree (me, super->root, NULL);

    *super->prevp = super->next;
    if (super->next) super->next->prevp = super->prevp;

    CALL(free_archive) (me, super);
    free(super);
}

/* ========================================================================= */

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
    struct vfs_s_super *psup;
    void *cookie;
    
    archive_name = inname;
    vfs_split( inname, &local, &op );
    if (!local)
        local = "";

    if (! (cookie = MEDATA->archive_check (me, archive_name, op)))
	return NULL;

    for (psup = MEDATA->supers; psup != NULL; psup = psup->next) {
	int i; /* 0 == other, 1 == same, return it, 2 == other but stop scanning */
	if ((i = MEDATA->archive_same (me, psup, archive_name, cookie))) {
	    if (i==1) goto return_success;
	    else break;
	}
    }

    if (flags & FL_NO_OPEN) ERRNOR (EIO, NULL);

    psup = vfs_s_new_super (me);
    result = MEDATA->open_archive (me, psup, archive_name, op);
    if (result == -1) {
	vfs_s_free_super (me, psup);
	ERRNOR (EIO, NULL);
    }

    vfs_s_stamp_me (me, psup, archive_name);

return_success:
    *archive = psup;
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

/* Support of archives */
/* ========================== readdir & friends ========================================= */

vfs_s_inode *vfs_s_inode_from_path (vfs *me, char *name, int flags)
{
    struct vfs_s_super *super;
    char *q;
    struct vfs_s_inode *ino;

    if (!(q = vfs_s_get_path_mangle (me, name, &super, 0))) 
	return NULL;

    ino = vfs_s_find_inode (me, super->root, q, flags & FL_FOLLOW ? FOLLOW : NO_FOLLOW, FL_NONE);
    if (ino) return ino;

    if ((flags & FL_DIR) && MEDATA->load_dir) {
	MEDATA->load_dir (me, super, q);
	ino = vfs_s_find_inode (me, super->root, q, flags & FL_FOLLOW ? FOLLOW : NO_FOLLOW, FL_NONE);
    }

    return ino;
}

void * vfs_s_opendir (vfs *me, char *dirname)
{
    struct vfs_s_inode *ino;
    struct vfs_s_entry **info;

    ino = vfs_s_inode_from_path (me, dirname, FL_DIR | FL_FOLLOW);
    if (!ino) return NULL;
    if (!S_ISDIR (ino->st.st_mode)) ERRNOR (ENOTDIR, NULL);

    info = (struct vfs_s_entry **) xmalloc (2*sizeof (struct vfs_s_entry *), "Dentry opendir");
    info[0] = ino->subdir;
    info[1] = ino->subdir;

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

    struct vfs_s_entry **info = (struct vfs_s_entry **) data;

    if (!*info)
    	return NULL;

    strcpy (&(dir.dir.d_name [0]), (*info)->name);
    
#ifndef DIRENT_LENGTH_COMPUTED
    dir.d_namlen = strlen (dir.dir.d_name);
#endif
    *info = (*info)->next;
    
    return (void *)&dir;
}

int vfs_s_telldir (void *data)
{
    struct vfs_s_entry **info = (struct vfs_s_entry **) data;
    struct vfs_s_entry *cur;
    int num = 0;

    cur = info[1];
    while (cur!=NULL) {
        if (cur == info[0]) return num;
	num++;
	cur = cur->next;
    }
    return -1;
}

void vfs_s_seekdir (void *data, int offset)
{
    struct vfs_s_entry **info = (struct vfs_s_entry **) data;
    int i;
    info[0] = info[1];
    for (i=0; i<offset; i++)
        vfs_s_readdir( data );
}

int vfs_s_closedir (void *data)
{
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

/* =========================== stat and friends ============================== */

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
    struct vfs_s_fh *fh;
    vfs_s_super *super;
    char *q;
    struct vfs_s_inode *ino;

    if ((q = vfs_s_get_path_mangle (me, file, &super, 0)) == NULL)
	return NULL;
    if (!(ino = vfs_s_find_inode (me, super->root, q, FOLLOW, FL_NONE)))
    	return NULL;
    if (S_ISDIR (ino->st.st_mode)) ERRNOR (EISDIR, NULL);
    if ((flags & O_ACCMODE) != O_RDONLY) ERRNOR (EROFS, NULL);
    
    fh = (struct vfs_s_fh *) xmalloc (sizeof (struct vfs_s_fh), "Direntry: filehandle");
    fh->super = super;
    fh->pos = 0;
    fh->ino = ino;
    fh->handle = -1;
    if (MEDATA->fh_open)
	if (MEDATA->fh_open (me, fh)) {
	    free(fh);
	    return NULL;
	}

     /* i.e. we had no open files and now we have one */
    vfs_rmstamp (&vfs_tarfs_ops, (vfsid) super, 1);
    super->fd_usage++;
    return fh;
}

int vfs_s_lseek (void *fh, off_t offset, int whence)
{
    off_t size = FH->ino->st.st_size;
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
    vfs *me = FH->super->me;

    FH->super->fd_usage--;
    if (!FH->super->fd_usage) {
        struct vfs_stamping *parent;
        vfs *v;
        
	v = vfs_type (FH->super->name);
	if (v == &vfs_local_ops) {
	    parent = NULL;
	} else {
	    parent = xmalloc (sizeof (struct vfs_stamping), "vfs stamping");
	    parent->v = v;
	    parent->next = 0;
	    parent->id = (*v->getid) (v, FH->super->name, &(parent->parent));
	}
        vfs_add_noncurrent_stamps (&vfs_tarfs_ops, (vfsid) (FH->super), parent);
	vfs_rm_parents (parent);
    }
    CALL(fh_close) (me, fh);
    if (FH->handle)
	mc_close(FH->handle);
	
    free (fh);
    return 0;
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

/* ----------------------------- Stamping support ----------------------------- */

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
