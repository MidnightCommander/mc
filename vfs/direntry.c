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

#include <config.h>
#include <errno.h>

#include "utilvfs.h"
#include "xdirentry.h"
#include "../src/tty.h"

#define CALL(x) if (MEDATA->x) MEDATA->x

static volatile int total_inodes = 0, total_entries = 0;

struct vfs_s_inode *
vfs_s_new_inode (struct vfs_class *me, struct vfs_s_super *super, struct stat *initstat)
{
    struct vfs_s_inode *ino;

    ino = g_new0 (struct vfs_s_inode, 1);
    if (!ino)
	return NULL;

    if (initstat)
        ino->st = *initstat;
    ino->super = super;
    ino->st.st_nlink = 0;
    ino->st.st_ino = MEDATA->inode_counter++;
    ino->st.st_dev = MEDATA->rdev;

    super->ino_usage++;
    total_inodes++;
    
    CALL (init_inode) (me, ino);

    return ino;
}

struct vfs_s_entry *
vfs_s_new_entry (struct vfs_class *me, char *name, struct vfs_s_inode *inode)
{
    struct vfs_s_entry *entry;

    entry = g_new0 (struct vfs_s_entry, 1);
    total_entries++;

    if (name)
	entry->name = g_strdup (name);

    entry->ino = inode;
    entry->ino->ent = entry;
    CALL (init_entry) (me, entry);

    return entry;
}

static void
vfs_s_free_inode (struct vfs_class *me, struct vfs_s_inode *ino)
{
    if (!ino)
	vfs_die ("Don't pass NULL to me");

    /* ==0 can happen if freshly created entry is deleted */
    if (ino->st.st_nlink <= 1){
	while (ino->subdir){
	    vfs_s_free_entry (me, ino->subdir);
	}

	CALL (free_inode) (me, ino);
	g_free (ino->linkname);
	if (ino->localname){
	    unlink (ino->localname);
	    g_free(ino->localname);
	}
	total_inodes--;
	ino->super->ino_usage--;
	g_free(ino);
    } else ino->st.st_nlink--;
}

void
vfs_s_free_entry (struct vfs_class *me, struct vfs_s_entry *ent)
{
    int is_dot = 0;
    if (ent->prevp){	/* It is possible that we are deleting freshly created entry */
	*ent->prevp = ent->next;
	if (ent->next)
	    ent->next->prevp = ent->prevp;
    }

    if (ent->name){
	is_dot = (!strcmp (ent->name, ".")) || (!strcmp (ent->name, ".."));
	g_free (ent->name);
	ent->name = NULL;
    }
	
    if (!is_dot && ent->ino){
	ent->ino->ent = NULL;
	vfs_s_free_inode (me, ent->ino);
	ent->ino = NULL;
    }

    total_entries--;
    g_free(ent);
}

void
vfs_s_insert_entry (struct vfs_class *me, struct vfs_s_inode *dir, struct vfs_s_entry *ent)
{
    struct vfs_s_entry **ep;

    for (ep = &dir->subdir; *ep != NULL; ep = &((*ep)->next))
	;
    ent->prevp = ep;
    ent->next = NULL;
    ent->dir = dir;
    *ep = ent;

    ent->ino->st.st_nlink++;
}

struct stat *
vfs_s_default_stat (struct vfs_class *me, mode_t mode)
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

void
vfs_s_add_dots (struct vfs_class *me, struct vfs_s_inode *dir, struct vfs_s_inode *parent)
{
    struct vfs_s_entry *dot, *dotdot;

    if (!parent)
        parent = dir;
    dot = vfs_s_new_entry (me, ".", dir);
    dotdot = vfs_s_new_entry (me, "..", parent);
    vfs_s_insert_entry (me, dir, dot);
    vfs_s_insert_entry (me, dir, dotdot);
    dir->st.st_nlink--;
    parent->st.st_nlink--;	/* We do not count "." and ".." into nlinks */
}

