/* Virtual File System: External file system.
   Copyright (C) 1995 The Free Software Foundation
   
   Written by: 1995 Jakub Jelinek
   Rewritten by: 1998 Pavel Machek
   Additional changes by: 1999 Andrew T. Veliath

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

/* Namespace: exports only vfs_extfs_ops */

#include <config.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include <errno.h>
#include "utilvfs.h"
#include "../src/dialog.h"
#include "../src/main.h"	/* For shell_execute */
#include "xdirentry.h"
#include "vfs.h"
#include "extfs.h"

#undef ERRNOR
#define ERRNOR(x,y) do { my_errno = x; return y; } while(0)

struct inode {
    nlink_t nlink;
    struct entry *first_in_subdir; /* only used if this is a directory */
    struct entry *last_in_subdir;
    ino_t inode;        /* This is inode # */
    dev_t dev;		/* This is an internal identification of the extfs archive */
    struct archive *archive; /* And this is an archive structure */
    dev_t rdev;
    mode_t mode;
    uid_t uid;
    gid_t gid;
    int size;
    time_t mtime;
    char linkflag;
    char *linkname;
    time_t atime;
    time_t ctime;
    char *local_filename;
};

struct entry {
    struct entry *next_in_dir;
    struct entry *dir;
    char *name;
    struct inode *inode;
};

struct pseudofile {
    struct archive *archive;
    unsigned int has_changed:1;
    int local_handle;
    struct entry *entry;
};

static struct entry *
find_entry (struct entry *dir, char *name, int make_dirs, int make_file);
static int extfs_which (vfs *me, char *path);
static void remove_entry (struct entry *e);

static struct archive *first_archive = NULL;
static int my_errno = 0;

#define MAXEXTFS 32
static char *extfs_prefixes [MAXEXTFS];
static char extfs_need_archive [MAXEXTFS];
static int extfs_no = 0;

static void extfs_fill_names (vfs *me, void (*func)(char *))
{
    struct archive *a = first_archive;
    char *name;
    
    while (a){
	name = g_strconcat (a->name ? a->name : "",
			    "#", extfs_prefixes [a->fstype],
			    PATH_SEP_STR, a->current_dir->name, NULL);
	(*func)(name);
	g_free (name);
	a = a->next;
    }
}

static void make_dot_doubledot (struct entry *ent)
{
    struct entry *entry = g_new (struct entry, 1);
    struct entry *parentry = ent->dir;
    struct inode *inode = ent->inode, *parent;
    
    parent = (parentry != NULL) ? parentry->inode : NULL;
    entry->name = g_strdup (".");
    entry->inode = inode;
    entry->dir = ent;
    inode->local_filename = NULL;
    inode->first_in_subdir = entry;
    inode->last_in_subdir = entry;
    inode->nlink++;
    entry->next_in_dir = g_new (struct entry, 1);
    entry=entry->next_in_dir;
    entry->name = g_strdup ("..");
    inode->last_in_subdir = entry;
    entry->next_in_dir = NULL;
    if (parent != NULL) {
        entry->inode = parent;
        entry->dir = parentry;
        parent->nlink++;
    } else {
    	entry->inode = inode;
    	entry->dir = ent;
    	inode->nlink++;
    }
}

static struct entry *generate_entry (struct archive *archive, 
    char *name, struct entry *parentry, mode_t mode)
{
    mode_t myumask;
    struct inode *inode, *parent; 
    struct entry *entry;

    parent = (parentry != NULL) ? parentry->inode : NULL;
    entry = g_new (struct entry, 1);
    
    entry->name = g_strdup (name);
    entry->next_in_dir = NULL;
    entry->dir = parentry;
    if (parent != NULL) {
    	parent->last_in_subdir->next_in_dir = entry;
    	parent->last_in_subdir = entry;
    }
    inode = g_new (struct inode, 1);
    entry->inode = inode;
    inode->local_filename = NULL;
    inode->linkname = 0;
    inode->inode = (archive->__inode_counter)++;
    inode->dev = archive->rdev;
    inode->archive = archive;
    myumask = umask (022);
    umask (myumask);
    inode->mode = mode & ~myumask;
    mode = inode->mode;
    inode->rdev = 0;
    inode->uid = getuid ();
    inode->gid = getgid ();
    inode->size = 0;
    inode->mtime = time (NULL);
    inode->atime = inode->mtime;
    inode->ctime = inode->mtime;
    inode->nlink = 1;
    if (S_ISDIR (mode))
        make_dot_doubledot (entry);
    return entry;
}

