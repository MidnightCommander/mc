/*
 * Large portions of tar.c & extfs.c were nearly same. I killed this 
 * redundancy, so code is maintainable, again.
 *
 * 1998 Pavel Machek
 */

static struct X_entry*
__X_find_entry (struct X_entry *dir, char *name, 
		     struct X_loop_protect *list, int make_dirs, int make_file)
{
    struct X_entry *pent, *pdir;
    char *p, *q, *name_end;
    char c;

    if (*name == '/') { /* Handle absolute paths */
    	name++;
    	dir = dir->inode->archive->root_entry;
    }

    pent = dir;
    p = name;
    name_end = name + strlen (name);
    q = strchr (p, '/');
    c = '/';
    if (!q)
	q = strchr (p, 0);
    
    for (; pent != NULL && c && *p; ){
	c = *q;
	*q = 0;

	if (strcmp (p, ".")){
	    if (!strcmp (p, "..")) 
		pent = pent->dir;
	    else {
		if ((pent = __X_resolve_symlinks (pent, list))==NULL){
		    *q = c;
		    return NULL;
		}
		if (c == '/' && !S_ISDIR (pent->inode->mode)){
		    *q = c;
		    notadir = 1;
		    return NULL;
		}
		pdir = pent;
		for (pent = pent->inode->first_in_subdir; pent; pent = pent->next_in_dir)
		    /* Hack: I keep the original semanthic unless
		       q+1 would break in the strchr */
		    if (!strcmp (pent->name, p)){
			if (q + 1 > name_end){
			    *q = c;
			    notadir = !S_ISDIR (pent->inode->mode);
			    return pent;
			}
			break;
		    }
		
		/* When we load archive, we create automagically
		 * non-existant directories
		 */
		if (pent == NULL && make_dirs) { 
		    pent = generate_entry (dir->inode->archive, p, pdir, S_IFDIR | 0777);
		}
		if (pent == NULL && make_file) { 
		    pent = generate_entry (dir->inode->archive, p, pdir, 0777);
		}
	    }
	}
	/* Next iteration */
	*q = c;
	p = q + 1;
	q = strchr (p, '/');
	if (!q)
	    q = strchr (p, 0);
    }
    if (pent == NULL)
    	Xerrno = ENOENT;
    return pent;
}

static struct X_entry *X_find_entry (struct X_entry *dir, char *name, int make_dirs, int make_file)
{
    struct X_entry *res;
    
    errloop = 0;
    notadir = 0;
    res = __X_find_entry (dir, name, NULL, make_dirs, make_file);
    if (res == NULL) {
    	if (errloop)
    	    Xerrno = ELOOP;
    	else if (notadir)
    	    Xerrno = ENOTDIR;
    }
    return res;
}


static int s_errno (void)
{
    return Xerrno;
}

static void * s_opendir (char *dirname)
{
    struct X_archive *archive;
    char *q;
    struct X_entry *entry;
    struct X_entry **info;

    if ((q = X_get_path (dirname, &archive, 1, 0)) == NULL)
	return NULL;
    entry = X_find_entry (archive->root_entry, q, 0, 0);
    if (entry == NULL)
    	return NULL;
    if ((entry = X_resolve_symlinks (entry)) == NULL)
	return NULL;
    if (!S_ISDIR (entry->inode->mode)) {
    	Xerrno = ENOTDIR;
    	return NULL;
    }

    info = (struct X_entry **) xmalloc (2*sizeof (struct X_entry *), "shared opendir");
    info[0] = entry->inode->first_in_subdir;
    info[1] = entry->inode->first_in_subdir;

    return info;
}

static void * s_readdir (void *data)
{
    static struct {
	struct dirent dir; 
#ifdef NEED_EXTRA_DIRENT_BUFFER
	char extra_buffer [MC_MAXPATHLEN];
#endif
    } dir;

    struct X_entry **info = (struct X_entry **) data;

    if (!*info)
    	return NULL;

    strcpy (&(dir.dir.d_name [0]), (*info)->name);
    
#ifndef DIRENT_LENGTH_COMPUTED
    dir.d_namlen = strlen (dir.dir.d_name);
#endif
    *info = (*info)->next_in_dir;
    
    return (void *)&dir;
}

static int s_telldir (void *data)
{
    struct X_entry **info = (struct X_entry **) data;
    struct X_entry *cur;
    int num = 0;

    cur = info[1];
    while (cur!=NULL) {
        if (cur == info[0]) return num;
	num++;
	cur = cur->next_in_dir;
    }
    return -1;
}

static void s_seekdir (void *data, int offset)
{
    struct X_entry **info = (struct X_entry **) data;
    int i;
    info[0] = info[1];
    for (i=0; i<offset; i++)
        s_readdir( data );
}

static int s_closedir (void *data)
{
    free (data);
    return 0;
}

static void stat_move( struct stat *buf, struct X_inode *inode )
{
    buf->st_dev = inode->dev;
    buf->st_ino = inode->inode;
    buf->st_mode = inode->mode;
    buf->st_nlink = inode->nlink;
    buf->st_uid = inode->uid;
    buf->st_gid = inode->gid;
#ifdef HAVE_ST_RDEV
    buf->st_rdev = inode->rdev;
#endif
    buf->st_size = inode->size;
#ifdef HAVE_ST_BLKSIZE
    buf->st_blksize = RECORDSIZE;
#endif
#ifdef HAVE_ST_BLOCKS
    buf->st_blocks = (inode->size + RECORDSIZE - 1) / RECORDSIZE;
#endif
    buf->st_atime = inode->atime;
    buf->st_mtime = inode->mtime;
    buf->st_ctime = inode->ctime;
}

static int s_internal_stat (char *path, struct stat *buf, int resolve)
{
    struct X_archive *archive;
    char *q;
    struct X_entry *entry;
    struct X_inode *inode;
char debugbuf[10240];
strcpy( debugbuf, path );


    if ((q = X_get_path (path, &archive, 0, 0)) == NULL)
	return -1;
    entry = X_find_entry (archive->root_entry, q, 0, 0);
    if (entry == NULL)
    	return -1;
    if (resolve && (entry = X_resolve_symlinks (entry)) == NULL)
	return -1;
    inode = entry->inode;
    stat_move( buf, inode );
    return 0;
}

static int s_stat (char *path, struct stat *buf)
{
    return s_internal_stat (path, buf, 1);
}

static int s_lstat (char *path, struct stat *buf)
{
    return s_internal_stat (path, buf, 0);
}

static int s_fstat (void *data, struct stat *buf)
{
    struct X_pseudofile *file = (struct X_pseudofile *)data;
    struct X_inode *inode;
    
    inode = file->entry->inode;
    stat_move( buf, inode );
    return 0;
}

static int s_readlink (char *path, char *buf, int size)
{
    struct X_archive *archive;
    char *q;
    int i;
    struct X_entry *entry;

    if ((q = X_get_path (path, &archive, 0, 0)) == NULL)
	return -1;
    entry = X_find_entry (archive->root_entry, q, 0, 0);
    if (entry == NULL)
    	return -1;
    if (!S_ISLNK (entry->inode->mode)) {
        Xerrno = EINVAL;
        return -1;
    }
    if (size > (i = strlen (entry->inode->linkname))) {
    	size = i;
    }
    strncpy (buf, entry->inode->linkname, i);
    return i;
}