struct vfs_s_entry *
vfs_s_generate_entry (struct vfs_class *me, char *name, struct vfs_s_inode *parent, mode_t mode)
{
    struct vfs_s_inode *inode;
    struct stat *st;

    st = vfs_s_default_stat (me, mode);
    inode = vfs_s_new_inode (me, parent->super, st);
    if (S_ISDIR (mode))
        vfs_s_add_dots (me, inode, parent);

    return vfs_s_new_entry (me, name, inode);
}

/* We were asked to create entries automagically */
static struct vfs_s_entry *
vfs_s_automake (struct vfs_class *me, struct vfs_s_inode *dir, char *path, int flags)
{
    struct vfs_s_entry *res;
    char *sep = strchr (path, PATH_SEP);
    
    if (sep)
	    *sep = 0;
    res = vfs_s_generate_entry (me, path, dir, flags & FL_MKDIR ? (0777 | S_IFDIR) : 0777);
    vfs_s_insert_entry (me, dir, res);

    if (sep)
	    *sep = PATH_SEP;

    return res;
}

/* If the entry is a symlink, find the entry for its target */
static struct vfs_s_entry *
vfs_s_resolve_symlink (struct vfs_class *me, struct vfs_s_entry *entry,
		       char *path, int follow)
{
    char *linkname;
    char *fullname = NULL;
    struct vfs_s_entry *target;

    if (follow == LINK_NO_FOLLOW)
	return entry;
    if (follow == 0)
	ERRNOR (ELOOP, NULL);
    if (!entry)
	ERRNOR (ENOENT, NULL);
    if (!S_ISLNK (entry->ino->st.st_mode))
	return entry;

    linkname = entry->ino->linkname;
    if (linkname == NULL)
	ERRNOR (EFAULT, NULL);

    /* make full path from relative */
    if (*linkname != PATH_SEP) {
	char *fullpath = vfs_s_fullpath (me, entry->dir);
	fullname = g_strdup_printf ("%s/%s", fullpath, linkname);
	linkname = fullname;
	g_free (fullpath);
    }

    target =
	(MEDATA->find_entry) (me, entry->dir->super->root, linkname,
			      follow - 1, 0);
    g_free (fullname);
    return target;
}

/*
 * Follow > 0: follow links, serves as loop protect,
 *       == -1: do not follow links
 */
struct vfs_s_entry *
vfs_s_find_entry_tree (struct vfs_class *me, struct vfs_s_inode *root,
		       char *path, int follow, int flags)
{
    unsigned int pseg;
    struct vfs_s_entry *ent = NULL;
    char p[MC_MAXPATHLEN] = "";

    if (strlen(path) >= MC_MAXPATHLEN)
	return NULL;

    canonicalize_pathname (path);

    while (root) {
	int t;

	while (*path == PATH_SEP)	/* Strip leading '/' */
	    path++;

	if (!path[0])
	    return ent;

	for (pseg = 0; path[pseg] && path[pseg] != PATH_SEP; pseg++);

	strcat (p, PATH_SEP_STR);
	strncpy (p + (t = strlen (p)), path, pseg);
	p[t + pseg] = '\0';

	for (ent = root->subdir; ent != NULL; ent = ent->next)
	    if (strlen (ent->name) == pseg
		&& (!strncmp (ent->name, path, pseg)))
		/* FOUND! */
		break;

	if (!ent && (flags & (FL_MKFILE | FL_MKDIR)))
	    ent = vfs_s_automake (me, root, path, flags);
	if (!ent)
	    ERRNOR (ENOENT, NULL);
	path += pseg;
	/* here we must follow leading directories always;
	   only the actual file is optional */
	ent =
	    vfs_s_resolve_symlink (me, ent, p,
				   strchr (path,
					   PATH_SEP) ? LINK_FOLLOW :
				   follow);
	if (!ent)
	    return NULL;
	root = ent->ino;
    }

    return NULL;
}

static void
split_dir_name (struct vfs_class *me, char *path, char **dir, char **name, char **save)
{
    char *s;
    s = strrchr (path, PATH_SEP);
    if (!s){
	*save = NULL;
	*name = path;
	*dir = "";
    } else {
	*save = s;
	*dir = path;
	*s++ = 0;
	*name = s;
    }
}