static void free_entries (struct entry *entry)
{
    return;
}

static void free_archive (struct archive *archive)
{
    free_entries (archive->root_entry);
    if (archive->local_name != NULL) {
        struct stat my;
        
        mc_stat (archive->local_name, &my);
        mc_ungetlocalcopy (archive->name, archive->local_name, 
            archive->local_stat.st_mtime != my.st_mtime);
        /* ungetlocalcopy frees local_name for us */
    }
    if (archive->name)
	g_free (archive->name);
    g_free (archive);
}

static FILE *
open_archive (int fstype, char *name, struct archive **pparc)
{
    static dev_t __extfs_no = 0;
    FILE *result;
    mode_t mode;
    char *cmd;
    char *mc_extfsdir;
    struct stat mystat;
    struct archive *current_archive;
    struct entry *root_entry;
    char *local_name = NULL, *tmp = 0;
    int uses_archive = extfs_need_archive[fstype];

    if (uses_archive) {
	if (mc_stat (name, &mystat) == -1)
	    return NULL;
	if (!vfs_file_is_local (name)) {
	    local_name = mc_getlocalcopy (name);
	    if (local_name == NULL)
		return NULL;
	}
	tmp = name_quote (name, 0);
    }

    mc_extfsdir = concat_dir_and_file (mc_home, "extfs" PATH_SEP_STR);
    cmd =
	g_strconcat (mc_extfsdir, extfs_prefixes[fstype], " list ",
		     local_name ? local_name : tmp, NULL);
    if (tmp)
	g_free (tmp);
    g_free (mc_extfsdir);
    open_error_pipe ();
    result = popen (cmd, "r");
    g_free (cmd);
    if (result == NULL) {
	close_error_pipe (1, NULL);
	if (local_name)
	    mc_ungetlocalcopy (name, local_name, 0);
	return NULL;
    }

    current_archive = g_new (struct archive, 1);
    current_archive->fstype = fstype;
    current_archive->name = name ? g_strdup (name) : name;
    current_archive->local_name = local_name;

    if (local_name != NULL)
	mc_stat (local_name, &current_archive->local_stat);
    current_archive->__inode_counter = 0;
    current_archive->fd_usage = 0;
    current_archive->rdev = __extfs_no++;
    current_archive->next = first_archive;
    first_archive = current_archive;
    mode = mystat.st_mode & 07777;
    if (mode & 0400)
	mode |= 0100;
    if (mode & 0040)
	mode |= 0010;
    if (mode & 0004)
	mode |= 0001;
    mode |= S_IFDIR;
    root_entry = generate_entry (current_archive, "/", NULL, mode);
    root_entry->inode->uid = mystat.st_uid;
    root_entry->inode->gid = mystat.st_gid;
    root_entry->inode->atime = mystat.st_atime;
    root_entry->inode->ctime = mystat.st_ctime;
    root_entry->inode->mtime = mystat.st_mtime;
    current_archive->root_entry = root_entry;
    current_archive->current_dir = root_entry;

    *pparc = current_archive;

    return result;
}

/*
 * Main loop for reading an archive.
 * Returns 0 on success, -1 on error.
 */
