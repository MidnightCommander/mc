/*
 * Large portions of tar.c & extfs.c were nearly same. I killed this 
 * redundancy, so code is maintainable, again.
 *
 * Actually, tar no longer uses this code :-).
 *
 * 1998 Pavel Machek
 *
 * Namespace: no pollution.
 */

static char *get_path (char *inname, struct archive **archive, int is_dir,
    int do_not_open)
{
    char *buf = strdup( inname );
    char *res = get_path_mangle( buf, archive, is_dir, do_not_open );
    char *res2 = NULL;
    if (res)
        res2 = strdup(res);
    free(buf);
    return res2;
}

static struct entry*
__find_entry (struct entry *dir, char *name, 
		     struct loop_protect *list, int make_dirs, int make_file)
{
    struct entry *pent, *pdir;
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
		if ((pent = __resolve_symlinks (pent, list))==NULL){
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
    	my_errno = ENOENT;
    return pent;
}

static struct entry *find_entry (struct entry *dir, char *name, int make_dirs, int make_file)
{
    struct entry *res;
    
    errloop = 0;
    notadir = 0;
    res = __find_entry (dir, name, NULL, make_dirs, make_file);
    if (res == NULL) {
    	if (errloop)
    	    my_errno = ELOOP;
    	else if (notadir)
    	    my_errno = ENOTDIR;
    }
    return res;
}


static int s_errno (vfs *me)
{
    return my_errno;
}

static void * s_opendir (vfs *me, char *dirname)
{
    struct archive *archive;
    char *q;
    struct entry *entry;
    struct entry **info;

    if ((q = get_path_mangle (dirname, &archive, 1, 0)) == NULL)
	return NULL;
    entry = find_entry (archive->root_entry, q, 0, 0);
    if (entry == NULL)
    	return NULL;
    if ((entry = my_resolve_symlinks (entry)) == NULL)
	return NULL;
    if (!S_ISDIR (entry->inode->mode)) ERRNOR (ENOTDIR, NULL);

    info = (struct entry **) xmalloc (2*sizeof (struct entry *), "shared opendir");
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

    struct entry **info = (struct entry **) data;

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
    struct entry **info = (struct entry **) data;
    struct entry *cur;
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
    struct entry **info = (struct entry **) data;
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

static void stat_move( struct stat *buf, struct inode *inode )
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
    struct archive *archive;
    char *q;
    struct entry *entry;
    struct inode *inode;

    if ((q = get_path_mangle (path, &archive, 0, 0)) == NULL)
	return -1;
    entry = find_entry (archive->root_entry, q, 0, 0);
    if (entry == NULL)
    	return -1;
    if (resolve && (entry = my_resolve_symlinks (entry)) == NULL)
	return -1;
    inode = entry->inode;
    stat_move( buf, inode );
    return 0;
}

static int s_stat (vfs *me, char *path, struct stat *buf)
{
    return s_internal_stat (path, buf, 1);
}

static int s_lstat (vfs *me, char *path, struct stat *buf)
{
    return s_internal_stat (path, buf, 0);
}

static int s_fstat (void *data, struct stat *buf)
{
    struct pseudofile *file = (struct pseudofile *)data;
    struct inode *inode;
    
    inode = file->entry->inode;
    stat_move( buf, inode );
    return 0;
}

static int s_readlink (vfs *me, char *path, char *buf, int size)
{
    struct archive *archive;
    char *q;
    int i;
    struct entry *entry;

    if ((q = get_path_mangle (path, &archive, 0, 0)) == NULL)
	return -1;
    entry = find_entry (archive->root_entry, q, 0, 0);
    if (entry == NULL)
    	return -1;
    if (!S_ISLNK (entry->inode->mode)) ERRNOR (EINVAL, -1);
    if (size > (i = strlen (entry->inode->linkname))) {
    	size = i;
    }
    strncpy (buf, entry->inode->linkname, i);
    return i;
}