struct vfs_s_entry *
vfs_s_find_entry_linear (struct vfs_class *me, struct vfs_s_inode *root, char *path, int follow, int flags)
{
    struct vfs_s_entry* ent = NULL;

    if (root->super->root != root)
        vfs_die ("We have to use _real_ root. Always. Sorry." );

    canonicalize_pathname (path);

    if (!(flags & FL_DIR)){
	char *dirname, *name, *save;
	struct vfs_s_inode *ino;
	split_dir_name (me, path, &dirname, &name, &save);
	ino = vfs_s_find_inode (me, root, dirname, follow, flags | FL_DIR);
	if (save)
	    *save = PATH_SEP;
	return vfs_s_find_entry_tree (me, ino, name, follow, flags);
    }

    for (ent = root->subdir; ent != NULL; ent = ent->next)
	if (!strcmp (ent->name, path))
	    break;

    if (ent && (! (MEDATA->dir_uptodate) (me, ent->ino))){
#if 1
	print_vfs_message (_("Directory cache expired for %s"), path);
#endif
	vfs_s_free_entry (me, ent);
	ent = NULL;
    }

    if (!ent){
	struct vfs_s_inode *ino;

	ino = vfs_s_new_inode (me, root->super, vfs_s_default_stat (me, S_IFDIR | 0755));
	ent = vfs_s_new_entry (me, path, ino);
	if ((MEDATA->dir_load) (me, ino, path) == -1){
	    vfs_s_free_entry (me, ent);
	    return NULL;
	}
	vfs_s_insert_entry (me, root, ent);

	for (ent = root->subdir; ent != NULL; ent = ent->next)
	    if (!strcmp (ent->name, path))
		break;
    }
    if (!ent) 
	vfs_die ("find_linear: success but directory is not there\n");

#if 0
    if (!vfs_s_resolve_symlink (me, ent, follow)) return NULL;
#endif
    return ent;
}

struct vfs_s_inode *
vfs_s_find_inode (struct vfs_class *me, struct vfs_s_inode *root, char *path, int follow, int flags)
{
    struct vfs_s_entry *ent;
    if ((MEDATA->find_entry == vfs_s_find_entry_tree) && (!*path))
	return root;
    ent = (MEDATA->find_entry)(me, root, path, follow, flags);
    if (!ent)
	return NULL;
    return ent->ino;
}

/* Ook, these were functions around directory entries / inodes */
/* -------------------------------- superblock games -------------------------- */

static struct vfs_s_super *
vfs_s_new_super (struct vfs_class *me)
{
    struct vfs_s_super *super;

    super = g_new0 (struct vfs_s_super, 1);
    super->me = me;
    return super;
}

static void
vfs_s_insert_super (struct vfs_class *me, struct vfs_s_super *super)
{
    super->next = MEDATA->supers;
    super->prevp = &MEDATA->supers;

    if (MEDATA->supers != NULL)
	    MEDATA->supers->prevp = &super->next;
    MEDATA->supers = super;
} 

static void
vfs_s_free_super (struct vfs_class *me, struct vfs_s_super *super)
{
    if (super->root){
	vfs_s_free_inode (me, super->root);
	super->root = NULL;
    }

#if 0
    /* FIXME: We currently leak small ammount of memory, sometimes. Fix it if you can. */
    if (super->ino_usage)
	message_1s1d (1, " Direntry warning ",
			 "Super ino_usage is %d, memory leak",
			 super->ino_usage);

    if (super->want_stale)
	message_1s (1, " Direntry warning ", "Super has want_stale set");
#endif

    if (super->prevp){
	*super->prevp = super->next;
	if (super->next)
		super->next->prevp = super->prevp;
    }

    CALL (free_archive) (me, super);
    g_free (super->name);
    super->name = NULL;
    g_free(super);
}

/* ------------------------------------------------------------------------= */