static int
read_archive (int fstype, char *name, struct archive **pparc)
{
    FILE *extfsd;
    char *buffer;
    struct archive *current_archive;
    char *current_file_name, *current_link_name;

    if ((extfsd = open_archive (fstype, name, &current_archive)) == NULL) {
	message_3s (1, MSG_ERROR, _("Cannot open %s archive\n%s"),
		    extfs_prefixes[fstype], name);
	return -1;
    }

    buffer = g_malloc (4096);
    while (fgets (buffer, 4096, extfsd) != NULL) {
	struct stat hstat;

	current_link_name = NULL;
	if (vfs_parse_ls_lga
	    (buffer, &hstat, &current_file_name, &current_link_name)) {
	    struct entry *entry, *pent;
	    struct inode *inode;
	    char *p, *q, *cfn = current_file_name;

	    if (*cfn) {
		if (*cfn == '/')
		    cfn++;
		p = strchr (cfn, 0);
		if (p != cfn && *(p - 1) == '/')
		    *(p - 1) = 0;
		p = strrchr (cfn, '/');
		if (p == NULL) {
		    p = cfn;
		    q = strchr (cfn, 0);
		} else {
		    *(p++) = 0;
		    q = cfn;
		}
		if (S_ISDIR (hstat.st_mode)
		    && (!strcmp (p, ".") || !strcmp (p, "..")))
		    goto read_extfs_continue;
		pent = find_entry (current_archive->root_entry, q, 1, 0);
		if (pent == NULL) {
		    /* FIXME: Should clean everything one day */
		    g_free (buffer);
		    pclose (extfsd);
		    close_error_pipe (1, _("Inconsistent extfs archive"));
		    return -1;
		}
		entry = g_new (struct entry, 1);
		entry->name = g_strdup (p);
		entry->next_in_dir = NULL;
		entry->dir = pent;
		if (pent != NULL) {
		    if (pent->inode->last_in_subdir) {
			pent->inode->last_in_subdir->next_in_dir = entry;
			pent->inode->last_in_subdir = entry;
		    }
		}
		if (!S_ISLNK (hstat.st_mode) && current_link_name != NULL) {
		    pent =
			find_entry (current_archive->root_entry,
				    current_link_name, 0, 0);
		    if (pent == NULL) {
			/* FIXME: Should clean everything one day */
			g_free (buffer);
			pclose (extfsd);
			close_error_pipe (1,
					  _("Inconsistent extfs archive"));
			return -1;
		    } else {
			entry->inode = pent->inode;
			pent->inode->nlink++;
		    }
		} else {
		    inode = g_new (struct inode, 1);
		    entry->inode = inode;
		    inode->local_filename = NULL;
		    inode->inode = (current_archive->__inode_counter)++;
		    inode->nlink = 1;
		    inode->dev = current_archive->rdev;
		    inode->archive = current_archive;
		    inode->mode = hstat.st_mode;
#ifdef HAVE_ST_RDEV
		    inode->rdev = hstat.st_rdev;
#else
		    inode->rdev = 0;
#endif
		    inode->uid = hstat.st_uid;
		    inode->gid = hstat.st_gid;
		    inode->size = hstat.st_size;
		    inode->mtime = hstat.st_mtime;
		    inode->atime = hstat.st_atime;
		    inode->ctime = hstat.st_ctime;
		    if (current_link_name != NULL
			&& S_ISLNK (hstat.st_mode)) {
			inode->linkname = current_link_name;
			current_link_name = NULL;
		    } else {
			if (S_ISLNK (hstat.st_mode))
			    inode->mode &= ~S_IFLNK;	/* You *DON'T* want to do this always */
			inode->linkname = NULL;
		    }
		    if (S_ISDIR (hstat.st_mode))
			make_dot_doubledot (entry);
		}
	    }
	  read_extfs_continue:
	    g_free (current_file_name);
	    if (current_link_name != NULL)
		g_free (current_link_name);
	}
    }
    pclose (extfsd);
    close_error_pipe (1, NULL);
#ifdef SCO_FLAVOR
    waitpid (-1, NULL, WNOHANG);
#endif				/* SCO_FLAVOR */
    *pparc = current_archive;
    g_free (buffer);
    return 0;
}

static char *get_path (char *inname, struct archive **archive, int is_dir,
    int do_not_open);

/* Returns path inside argument. Returned char* is inside inname, which is
 * mangled by this operation (so you must not free it's return value).
 */
