/*
 * Shared code between the fish.c and the ftp.c file systems
 *
 * Actually, this code is not being used by fish.c any more :-).
 *
 * Namespace pollution: X_hint_reread, X_flushdir.
 */
static int         store_file           (struct direntry *fe);
static int         retrieve_file        (struct direntry *fe);
static int         remove_temp_file     (char *file_name);
static struct dir *retrieve_dir         (struct connection *bucket,
					 char *remote_path,
					 int resolve_symlinks);
static void	   my_forget		(char *path);

static int         linear_start         (struct direntry *fe, int from);
static int         linear_read          (struct direntry *fe, void *buf, int len);
static void        linear_close         (struct direntry *fe);

static int
select_on_two (int fd1, int fd2)
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

static int
get_line (int sock, char *buf, int buf_len, char term)
{
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

static void
direntry_destructor (void *data)
{
    struct direntry *fe = data;
    
    fe->count--;
	
    if (fe->count > 0)
        return;
    free(fe->name);
    if (fe->linkname)
	free(fe->linkname);
    if (fe->local_filename) {
        if (fe->local_is_temp) {
            if (!fe->local_stat.st_mtime)
	        unlink(fe->local_filename);
	    else {
	        struct stat sb;
	        
	        if (stat (fe->local_filename, &sb) >=0 && 
	            fe->local_stat.st_mtime == sb.st_mtime)
	            unlink (fe->local_filename); /* Delete only if it hasn't changed */
	    }
	}
	free(fe->local_filename);
	fe->local_filename = NULL;
    }
    if (fe->remote_filename)
	free(fe->remote_filename);
    if (fe->l_stat)
	free(fe->l_stat);
    free(fe);
}

static void
dir_destructor(void *data)
{
    struct dir *fd = data;

    fd->count--;
    if (fd->count > 0) 
	return;
    free(fd->remote_path);
    linklist_destroy(fd->file_list, direntry_destructor);
    free(fd);
}

static int
get_line_interruptible (char *buffer, int size, int fd)
{
    int n;
    int i = 0;

    for (i = 0; i < size-1; i++) {
	n = read (fd, buffer+i, 1);
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

static void
free_bucket (void *data)
{
    struct connection *bucket = data;

    free(qhost(bucket));
    free(quser(bucket));
    if (qcdir(bucket))
	free(qcdir(bucket));
    if (qhome(bucket))
    	free(qhome(bucket));
    if (qupdir(bucket))
        free(qupdir(bucket));
    if (bucket->password)
	wipe_password (bucket->password);
    linklist_destroy(qdcache(bucket), dir_destructor);
    free(bucket);
}


static void
connection_destructor(void *data)
{
    connection_close (data);
    free_bucket (data);
}

static void
flush_all_directory(struct connection *bucket)
{
    linklist_delete_all(qdcache(bucket), dir_destructor);
}

static void X_fill_names (vfs *me, void (*func)(char *))
{
    struct linklist *lptr;
    char   *path_name;
    struct connection *bucket;
    
    if (!connections_list)
	return;
    lptr = connections_list;
    do {
	if ((bucket = lptr->data) != 0){

	    path_name = copy_strings ( X_myname, quser (bucket),
				      "@",      qhost (bucket), 
				      qcdir(bucket), 0);
	    (*func)(path_name);
	    free (path_name);
	}
	lptr = lptr->next;
    } while (lptr != connections_list);
}

/* get_path:
 * makes BUCKET point to the connection bucket descriptor for PATH
 * returns a malloced string with the pathname relative to BUCKET.
 *
 * path must _not_ contain initial /bla/#ftp:
 */
static char*
s_get_path (struct connection **bucket, char *path, char *name)
{
    char *user, *host, *remote_path, *pass;
    int port;

#ifndef BROKEN_PATHS
    if (strncmp (path, name, strlen (name)))
        return NULL; 	/* Normal: consider cd /bla/#ftp */ 
#else
    if (!(path = strstr (path, name)))
        return NULL;
#endif    
    path += strlen (name);

    if (!(remote_path = my_get_host_and_username (path, &host, &user, &port, &pass)))
        my_errno = ENOENT;
    else {
        if ((*bucket = open_link (host, user, port, pass)) == NULL) {
            free (remote_path);
	    remote_path = NULL;
	}
    }
    free (host);
    free (user);
    if (pass)
        wipe_password (pass);

    if (!remote_path) 
        return NULL;

    /* NOTE: Usage of tildes is deprecated, consider:
     * cd /#ftp:pavel@hobit
     * cd ~
     * And now: what do I want to do? Do I want to go to /home/pavel or to
     * /#ftp:hobit/home/pavel? I think first has better sense...
     */
    {
        int f = !strcmp( remote_path, "/~" );
	if (f || !strncmp( remote_path, "/~/", 3 )) {
	    char *s;
	    s = concat_dir_and_file( qhome (*bucket), remote_path +3-f );
	    free (remote_path);
	    remote_path = s;
	}
    }
    return remote_path;
}

void
X_flushdir (void)
{
    force_expiration = 1;
}


static int 
s_setctl (vfs *me, char *path, int ctlop, char *arg)
{
    switch (ctlop) {
	case MCCTL_REMOVELOCALCOPY:
	    return remove_temp_file (path);
            return 0;
        case MCCTL_FORGET_ABOUT:
	    my_forget(path);
	    return 0;
    }
    return 0;
}

static struct direntry *
_get_file_entry(struct connection *bucket, char *file_name, 
		int op, int flags)
{
    char *p, q;
    struct direntry *ent;
    struct linklist *file_list, *lptr;
    struct dir *dcache;
    struct stat sb;

    p = strrchr(file_name, '/');
    q = *p;
    *p = '\0';
    dcache = retrieve_dir(bucket, *file_name ? file_name : "/", op & DO_RESOLVE_SYMLINK);
    if (dcache == NULL)
        return NULL;
    file_list = dcache->file_list;
    *p++ = q;
    if (!*p) 
        p = ".";
    for (lptr = file_list->next; lptr != file_list; lptr = lptr->next) {
	ent = lptr->data;
        if (strcmp(p, ent->name) == 0) {
	    if (S_ISLNK(ent->s.st_mode) && (op & DO_RESOLVE_SYMLINK)) {
		if (ent->l_stat == NULL) ERRNOR (ENOENT, NULL);
		if (S_ISLNK(ent->l_stat->st_mode)) ERRNOR (ELOOP, NULL);
	    }
	    if (ent && (op & DO_OPEN)) {
		mode_t fmode;

		fmode = S_ISLNK(ent->s.st_mode)
		    ? ent->l_stat->st_mode
		    : ent->s.st_mode;
		if (S_ISDIR(fmode)) ERRNOR (EISDIR, NULL);
		if (!S_ISREG(fmode)) ERRNOR (EPERM, NULL);
		if ((flags & O_EXCL) && (flags & O_CREAT)) ERRNOR (EEXIST, NULL);
		if (ent->remote_filename == NULL)
		    if (!(ent->remote_filename = strdup(file_name))) ERRNOR (ENOMEM, NULL);
		if (ent->local_filename == NULL || 
		    !ent->local_stat.st_mtime || 
		    stat (ent->local_filename, &sb) < 0 || 
		    sb.st_mtime != ent->local_stat.st_mtime) {
		    int handle;
		    
		    if (ent->local_filename){
		        free (ent->local_filename);
			ent->local_filename = NULL;
		    }
		    if (flags & O_TRUNC) {
			ent->local_filename = tempnam (NULL, X "fs");
			if (ent->local_filename == NULL) ERRNOR (ENOMEM, NULL);
			handle = open(ent->local_filename, O_CREAT | O_TRUNC | O_RDWR | O_EXCL, 0600);
			if (handle < 0) ERRNOR (EIO, NULL);
			close(handle);
			if (stat (ent->local_filename, &ent->local_stat) < 0)
			    ent->local_stat.st_mtime = 0;
		    }
		    else {
		        if (IS_LINEAR(flags)) {
		            ent->local_is_temp = 0;
		            ent->local_filename = NULL;
			    ent->linear_state = LS_LINEAR_CLOSED;
		            return ent;
		        }
		        if (!retrieve_file(ent))
			    return NULL;
		    }
		}
		else if (flags & O_TRUNC) {
			truncate(ent->local_filename, 0);
		}
	    }
	    return ent;
	}
    }
    if ((op & DO_OPEN) && (flags & O_CREAT)) {
	int handle;

	ent = xmalloc(sizeof(struct direntry), "struct direntry");
	ent->freshly_created = 0;
	if (ent == NULL) ERRNOR (ENOMEM, NULL);
	ent->count = 1;
	ent->linkname = NULL;
	ent->l_stat = NULL;
	ent->bucket = bucket;
	ent->name = strdup(p);
	ent->remote_filename = strdup(file_name);
	ent->local_filename = tempnam (NULL, X "fs");
	if (!ent->name && !ent->remote_filename && !ent->local_filename) {
	    direntry_destructor(ent);
	    ERRNOR (ENOMEM, NULL);
	}
        handle = creat(ent->local_filename, 0700);
	if (handle == -1) {
	    my_errno = EIO;
	    goto error;
	}
	fstat(handle, &ent->s);
	close(handle);
#if 0
/* This is very wrong - like this a zero length file will be always created
   and usually preclude uploading anything more desirable */	
#if defined(UPLOAD_ZERO_LENGTH_FILE)
	if (!store_file(ent)) goto error;
#endif
#endif
	if (!linklist_insert(file_list, ent)) {
	    my_errno = ENOMEM;
	    goto error;
	}
	ent->freshly_created = 1;
	return ent;
    }
    else ERRNOR (ENOENT, NULL);
error:
    direntry_destructor(ent);
    return NULL;
}


/* this just free's the local temp file. I don't know if the
   remote file can be used after this without crashing - paul
   psheer@obsidian.co.za psheer@icon.co.za */
static int
remove_temp_file (char *file_name)
{
    char *p, q;
    struct connection *bucket;
    struct direntry *ent;
    struct linklist *file_list, *lptr;
    struct dir *dcache;

    if (!(file_name = get_path (&bucket, file_name)))
	return -1;
    p = strrchr (file_name, '/');
    q = *p;
    *p = '\0';
    dcache = retrieve_dir (bucket, *file_name ? file_name : "/", 0);
    if (dcache == NULL)
	return -1;
    file_list = dcache->file_list;
    *p++ = q;
    if (!*p)
	p = ".";
    for (lptr = file_list->next; lptr != file_list; lptr = lptr->next) {
	ent = lptr->data;
	if (strcmp (p, ent->name) == 0) {
	    if (ent->local_filename) {
		unlink (ent->local_filename);
		free (ent->local_filename);
		ent->local_filename = NULL;
		return 0;
	    }
	}
    }
    return -1;
}

static struct direntry *
get_file_entry(char *path, int op, int flags)
{
    struct connection *bucket;
    struct direntry *fe;
    char *remote_path;

    if (!(remote_path = get_path (&bucket, path)))
	return NULL;
    fe = _get_file_entry(bucket, remote_path, op,
			 flags);
    free(remote_path);
#if 0
    if (op & DO_FREE_RESOURCE)
	vfs_add_noncurrent_stamps (&vfs_X_ops, (vfsid) bucket, NULL);
#endif
    return fe;
}

#define OPT_FLUSH        1
#define OPT_IGNORE_ERROR 2

static int normal_flush = 1;

void X_hint_reread(int reread)
{
    if (reread)
	normal_flush++;
    else
	normal_flush--;
}


/* The callbacks */

struct filp {
    unsigned int has_changed:1;
    struct direntry *fe;
    int local_handle;
};

static void *s_open (vfs *me, char *file, int flags, int mode)
{
    struct filp *fp;
    struct direntry *fe;

    fp = xmalloc(sizeof(struct filp), "struct filp");
    if (fp == NULL) ERRNOR (ENOMEM, NULL);
    fe = get_file_entry(file, DO_OPEN | DO_RESOLVE_SYMLINK, flags);
    if (!fe) {
	free(fp);
        return NULL;
    }
    fe->linear_state = IS_LINEAR(flags);
    if (!fe->linear_state) {
        fp->local_handle = open(fe->local_filename, flags, mode);
        if (fp->local_handle < 0) {
	    free(fp);
	    ERRNOR (errno, NULL);
        }
    } else fp->local_handle = -1;
#ifdef UPLOAD_ZERO_LENGTH_FILE        
    fp->has_changed = fe->freshly_created;
#else
    fp->has_changed = 0;
#endif
    fp->fe = fe;
    qlock(fe->bucket)++;
    fe->count++;
    return fp;
}

static int s_read (void *data, char *buffer, int count)
{
    struct filp *fp;
    int n;
    
    fp = data;
    if (fp->fe->linear_state == LS_LINEAR_CLOSED) {
        print_vfs_message ("Starting linear transfer...");
	if (!linear_start (fp->fe, 0))
	    return -1;
    }

    if (fp->fe->linear_state == LS_LINEAR_CLOSED)
        vfs_die ("linear_start() did not set linear_state!");

    if (fp->fe->linear_state == LS_LINEAR_OPEN)
        return linear_read (fp->fe, buffer, count);
        
    n = read (fp->local_handle, buffer, count);
    if (n < 0)
        my_errno = errno;
    return n;
}

static int s_write (void *data, char *buf, int nbyte)
{
    struct filp *fp = data;
    int n;

    if (fp->fe->linear_state)
        vfs_die ("You may not write to linear file");
    n = write (fp->local_handle, buf, nbyte);
    if (n < 0)
        my_errno = errno;
    fp->has_changed = 1;
    return n;
}

static int s_close (void *data)
{
    struct filp *fp = data;
    int result = 0;

    if (fp->has_changed) {
	if (!store_file(fp->fe))
	    result = -1;
	if (normal_flush)
	    flush_all_directory(fp->fe->bucket);
    }
    if (fp->fe->linear_state == LS_LINEAR_OPEN)
        linear_close(fp->fe);
    if (fp->local_handle >= 0)
        close(fp->local_handle);
    qlock(fp->fe->bucket)--;
    direntry_destructor(fp->fe);
    free(fp);
    return result;
}

static int s_errno (vfs *me)
{
    return my_errno;
}


/* Explanation:
 * On some operating systems (Slowaris 2 for example)
 * the d_name member is just a char long (nice trick that break everything),
 * so we need to set up some space for the filename.
 */
struct my_dirent {
    struct dirent dent;
#ifdef NEED_EXTRA_DIRENT_BUFFER
    char extra_buffer [MC_MAXPATHLEN];
#endif
    struct linklist *pos;
    struct dir *dcache;
};

/* Possible FIXME: what happens if one directory is opened twice ? */

static void *s_opendir (vfs *me, char *dirname)
{
    struct connection *bucket;
    char *remote_path;
    struct my_dirent *dirp;

    if (!(remote_path = get_path (&bucket, dirname)))
        return NULL;
    dirp = xmalloc(sizeof(struct my_dirent), "struct my_dirent");
    if (dirp == NULL) {
	my_errno = ENOMEM;
	goto error_return;
    }
    dirp->dcache = retrieve_dir(bucket, remote_path, 1);
    if (dirp->dcache == NULL)
        goto error_return;
    dirp->pos = dirp->dcache->file_list->next;
    free(remote_path);
    dirp->dcache->count++;
    return (void *)dirp;
error_return:
    vfs_add_noncurrent_stamps (&vfs_X_ops, (vfsid) bucket, NULL);
    free(remote_path);
    free(dirp);
    return NULL;
}

static void *s_readdir (void *data)
{
    struct direntry *fe;
    struct my_dirent *dirp = data;
    
    if (dirp->pos == dirp->dcache->file_list)
	return NULL;
    fe = dirp->pos->data;
    strcpy (&(dirp->dent.d_name [0]), fe->name);
#ifndef DIRENT_LENGTH_COMPUTED
    dirp->d_namlen = strlen (dirp->d_name);
#endif
    dirp->pos = dirp->pos->next;
    return (void *) &dirp->dent;
}

static int s_telldir (void *data)
{
    struct my_dirent *dirp = data;
    struct linklist *pos;
    int i = 0;
    
    pos = dirp->dcache->file_list->next;
    while( pos!=dirp->dcache->file_list) {
    	if (pos == dirp->pos)
	    return i;
	pos = pos->next;
	i++;
    }
    return -1;
}

static void s_seekdir (void *data, int pos)
{
    struct my_dirent *dirp = data;
    int i;

    dirp->pos = dirp->dcache->file_list->next;
    for (i=0; i<pos; i++)
        s_readdir(data);
}

static int s_closedir (void *info)
{
    struct my_dirent *dirp = info;
    dir_destructor(dirp->dcache);
    free(dirp);
    return 0;
}

static int s_lstat (vfs *me, char *path, struct stat *buf)
{
    struct direntry *fe;
    
    fe = get_file_entry(path, DO_FREE_RESOURCE, 0);
    if (fe) {
	*buf = fe->s;
	return 0;
    }
    else
        return -1;
}

static int s_stat (vfs *me, char *path, struct stat *buf)
{
    struct direntry *fe;

    fe = get_file_entry(path, DO_RESOLVE_SYMLINK | DO_FREE_RESOURCE, 0);
    if (fe) {
	if (!S_ISLNK(fe->s.st_mode))
	    *buf = fe->s;
	else
	    *buf = *fe->l_stat;
	return 0;
    }
    else
        return -1;
}

static int s_fstat (void *data, struct stat *buf)
{
    struct filp *fp = data;

    if (!S_ISLNK(fp->fe->s.st_mode))
	*buf = fp->fe->s;
    else
	*buf = *fp->fe->l_stat;
    return 0;
}

static int s_readlink (vfs *me, char *path, char *buf, int size)
{
    struct direntry *fe;

    fe = get_file_entry(path, DO_FREE_RESOURCE, 0);
    if (!fe)
	return -1;
    if (!S_ISLNK(fe->s.st_mode)) ERRNOR (EINVAL, -1);
    if (fe->linkname == NULL) ERRNOR (EACCES, -1);
    if (strlen(fe->linkname) >= size) ERRNOR (ERANGE, -1);
    strncpy(buf, fe->linkname, size);
    return strlen(fe->linkname);
}

static int s_chdir (vfs *me, char *path)
{
    char *remote_path;
    struct connection *bucket;

    if (!(remote_path = get_path(&bucket, path)))
	return -1;
    if (qcdir(bucket))
        free(qcdir(bucket));
    qcdir(bucket) = remote_path;
    bucket->cwd_defered = 1;
    
    vfs_add_noncurrent_stamps (&vfs_X_ops, (vfsid) bucket, NULL);
    return 0;
}

static int s_lseek (void *data, off_t offset, int whence)
{
    struct filp *fp = data;

    if (fp->fe->linear_state == LS_LINEAR_OPEN)
        vfs_die ("You promissed not to seek!");
    if (fp->fe->linear_state == LS_LINEAR_CLOSED) {
        print_vfs_message ("Preparing reget...");
        if (whence != SEEK_SET)
	    vfs_die ("You may not do such seek on linear file");
	if (!linear_start (fp->fe, offset))
	    return -1;
	return offset;
    }
    return lseek(fp->local_handle, offset, whence);
}

static vfsid s_getid (vfs *me, char *p, struct vfs_stamping **parent)
{
    struct connection *bucket;
    char *remote_path;

    *parent = NULL; /* We are not enclosed in any other fs */
    
    if (!(remote_path = get_path (&bucket, p)))
        return (vfsid) -1;
    else {
	free(remote_path);
    	return (vfsid) bucket;
    }
}

static int s_nothingisopen (vfsid id)
{
    return qlock((struct connection *)id) == 0;
}

static void s_free (vfsid id)
{
    struct connection *bucket = (struct connection *) id;
 
    connection_destructor(bucket);
    linklist_delete(connections_list, bucket);
}

static char *s_getlocalcopy (vfs *me, char *path)
{
    struct filp *fp = (struct filp *) s_open (me, path, O_RDONLY, 0);
    char *p;
    
    if (fp == NULL)
        return NULL;
    if (fp->fe->local_filename == NULL) {
        s_close ((void *) fp);
        return NULL;
    }
    p = strdup (fp->fe->local_filename);
    qlock(fp->fe->bucket)++;
    fp->fe->count++;
    s_close ((void *) fp);
    return p;
}

static void s_ungetlocalcopy (vfs *me, char *path, char *local, int has_changed)
{
    struct filp *fp = (struct filp *) s_open (me, path, O_WRONLY, 0);
    
    if (fp == NULL)
        return;
    if (!strcmp (fp->fe->local_filename, local)) {
        fp->has_changed = has_changed;
        qlock(fp->fe->bucket)--;
        direntry_destructor(fp->fe);
        s_close ((void *) fp);
    } else {
        /* Should not happen */
        s_close ((void *) fp);
        mc_def_ungetlocalcopy (me, path, local, has_changed);
    }
}

static void
X_done(vfs *me)
{
    linklist_destroy(connections_list, connection_destructor);
    connections_list = NULL;
    if (logfile)
	fclose (logfile);
    logfile = NULL;
}

static int retrieve_file(struct direntry *fe)
{
    /* If you want reget, you'll have to open file with O_LINEAR */
    int total = 0;
    char buffer[8192];
    int local_handle, n;
    int stat_size = fe->s.st_size;
    
    if (fe->local_filename)
        return 1;
    if (!(fe->local_filename = tempnam (NULL, X))) ERRNOR (ENOMEM, 0);
    fe->local_is_temp = 1;

    local_handle = open(fe->local_filename, O_RDWR | O_CREAT | O_TRUNC | O_EXCL, 0600);
    if (local_handle == -1) {
	my_errno = EIO;
	goto error_4;
    }

    if (!linear_start (fe, 0))
        goto error_3;

    /* Clear the interrupt status */
    enable_interrupt_key ();
    
    while (1) {
	if ((n = linear_read(fe, buffer, sizeof(buffer))) < 0)
	    goto error_1;
	if (!n)
	    break;

	total += n;
	vfs_print_stats (X, "Getting file", fe->remote_filename, total, stat_size);

        while (write(local_handle, buffer, n) < 0) {
	    if (errno == EINTR) {
		if (got_interrupt()) {
		    my_errno = EINTR;
		    goto error_2;
		}
		else
		    continue;
	    }
	    my_errno = errno;
	    goto error_1;
	}
    }
    linear_close(fe);
    disable_interrupt_key();
    close(local_handle);

    if (stat (fe->local_filename, &fe->local_stat) < 0)
        fe->local_stat.st_mtime = 0;
    
    return 1;
error_1:
error_2:
    linear_close(fe);
error_3:
    disable_interrupt_key();
    close(local_handle);
    unlink(fe->local_filename);
error_4:
    free(fe->local_filename);
    fe->local_filename = NULL;
    return 0;
}