static void
vfs_s_stamp_me (struct vfs_class *me, struct vfs_s_super *psup, char *fs_name)
{
    struct vfs_stamping *parent;
    struct vfs_class *v;
 
    v = vfs_get_class (fs_name);
    if (v->flags & VFSF_LOCAL){
	parent = NULL;
    } else {
	parent = g_new (struct vfs_stamping, 1);
	parent->v = v;
	parent->next = 0;
	parent->id = (*v->getid) (v, fs_name, &(parent->parent));
    }
    vfs_add_noncurrent_stamps (me, (vfsid) psup, parent);
}

char *
vfs_s_get_path_mangle (struct vfs_class *me, char *inname, struct vfs_s_super **archive, int flags)
{
    char *local, *op, *archive_name;
    int result = -1;
    struct vfs_s_super *super;
    void *cookie = NULL;
    
    archive_name = inname;
    vfs_split (inname, &local, &op);
    if (!local)
        local = "";

    if (MEDATA->archive_check)
	if (! (cookie = MEDATA->archive_check (me, archive_name, op)))
	    return NULL;

    for (super = MEDATA->supers; super != NULL; super = super->next){
	int i; /* 0 == other, 1 == same, return it, 2 == other but stop scanning */
	if ((i = MEDATA->archive_same (me, super, archive_name, op, cookie))){
	    if (i==1) goto return_success;
	    else break;
	}
    }

    if (flags & FL_NO_OPEN)
	ERRNOR (EIO, NULL);

    super = vfs_s_new_super (me);
    result = MEDATA->open_archive (me, super, archive_name, op);
    if (result == -1){
	vfs_s_free_super (me, super);
	ERRNOR (EIO, NULL);
    }
    if (!super->name)
	vfs_die ("You have to fill name\n");
    if (!super->root)
	vfs_die ("You have to fill root inode\n");

    vfs_s_insert_super (me, super);
    vfs_s_stamp_me (me, super, archive_name);

return_success:
    *archive = super;
    return local;
}

static char *
vfs_s_get_path (struct vfs_class *me, const char *inname, struct vfs_s_super **archive, int flags)
{
    char *buf = g_strdup( inname );
    char *res = vfs_s_get_path_mangle (me, buf, archive, flags);
    if (res)
        res = g_strdup(res);
    g_free(buf);
    return res;
}

void
vfs_s_invalidate (struct vfs_class *me, struct vfs_s_super *super)
{
    if (!super->want_stale){
	vfs_s_free_inode (me, super->root);
	super->root = vfs_s_new_inode (me, super, vfs_s_default_stat (me, S_IFDIR | 0755));
    }
}

char *
vfs_s_fullpath (struct vfs_class *me, struct vfs_s_inode *ino)
{
    if (!ino->ent)
	ERRNOR (EAGAIN, NULL);

    if (MEDATA->find_entry != vfs_s_find_entry_linear) {
	char *newpath;
	char *path = g_strdup (ino->ent->name);
	while (1) {
	    ino = ino->ent->dir;
	    if (ino == ino->super->root)
		break;
	    newpath = g_strdup_printf ("%s/%s", ino->ent->name, path);
	    g_free (path);
	    path = newpath;
	}
	return path;
    }

    /* It must be directory */
    if ((!ino->ent->dir) || (!ino->ent->dir->ent))
	return g_strdup (ino->ent->name);

    return g_strconcat (ino->ent->dir->ent->name, PATH_SEP_STR,
			ino->ent->name, NULL);
}

/* Support of archives */
/* ------------------------ readdir & friends ----------------------------- */

static struct vfs_s_inode *
vfs_s_inode_from_path (struct vfs_class *me, char *name, int flags)
{
    struct vfs_s_super *super;
    struct vfs_s_inode *ino;
    char *q;

    if (!(q = vfs_s_get_path_mangle (me, name, &super, 0))) 
	return NULL;

    ino = vfs_s_find_inode (me, super->root, q, flags & FL_FOLLOW ? LINK_FOLLOW : LINK_NO_FOLLOW, flags & ~FL_FOLLOW);
    if ((!ino) && (!*q))
	/* We are asking about / directory of ftp server: assume it exists */
	ino = vfs_s_find_inode (me, super->root, q, flags & FL_FOLLOW ? LINK_FOLLOW : LINK_NO_FOLLOW, FL_DIR | (flags & ~FL_FOLLOW));
    return ino;
}