static char *
get_path_mangle (char *inname, struct archive **archive, int is_dir,
		 int do_not_open)
{
    char *local, *archive_name, *op;
    int result = -1;
    struct archive *parc;
    struct vfs_stamping *parent;
    vfs *v;
    int fstype;

    archive_name = inname;
    vfs_split (inname, &local, &op);

    /*
     * FIXME: we really should pass self pointer. But as we know that
     * extfs_which does not touch vfs *me, it does not matter for now
     */
    fstype = extfs_which (NULL, op);

    if (fstype == -1)
	return NULL;
    if (!local)
	local = "";

    /*
     * All filesystems should have some local archive, at least
     * it can be '/'.
     */
    for (parc = first_archive; parc != NULL; parc = parc->next)
	if (parc->name) {
	    if (!strcmp (parc->name, archive_name)) {
		vfs_stamp (&vfs_extfs_ops, (vfsid) parc);
		goto return_success;
	    }
	}

    result = do_not_open ? -1 : read_archive (fstype, archive_name, &parc);
    if (result == -1)
	ERRNOR (EIO, NULL);

    if (archive_name) {
	v = vfs_type (archive_name);
	if (v == &vfs_local_ops) {
	    parent = NULL;
	} else {
	    parent = g_new (struct vfs_stamping, 1);
	    parent->v = v;
	    parent->next = 0;
	    parent->id = (*v->getid) (v, archive_name, &(parent->parent));
	}
	vfs_add_noncurrent_stamps (&vfs_extfs_ops, (vfsid) parc, parent);
	vfs_rm_parents (parent);
    }
  return_success:
    *archive = parc;
    return local;
}

/* Returns allocated path (without leading slash) inside the archive  */
static char *get_path_from_entry (struct entry *entry)
{
    struct list {
	struct list *next;
	char *name;
    } *head, *p;
    char *localpath;
    size_t len;
    
    for (len = 0, head = 0; entry->dir; entry = entry->dir) {
	p = g_new (struct list, 1);
        p->next = head;
	p->name = entry->name;
	head = p;
	len += strlen (entry->name) + 1;
    }

    if (len == 0)
	return g_strdup ("");
    
    localpath = g_malloc (len);
    *localpath = '\0';
    while (head) {
	strcat (localpath, head->name);
	if (head->next)
	    strcat (localpath, "/");
	p = head;
	head = head->next;
	g_free (p);
    }
    return (localpath);
}


struct loop_protect {
    struct entry *entry;
    struct loop_protect *next;
};
static int errloop;
static int notadir;

static struct entry *
__find_entry (struct entry *dir, char *name,
		    struct loop_protect *list, int make_dirs, int make_file);

static struct entry *
__resolve_symlinks (struct entry *entry, 
			  struct loop_protect *list)
{
    struct entry *pent;
    struct loop_protect *looping;
    
    if (!S_ISLNK (entry->inode->mode))
    	return entry;
    for (looping = list; looping != NULL; looping = looping->next)
    	if (entry == looping->entry) { /* Here we protect us against symlink looping */
    	    errloop = 1;
    	    return NULL;
    	}
    looping = g_new (struct loop_protect, 1);
    looping->entry = entry;
    looping->next = list;
    pent = __find_entry (entry->dir, entry->inode->linkname, looping, 0, 0);
    g_free (looping);
    if (pent == NULL)
    	my_errno = ENOENT;
    return pent;
}

static struct entry *my_resolve_symlinks (struct entry *entry)
{
    struct entry *res;
    
    errloop = 0;
    notadir = 0;
    res = __resolve_symlinks (entry, NULL);
    if (res == NULL) {
    	if (errloop)
    	    my_errno = ELOOP;
    	else if (notadir)
    	    my_errno = ENOTDIR;
    }
    return res;
}

static char *get_archive_name (struct archive *archive)
{
    char *archive_name;

    if (archive->local_name)
	archive_name = archive->local_name;
    else
	archive_name = archive->name;
    
    if (!archive_name || !*archive_name)
	return "no_archive_name";
    else
	return archive_name;
}

/* Don't pass localname as NULL */
static int
extfs_cmd (const char *extfs_cmd, struct archive *archive,
	   struct entry *entry, const char *localname)
{
    char *file;
    char *quoted_file;
    char *archive_name;
    char *mc_extfsdir;
    char *cmd;
    int retval;

    file = get_path_from_entry (entry);
    quoted_file = name_quote (file, 0);
    g_free (file);
    archive_name = name_quote (get_archive_name (archive), 0);

    mc_extfsdir = concat_dir_and_file (mc_home, "extfs" PATH_SEP_STR);
    cmd = g_strconcat (mc_extfsdir, extfs_prefixes[archive->fstype],
		       extfs_cmd, archive_name, " ", quoted_file, " ",
		       localname, NULL);
    g_free (quoted_file);
    g_free (mc_extfsdir);
    g_free (archive_name);