struct dirhandle {
    struct vfs_s_entry *cur;
    struct vfs_s_inode *dir;
};

static void *
vfs_s_opendir (struct vfs_class *me, char *dirname)
{
    struct vfs_s_inode *dir;
    struct dirhandle *info;

    dir = vfs_s_inode_from_path (me, dirname, FL_DIR | FL_FOLLOW);
    if (!dir)
	return NULL;
    if (!S_ISDIR (dir->st.st_mode))
	ERRNOR (ENOTDIR, NULL);

    dir->st.st_nlink++;
#if 0
    if (!dir->subdir)	/* This can actually happen if we allow empty directories */
	ERRNOR (EAGAIN, NULL);
#endif
    info = g_new (struct dirhandle, 1);
    info->cur = dir->subdir;
    info->dir = dir;

    return info;
}

static void *
vfs_s_readdir(void *data)
{
    static union vfs_dirent dir;
    struct dirhandle *info = (struct dirhandle *) data;

    if (!(info->cur))
	return NULL;

    if (info->cur->name) {
	strncpy(dir.dent.d_name, info->cur->name, MC_MAXPATHLEN);
	dir.dent.d_name[MC_MAXPATHLEN] = 0;
    } else {
	vfs_die("Null in structure-cannot happen");
    }

    compute_namelen(&dir.dent);
    info->cur = info->cur->next;

    return (void *) &dir;
}

static int
vfs_s_closedir (void *data)
{
    struct dirhandle *info = (struct dirhandle *) data;
    struct vfs_s_inode *dir = info->dir;

    vfs_s_free_inode (dir->super->me, dir);
    g_free (data);
    return 0;
}

static int
vfs_s_chdir (struct vfs_class *me, char *path)
{
    void *data;
    if (!(data = vfs_s_opendir (me, path)))
	return -1;
    vfs_s_closedir (data);
    return 0;
}

/* --------------------------- stat and friends ---------------------------- */

static int
vfs_s_internal_stat (struct vfs_class *me, char *path, struct stat *buf, int flag)
{
    struct vfs_s_inode *ino;

    if (!(ino = vfs_s_inode_from_path (me, path, flag)))
        return -1;
    *buf = ino->st;
    return 0;
}

static int
vfs_s_stat (struct vfs_class *me, char *path, struct stat *buf)
{
    return vfs_s_internal_stat (me, path, buf, FL_FOLLOW);
}

static int
vfs_s_lstat (struct vfs_class *me, char *path, struct stat *buf)
{
    return vfs_s_internal_stat (me, path, buf, FL_NONE);
}

static int
vfs_s_fstat (void *fh, struct stat *buf)
{
    *buf = FH->ino->st;
    return 0;
}

static int
vfs_s_readlink (struct vfs_class *me, char *path, char *buf, int size)
{
    struct vfs_s_inode *ino;

    ino = vfs_s_inode_from_path (me, path, 0);
    if (!ino)
	return -1;

    if (!S_ISLNK (ino->st.st_mode))
	ERRNOR (EINVAL, -1);

    if (ino->linkname == NULL)
	ERRNOR (EFAULT, -1);

    strncpy (buf, ino->linkname, size);
    *(buf+size-1) = 0;
    return strlen (buf);
}