    open_error_pipe ();
    retval = my_system (EXECUTE_AS_SHELL, shell, cmd);
    g_free (cmd);
    close_error_pipe (1, NULL);
    return retval;
}

static void
extfs_run (char *file)
{
    struct archive *archive;
    char *p, *q, *archive_name, *mc_extfsdir;
    char *cmd;

    if ((p = get_path (file, &archive, 0, 0)) == NULL)
	return;
    q = name_quote (p, 0);
    g_free (p);

    archive_name = name_quote (get_archive_name (archive), 0);
    mc_extfsdir = concat_dir_and_file (mc_home, "extfs" PATH_SEP_STR);
    cmd = g_strconcat (mc_extfsdir, extfs_prefixes[archive->fstype],
		       " run ", archive_name, " ", q, NULL);
    g_free (mc_extfsdir);
    g_free (archive_name);
    g_free (q);
    shell_execute (cmd, 0);
    g_free (cmd);
}

static void *
extfs_open (vfs * me, char *file, int flags, int mode)
{
    struct pseudofile *extfs_info;
    struct archive *archive;
    char *q;
    struct entry *entry;
    int local_handle;
    int created = 0;

    if ((q = get_path_mangle (file, &archive, 0, 0)) == NULL)
	return NULL;
    entry = find_entry (archive->root_entry, q, 0, 0);
    if (entry == NULL && (flags & O_CREAT)) {
	/* Create new entry */
	entry = find_entry (archive->root_entry, q, 0, 1);
	created = (entry != NULL);
    }
    if (entry == NULL)
	return NULL;
    if ((entry = my_resolve_symlinks (entry)) == NULL)
	return NULL;

    if (S_ISDIR (entry->inode->mode))
	ERRNOR (EISDIR, NULL);

    if (entry->inode->local_filename == NULL) {
	char *local_filename;

	local_handle = mc_mkstemps (&local_filename, "extfs", NULL);

	if (local_handle == -1)
	    return NULL;
	close (local_handle);

	if (!created && !(flags & O_TRUNC)
	    && extfs_cmd (" copyout ", archive, entry, local_filename)) {
	    unlink (local_filename);
	    free (local_filename);
	    my_errno = EIO;
	    return NULL;
	}
	entry->inode->local_filename = local_filename;
    }

    local_handle = open (entry->inode->local_filename, NO_LINEAR (flags),
			 mode);
    if (local_handle == -1)
	ERRNOR (EIO, NULL);

    extfs_info = g_new (struct pseudofile, 1);
    extfs_info->archive = archive;
    extfs_info->entry = entry;
    extfs_info->has_changed = created;
    extfs_info->local_handle = local_handle;

    /* i.e. we had no open files and now we have one */
    vfs_rmstamp (&vfs_extfs_ops, (vfsid) archive, 1);
    archive->fd_usage++;
    return extfs_info;
}

static int extfs_read (void *data, char *buffer, int count)
{
    struct pseudofile *file = (struct pseudofile *)data;

    return read (file->local_handle, buffer, count);
}

static int extfs_close (void *data)
{
    struct pseudofile *file;
    int    errno_code = 0;
    file = (struct pseudofile *)data;
    
    close (file->local_handle);

    /* Commit the file if it has changed */
    if (file->has_changed) {
	if (extfs_cmd (" copyin ", file->archive, file->entry,
		       file->entry->inode->local_filename))
	    errno_code = EIO;
	{
	    struct stat file_status;
	    if (stat (file->entry->inode->local_filename, &file_status) != 0)
		errno_code = EIO;
	    else
		file->entry->inode->size = file_status.st_size;
	}

	file->entry->inode->mtime = time (NULL);
    }

    file->archive->fd_usage--;
    if (!file->archive->fd_usage) {
        struct vfs_stamping *parent;
        vfs *v;
        
	if (!file->archive->name || !*file->archive->name || (v = vfs_type (file->archive->name)) == &vfs_local_ops) {
	    parent = NULL;
	} else {
	    parent = g_new (struct vfs_stamping, 1);
	    parent->v = v;
	    parent->next = 0;
	    parent->id = (*v->getid) (v, file->archive->name, &(parent->parent));
	}
        vfs_add_noncurrent_stamps (&vfs_extfs_ops, (vfsid) (file->archive), parent);
	vfs_rm_parents (parent);
    }

    g_free (data);
    if (errno_code) ERRNOR (EIO, -1);
    return 0;
}