static void *
vfs_s_open (struct vfs_class *me, char *file, int flags, int mode)
{
    int was_changed = 0;
    struct vfs_s_fh *fh;
    struct vfs_s_super *super;
    char *q;
    struct vfs_s_inode *ino;

    if ((q = vfs_s_get_path_mangle (me, file, &super, 0)) == NULL)
	return NULL;
    ino = vfs_s_find_inode (me, super->root, q, LINK_FOLLOW, FL_NONE);
    if (ino && ((flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL)))
	ERRNOR (EEXIST, NULL);
    if (!ino){ 
	char *dirname, *name, *save;
	struct vfs_s_entry *ent;
	struct vfs_s_inode *dir;
	int tmp_handle;

	/* If the filesystem is read-only, disable file creation */
	if (!(flags & O_CREAT) || !(me->write))
	    return NULL;

	split_dir_name (me, q, &dirname, &name, &save);
/* FIXME: if vfs_s_find_inode returns NULL, this will do rather bad
   things. */
	dir = vfs_s_find_inode (me, super->root, dirname, LINK_FOLLOW, FL_DIR);
	if (save)
	    *save = PATH_SEP;
	ent = vfs_s_generate_entry (me, name, dir, 0755);
	ino = ent->ino;
	vfs_s_insert_entry (me, dir, ent);
	tmp_handle = mc_mkstemps (&ino->localname, me->name, NULL);
	if (tmp_handle == -1)
	    return NULL;
	close (tmp_handle);
	was_changed = 1;
    }

    if (S_ISDIR (ino->st.st_mode))
	ERRNOR (EISDIR, NULL);
    
    fh = g_new (struct vfs_s_fh, 1);
    fh->pos = 0;
    fh->ino = ino;
    fh->handle = -1;
    fh->changed = was_changed;
    fh->linear = 0;

    if (IS_LINEAR(flags)) {
	if (MEDATA->linear_start) {
	    print_vfs_message (_("Starting linear transfer..."));
	    if (!MEDATA->linear_start (me, fh, 0)){
		g_free(fh);
		return NULL;
	    }
	}
    } else if ((MEDATA->fh_open) && (MEDATA->fh_open (me, fh, flags, mode))){
	    g_free(fh);
	    return NULL;
	}

    if (fh->ino->localname){
	fh->handle = open (fh->ino->localname, NO_LINEAR(flags), mode);
	if (fh->handle == -1){
	    g_free(fh);
	    ERRNOR (errno, NULL);
	}
    }

     /* i.e. we had no open files and now we have one */
    vfs_rmstamp (me, (vfsid) super, 1);
    super->fd_usage++;
    fh->ino->st.st_nlink++;
    return fh;
}

static int
vfs_s_read (void *fh, char *buffer, int count)
{
    int n;
    struct vfs_class *me = FH_SUPER->me;

    if (FH->linear == LS_LINEAR_CLOSED)
        vfs_die ("linear_start() did not set linear_state!");

    if (FH->linear == LS_LINEAR_OPEN)
        return MEDATA->linear_read (me, FH, buffer, count);
        
    if (FH->handle != -1){
	n = read (FH->handle, buffer, count);
	if (n < 0)
	    me->verrno = errno;
	return n;
    }
    vfs_die ("vfs_s_read: This should not happen\n");
    return -1;
}

static int
vfs_s_write (void *fh, char *buffer, int count)
{
    int n;
    struct vfs_class *me = FH_SUPER->me;
    
    if (FH->linear)
	vfs_die ("no writing to linear files, please");
        
    FH->changed = 1;
    if (FH->handle != -1){
	n = write (FH->handle, buffer, count);
	if (n < 0)
	    me->verrno = errno;
	return n;
    }
    vfs_die ("vfs_s_write: This should not happen\n");
    return 0;
}