#define RECORDSIZE 512

static char *get_path (char *inname, struct archive **archive, int is_dir,
    int do_not_open)
{
    char *buf = g_strdup (inname);
    char *res = get_path_mangle( buf, archive, is_dir, do_not_open );
    char *res2 = NULL;
    if (res)
        res2 = g_strdup(res);
    g_free(buf);
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
		if (!S_ISDIR (pent->inode->mode)){
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

    info = g_new (struct entry *, 2);
    info[0] = entry->inode->first_in_subdir;
    info[1] = entry->inode->first_in_subdir;

    return info;
}

static void * s_readdir(void *data)
{
    static union vfs_dirent dir;
    struct entry **info = (struct entry **) data;

    if (!*info)
	return NULL;

    strncpy(dir.dent.d_name, (*info)->name, MC_MAXPATHLEN);
    dir.dent.d_name[MC_MAXPATHLEN] = 0;

    compute_namelen(&dir.dent);
    *info = (*info)->next_in_dir;

    return (void *) &dir;
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
    g_free (data);
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

static int extfs_chmod (vfs *me, char *path, int mode)
{
    return 0;
}

static int extfs_write (void *data, char *buf, int nbyte)
{
    struct pseudofile *file = (struct pseudofile *)data;

    file->has_changed = 1;
    return write (file->local_handle, buf, nbyte);
}

static int extfs_unlink (vfs *me, char *file)
{
    struct archive *archive;
    char *q;
    struct entry *entry;

    if ((q = get_path_mangle (file, &archive, 0, 0)) == NULL)
	return -1;
    entry = find_entry (archive->root_entry, q, 0, 0);
    if (entry == NULL)
    	return -1;
    if ((entry = my_resolve_symlinks (entry)) == NULL)
	return -1;
    if (S_ISDIR (entry->inode->mode)) ERRNOR (EISDIR, -1);

    if (extfs_cmd (" rm ", archive, entry, "")){
        my_errno = EIO;
        return -1;
    }
    remove_entry (entry);

    return 0;
}

static int extfs_mkdir (vfs *me, char *path, mode_t mode)
{
    struct archive *archive;
    char *q;
    struct entry *entry;

    if ((q = get_path_mangle (path, &archive, 0, 0)) == NULL)
	return -1;
    entry = find_entry (archive->root_entry, q, 0, 0);
    if (entry != NULL) ERRNOR (EEXIST, -1);
    entry = find_entry (archive->root_entry, q, 1, 0);
    if (entry == NULL)
	return -1;
    if ((entry = my_resolve_symlinks (entry)) == NULL)
	return -1;
    if (!S_ISDIR (entry->inode->mode)) ERRNOR (ENOTDIR, -1);

    if (extfs_cmd (" mkdir ", archive, entry, "")){
	my_errno = EIO;
	remove_entry (entry);
	return -1;
    }

    return 0;
}

static int extfs_rmdir (vfs *me, char *path)
{
    struct archive *archive;
    char *q;
    struct entry *entry;

    if ((q = get_path_mangle (path, &archive, 0, 0)) == NULL)
	return -1;
    entry = find_entry (archive->root_entry, q, 0, 0);
    if (entry == NULL)
    	return -1;
    if ((entry = my_resolve_symlinks (entry)) == NULL)
	return -1;
    if (!S_ISDIR (entry->inode->mode)) ERRNOR (ENOTDIR, -1);

    if (extfs_cmd (" rmdir ", archive, entry, "")){
        my_errno = EIO;
        return -1;
    }
    remove_entry (entry);

    return 0;
}

static int extfs_chdir (vfs *me, char *path)
{
    struct archive *archive;
    char *q;
    struct entry *entry;

    my_errno = ENOTDIR;
    if ((q = get_path_mangle (path, &archive, 1, 0)) == NULL)
	return -1;
    entry = find_entry (archive->root_entry, q, 0, 0);
    if (!entry)
    	return -1;
    entry = my_resolve_symlinks (entry);
    if ((!entry) || (!S_ISDIR (entry->inode->mode)))
    	return -1;
    entry->inode->archive->current_dir = entry;
    my_errno = 0;
    return 0;
}

static int extfs_lseek (void *data, off_t offset, int whence)
{
    struct pseudofile *file = (struct pseudofile *) data;

    return lseek (file->local_handle, offset, whence);
}

static vfsid extfs_getid (vfs *me, char *path, struct vfs_stamping **parent)
{
    struct archive *archive;
    vfs *v;
    vfsid id;
    struct vfs_stamping *par;
    char *p;

    *parent = NULL;
    if (!(p = get_path (path, &archive, 1, 1)))
	return (vfsid) -1;
    g_free(p);
    if (archive->name){
	v = vfs_type (archive->name);
	id = (*v->getid) (v, archive->name, &par);
	if (id != (vfsid)-1) {
	    *parent = g_new (struct vfs_stamping, 1);
	    (*parent)->v = v;
	    (*parent)->id = id;
	    (*parent)->parent = par;
	    (*parent)->next = NULL;
	}
    }
    return (vfsid) archive;    
}

static int extfs_nothingisopen (vfsid id)
{
    if (((struct archive *)id)->fd_usage <= 0)
    	return 1;
    return 0;
}

static void remove_entry (struct entry *e)
{
    int i = --(e->inode->nlink);
    struct entry *pe, *ent, *prev;

    if (S_ISDIR (e->inode->mode) && e->inode->first_in_subdir != NULL) {
        struct entry *f = e->inode->first_in_subdir;
        e->inode->first_in_subdir = NULL;
	remove_entry (f);
    }
    pe = e->dir;
    if (e == pe->inode->first_in_subdir)
	pe->inode->first_in_subdir = e->next_in_dir;

    prev = NULL;
    for (ent = pe->inode->first_in_subdir; ent && ent->next_in_dir;
	 ent = ent->next_in_dir)
	    if (e == ent->next_in_dir) {
		prev = ent;
		break;
	    }
    if (prev)
	prev->next_in_dir = e->next_in_dir;
    if (e == pe->inode->last_in_subdir)
	pe->inode->last_in_subdir = prev;

    if (i <= 0) {
        if (e->inode->local_filename != NULL) {
            unlink (e->inode->local_filename);
            free (e->inode->local_filename);
        }
        if (e->inode->linkname != NULL)
            g_free (e->inode->linkname);
        g_free (e->inode);
    }

    g_free (e->name);
    g_free (e);
}

static void free_entry (struct entry *e)
{
    int i = --(e->inode->nlink);
    if (S_ISDIR (e->inode->mode) && e->inode->first_in_subdir != NULL) {
        struct entry *f = e->inode->first_in_subdir;
        
        e->inode->first_in_subdir = NULL;
        free_entry (f);
    }
    if (i <= 0) {
        if (e->inode->local_filename != NULL) {
            unlink (e->inode->local_filename);
            free (e->inode->local_filename);
        }
        if (e->inode->linkname != NULL)
            g_free (e->inode->linkname);
        g_free (e->inode);
    }
    if (e->next_in_dir != NULL)
        free_entry (e->next_in_dir);
    g_free (e->name);
    g_free (e);
}

static void extfs_free (vfsid id)
{
    struct archive *parc;
    struct archive *archive = (struct archive *)id;

    free_entry (archive->root_entry);
    if (archive == first_archive) {
        first_archive = archive->next;
    } else {
        for (parc = first_archive; parc != NULL; parc = parc->next)
            if (parc->next == archive)
                break;
        if (parc != NULL)
            parc->next = archive->next;
    }
    free_archive (archive);
}

static char *extfs_getlocalcopy (vfs *me, char *path)
{
    struct pseudofile *fp = 
        (struct pseudofile *) extfs_open (me, path, O_RDONLY, 0);
    char *p;
    
    if (fp == NULL)
        return NULL;
    if (fp->entry->inode->local_filename == NULL) {
        extfs_close ((void *) fp);
        return NULL;
    }
    p = g_strdup (fp->entry->inode->local_filename);
    fp->archive->fd_usage++;
    extfs_close ((void *) fp);
    return p;
}

static int extfs_ungetlocalcopy (vfs *me, char *path, char *local, int has_changed)
{
    struct pseudofile *fp = 
        (struct pseudofile *) extfs_open (me, path, O_RDONLY, 0);
    
    if (fp == NULL)
        return 0;
    if (!strcmp (fp->entry->inode->local_filename, local)) {
        fp->archive->fd_usage--;
        fp->has_changed |= has_changed;
        extfs_close ((void *) fp);
        return 0;
    } else {
        /* Should not happen */
        extfs_close ((void *) fp);
        return mc_def_ungetlocalcopy (me, path, local, has_changed);
    }
}


static int extfs_init (vfs *me)
{
    FILE *cfg;
    char *mc_extfsini;

    mc_extfsini = concat_dir_and_file (mc_home, "extfs" PATH_SEP_STR "extfs.ini");
    cfg = fopen (mc_extfsini, "r");

    /* We may not use vfs_die() message or message_1s or similar,
     * UI is not initialized at this time and message would not
     * appear on screen. */
    if (!cfg) {
	fprintf(stderr, _("Warning: file %s not found\n"), mc_extfsini);
	g_free (mc_extfsini);
	return 0;
    }

    extfs_no = 0;
    while ( extfs_no < MAXEXTFS ) {
        char key[256];
	char *c;

        if (!fgets( key, sizeof (key)-1, cfg ))
	    break;

	/* Handle those with a trailing ':', those flag that the
	 * file system does not require an archive to work
	 */

	if (*key == '[') {
	    fprintf(stderr, "Warning: You need to update your %s file.\n",
		    mc_extfsini);
	    fclose(cfg);
	    g_free (mc_extfsini);
	    return 0;
	}
	if (*key == '#')
	    continue;

	if ((c = strchr (key, '\n'))){
	    *c = 0;
	    c = &key [strlen (key) - 1];
	} else {
	    c = key;
	}
	extfs_need_archive [extfs_no] = !(*c == ':');
	if (*c == ':')
		*c = 0;
	if (!(*key))
	    continue;

	extfs_prefixes [extfs_no] = g_strdup (key);
	extfs_no++;
    }
    fclose(cfg);
    g_free (mc_extfsini);
    return 1;
}

/* Do NOT use me argument in this function */
static int extfs_which (vfs *me, char *path)
{
    int i;

    for (i = 0; i < extfs_no; i++)
        if (!strcmp (path, extfs_prefixes [i]))
            return i;
    return -1;
}

static void extfs_done (vfs *me)
{
    int i;

    for (i = 0; i < extfs_no; i++ )
	g_free (extfs_prefixes [i]);
    extfs_no = 0;
}

static int extfs_setctl (vfs *me, char *path, int ctlop, char *arg)
{
    if (ctlop == MCCTL_EXTFS_RUN) {
        extfs_run (path);
	return 1;
    }
    return 0;
}

vfs vfs_extfs_ops = {
    NULL,	/* This is place of next pointer */
    "extfs",
    F_EXEC,	/* flags */
    NULL,	/* prefix */
    NULL,	/* data */
    0,		/* errno */
    extfs_init,
    extfs_done,
    extfs_fill_names,
    extfs_which,

    extfs_open,
    extfs_close,
    extfs_read,
    extfs_write,

    s_opendir,
    s_readdir,
    s_closedir,
    s_telldir,
    s_seekdir,

    s_stat,
    s_lstat,
    s_fstat,

    extfs_chmod,		/* chmod ... strange, returns success? */
    NULL,
    NULL,

    s_readlink,
    
    NULL,		/* symlink */
    NULL,
    extfs_unlink,

    NULL,
    extfs_chdir,
    s_errno,
    extfs_lseek,
    NULL,
    
    extfs_getid,
    extfs_nothingisopen,
    extfs_free,
    
    extfs_getlocalcopy,
    extfs_ungetlocalcopy,
    
    extfs_mkdir,	/* mkdir */
    extfs_rmdir,
    NULL,
    extfs_setctl

MMAPNULL
};