static int
vfs_s_lseek (void *fh, off_t offset, int whence)
{
    off_t size = FH->ino->st.st_size;

    if (FH->handle != -1){	/* If we have local file opened, we want to work with it */
	int retval = lseek (FH->handle, offset, whence);
	if (retval == -1)
	    FH->ino->super->me->verrno = errno;
	return retval;
    }

    switch (whence){
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

static int
vfs_s_close (void *fh)
{
    int res = 0;
    struct vfs_class *me = FH_SUPER->me;

    FH_SUPER->fd_usage--;
    if (!FH_SUPER->fd_usage){
        struct vfs_stamping *parent;
        struct vfs_class *v;
        
	v = vfs_get_class (FH_SUPER->name);
	if (v->flags & VFSF_LOCAL) {
	    parent = NULL;
	} else {
	    parent = g_new (struct vfs_stamping, 1);
	    parent->v = v;
	    parent->next = 0;
	    parent->id = (*v->getid) (v, FH_SUPER->name, &(parent->parent));
	}
        vfs_add_noncurrent_stamps (me, (vfsid) (FH_SUPER), parent);
    }
    if (FH->linear == LS_LINEAR_OPEN)
	MEDATA->linear_close (me, fh);
    if (MEDATA->fh_close)
	res = MEDATA->fh_close (me, fh);
    if (FH->changed && MEDATA->file_store){
	char *s = vfs_s_fullpath (me, FH->ino);
	if (!s)
	    res = -1;
 	else {
	    res = MEDATA->file_store (me, fh, s, FH->ino->localname);
	    g_free (s);
	}
	vfs_s_invalidate (me, FH_SUPER);
    }
    if (FH->handle != -1)
	close (FH->handle);
	
    vfs_s_free_inode (me, FH->ino);
    g_free (fh);
    return res;
}

int
vfs_s_retrieve_file (struct vfs_class *me, struct vfs_s_inode *ino)
{
    /* If you want reget, you'll have to open file with O_LINEAR */
    off_t total = 0;
    char buffer[8192];
    int handle, n;
    off_t stat_size = ino->st.st_size;
    struct vfs_s_fh fh;

    memset (&fh, 0, sizeof (fh));

    fh.ino = ino;
    fh.handle = -1;

    handle = mc_mkstemps (&ino->localname, me->name, NULL);
    if (handle == -1) {
	me->verrno = errno;
	goto error_4;
    }

    if (!MEDATA->linear_start (me, &fh, 0))
	goto error_3;

    /* Clear the interrupt status */
    got_interrupt ();
    enable_interrupt_key ();

    while ((n = MEDATA->linear_read (me, &fh, buffer, sizeof (buffer)))) {

	if (n < 0)
	    goto error_1;

	total += n;
	vfs_print_stats (me->name, _("Getting file"), ino->ent->name,
			 total, stat_size);

	if (got_interrupt ())
	    goto error_1;

	if (write (handle, buffer, n) < 0) {
	    me->verrno = errno;
	    goto error_1;
	}
    }
    MEDATA->linear_close (me, &fh);
    close (handle);

    disable_interrupt_key ();
    return 0;

  error_1:
    MEDATA->linear_close (me, &fh);
  error_3:
    disable_interrupt_key ();
    close (handle);
    unlink (ino->localname);
  error_4:
    g_free (ino->localname);
    ino->localname = NULL;
    return -1;
}

/* ------------------------------- mc support ---------------------------- */

static void
vfs_s_fill_names (struct vfs_class *me, void (*func)(char *))
{
    struct vfs_s_super *a = MEDATA->supers;
    char *name;
    
    while (a){
	name = g_strconcat ( a->name, "#", me->prefix, "/", /* a->current_dir->name, */ NULL);
	(*func)(name);
	g_free (name);
	a = a->next;
    }
}

static int
vfs_s_ferrno (struct vfs_class *me)
{
    return me->verrno;
}

static char *
vfs_s_getlocalcopy (struct vfs_class *me, char *path)
{
    struct vfs_s_inode *ino;
    char buf[MC_MAXPATHLEN];

    strncpy (buf, path, MC_MAXPATHLEN);
    ino = vfs_s_inode_from_path (me, path, FL_FOLLOW | FL_NONE);

    if (!ino->localname)
	ino->localname = mc_def_getlocalcopy (me, buf);
    /* FIXME: fd_usage++ missing */
    return g_strdup (ino->localname);
}

static int
vfs_s_setctl (struct vfs_class *me, char *path, int ctlop, void *arg)
{
    switch (ctlop) {
    case VFS_SETCTL_STALE_DATA:
	{
	    struct vfs_s_inode *ino = vfs_s_inode_from_path (me, path, 0);

	    if (!ino)
		return 0;
	    if (arg)
		ino->super->want_stale = 1;
	    else {
		ino->super->want_stale = 0;
		vfs_s_invalidate (me, ino->super);
	    }
	    return 1;
	}
    case VFS_SETCTL_LOGFILE:
	MEDATA->logfile = fopen ((char *) arg, "w");
	return 1;
    case VFS_SETCTL_FLUSH:
	MEDATA->flush = 1;
	return 1;
    }
    return 0;
}


/* ----------------------------- Stamping support -------------------------- */

static vfsid
vfs_s_getid (struct vfs_class *me, const char *path, struct vfs_stamping **parent)
{
    struct vfs_s_super *archive;
    struct vfs_class *v;
    char *p;
    vfsid id;
    struct vfs_stamping *par;

    *parent = NULL;
    if (!(p = vfs_s_get_path (me, path, &archive, FL_NO_OPEN)))
	return (vfsid) -1;
    g_free(p);
    v = vfs_get_class (archive->name);
    id = (*v->getid) (v, archive->name, &par);
    if (id != (vfsid)-1){
        *parent = g_new (struct vfs_stamping, 1);
        (*parent)->v = v;
        (*parent)->id = id;
        (*parent)->parent = par;
        (*parent)->next = NULL;
    }
    return (vfsid) archive;    
}

static int
vfs_s_nothingisopen (vfsid id)
{
  /* Our data structures should survive free of superblock at any time */
    return 1;
}

static void
vfs_s_free (vfsid id)
{
    vfs_s_free_super (((struct vfs_s_super *)id)->me, (struct vfs_s_super *)id);
}

void
vfs_s_init_class (struct vfs_class *vclass)
{
    vclass->fill_names = vfs_s_fill_names;
    vclass->open = vfs_s_open;
    vclass->close = vfs_s_close;
    vclass->read = vfs_s_read;
    vclass->write = vfs_s_write;
    vclass->opendir = vfs_s_opendir;
    vclass->readdir = vfs_s_readdir;
    vclass->closedir = vfs_s_closedir;
    vclass->stat = vfs_s_stat;
    vclass->lstat = vfs_s_lstat;
    vclass->fstat = vfs_s_fstat;
    vclass->readlink = vfs_s_readlink;
    vclass->chdir = vfs_s_chdir;
    vclass->ferrno = vfs_s_ferrno;
    vclass->lseek = vfs_s_lseek;
    vclass->getid = vfs_s_getid;
    vclass->nothingisopen = vfs_s_nothingisopen;
    vclass->free = vfs_s_free;
    vclass->getlocalcopy = vfs_s_getlocalcopy;
    vclass->setctl = vfs_s_setctl;
}

/* ----------- Utility functions for networked filesystems  -------------- */

#ifdef USE_NETCODE
int
vfs_s_select_on_two (int fd1, int fd2)
{
    fd_set set;
    struct timeval timeout;
    int v;
    int maxfd = (fd1 > fd2 ? fd1 : fd2) + 1;

    timeout.tv_sec  = 1;
    timeout.tv_usec = 0;
    FD_ZERO (&set);
    FD_SET (fd1, &set);
    FD_SET (fd2, &set);
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
vfs_s_get_line (struct vfs_class *me, int sock, char *buf, int buf_len, char term)
{
    FILE *logfile = MEDATA->logfile;
    int i, status;
    char c;

    for (i = 0; i < buf_len - 1; i++, buf++){
	if (read (sock, buf, sizeof(char)) <= 0)
	    return 0;
	if (logfile){
	    fwrite (buf, 1, 1, logfile);
	    fflush (logfile);
	}
	if (*buf == term){
	    *buf = 0;
	    return 1;
	}
    }

    /* Line is too long - terminate buffer and discard the rest of line */
    *buf = 0;
    while ((status = read (sock, &c, sizeof (c))) > 0){
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
vfs_s_get_line_interruptible (struct vfs_class *me, char *buffer, int size, int fd)
{
    int n;
    int i;

    enable_interrupt_key ();
    for (i = 0; i < size-1; i++){
	n = read (fd, buffer+i, 1);
	disable_interrupt_key ();
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
#endif /* USE_NETCODE */
