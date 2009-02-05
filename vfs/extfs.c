/* Virtual File System: External file system.
   Copyright (C) 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2006, 2007 Free Software Foundation, Inc.
   
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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/* Namespace: init_extfs */

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

#include <mhl/memory.h>
#include <mhl/string.h>

#include "../src/global.h"
#include "../src/tty.h"		/* enable/disable interrupt key */
#include "../src/wtools.h"	/* message() */
#include "../src/main.h"	/* print_vfs_message */
#include "utilvfs.h"
#include "../src/execute.h"	/* For shell_execute */
#include "vfs.h"
#include "vfs-impl.h"
#include "gc.h"		/* vfs_rmstamp */

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
    off_t size;
    time_t mtime;
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

struct archive {
    int fstype;
    char *name;
    char *local_name;
    struct stat local_stat;
    dev_t rdev;
    int fd_usage;
    ino_t inode_counter;
    struct entry *root_entry;
    struct archive *next;
};

static struct entry *extfs_find_entry (struct entry *dir, char *name,
				       int make_dirs, int make_file);
static int extfs_which (struct vfs_class *me, const char *path);
static void extfs_remove_entry (struct entry *e);
static void extfs_free (vfsid id);
static void extfs_free_entry (struct entry *e);

static struct vfs_class vfs_extfs_ops;
static struct archive *first_archive = NULL;
static int my_errno = 0;

#define MAXEXTFS 32
static char *extfs_prefixes [MAXEXTFS];
static char extfs_need_archive [MAXEXTFS];
static int extfs_no = 0;

static void
extfs_fill_names (struct vfs_class *me, fill_names_f func)
{
    struct archive *a = first_archive;
    char *name;

    (void) me;

    while (a) {
	name =
	    g_strconcat (a->name ? a->name : "", "#",
			 extfs_prefixes[a->fstype], (char *) NULL);
	(*func) (name);
	g_free (name);
	a = a->next;
    }
}

static void extfs_make_dots (struct entry *ent)
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
    inode->nlink++;
    entry->next_in_dir = g_new (struct entry, 1);
    entry = entry->next_in_dir;
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

static struct entry *extfs_generate_entry (struct archive *archive, 
    const char *name, struct entry *parentry, mode_t mode)
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
    inode->linkname = NULL;
    inode->last_in_subdir = NULL;
    inode->inode = (archive->inode_counter)++;
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
        extfs_make_dots (entry);
    return entry;
}

#if 0
static void extfs_free_entries (struct entry *entry)
{
    (void) entry;
    return;
}
#endif

static void extfs_free_archive (struct archive *archive)
{
    extfs_free_entry (archive->root_entry);
    if (archive->local_name != NULL) {
        struct stat my;
        
        mc_stat (archive->local_name, &my);
        mc_ungetlocalcopy (archive->name, archive->local_name, 
            archive->local_stat.st_mtime != my.st_mtime);
        g_free(archive->local_name);
    }
    g_free (archive->name);
    g_free (archive);
}

static FILE *
extfs_open_archive (int fstype, const char *name, struct archive **pparc)
{
    static dev_t archive_counter = 0;
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

    mc_extfsdir = mhl_str_dir_plus_file (mc_home, "extfs" PATH_SEP_STR);
    cmd =
	g_strconcat (mc_extfsdir, extfs_prefixes[fstype], " list ",
		     local_name ? local_name : tmp, (char *) NULL);
    g_free (tmp);
    g_free (mc_extfsdir);
    open_error_pipe ();
    result = popen (cmd, "r");
    g_free (cmd);
    if (result == NULL) {
	close_error_pipe (D_ERROR, NULL);
	if (local_name) {
	    mc_ungetlocalcopy (name, local_name, 0);
	    g_free(local_name);
	}
	return NULL;
    } 
#ifdef ___QNXNTO__    
    setvbuf (result, NULL, _IONBF, 0);
#endif    

    current_archive = g_new (struct archive, 1);
    current_archive->fstype = fstype;
    current_archive->name = name ? g_strdup (name) : NULL;
    current_archive->local_name = local_name;

    if (local_name != NULL)
	mc_stat (local_name, &current_archive->local_stat);
    current_archive->inode_counter = 0;
    current_archive->fd_usage = 0;
    current_archive->rdev = archive_counter++;
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
    root_entry = extfs_generate_entry (current_archive, "/", NULL, mode);
    root_entry->inode->uid = mystat.st_uid;
    root_entry->inode->gid = mystat.st_gid;
    root_entry->inode->atime = mystat.st_atime;
    root_entry->inode->ctime = mystat.st_ctime;
    root_entry->inode->mtime = mystat.st_mtime;
    current_archive->root_entry = root_entry;

    *pparc = current_archive;

    return result;
}

/*
 * Main loop for reading an archive.
 * Return 0 on success, -1 on error.
 */
static int
extfs_read_archive (int fstype, const char *name, struct archive **pparc)
{
    FILE *extfsd;
    char *buffer;
    struct archive *current_archive;
    char *current_file_name, *current_link_name;

    if ((extfsd =
	 extfs_open_archive (fstype, name, &current_archive)) == NULL) {
	message (D_ERROR, MSG_ERROR, _("Cannot open %s archive\n%s"),
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
		pent =
		    extfs_find_entry (current_archive->root_entry, q, 1,
				      0);
		if (pent == NULL) {
		    /* FIXME: Should clean everything one day */
		    g_free (buffer);
		    pclose (extfsd);
		    close_error_pipe (D_ERROR, _("Inconsistent extfs archive"));
		    return -1;
		}
		entry = g_new (struct entry, 1);
		entry->name = g_strdup (p);
		entry->next_in_dir = NULL;
		entry->dir = pent;
		if (pent->inode->last_in_subdir) {
		    pent->inode->last_in_subdir->next_in_dir = entry;
		    pent->inode->last_in_subdir = entry;
		}
		if (!S_ISLNK (hstat.st_mode) && current_link_name != NULL) {
		    pent =
			extfs_find_entry (current_archive->root_entry,
					  current_link_name, 0, 0);
		    if (pent == NULL) {
			/* FIXME: Should clean everything one day */
			g_free (buffer);
			pclose (extfsd);
			close_error_pipe (D_ERROR,
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
		    inode->inode = (current_archive->inode_counter)++;
		    inode->nlink = 1;
		    inode->dev = current_archive->rdev;
		    inode->archive = current_archive;
		    inode->mode = hstat.st_mode;
#ifdef HAVE_STRUCT_STAT_ST_RDEV
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
		    inode->first_in_subdir = NULL;
		    inode->last_in_subdir = NULL;
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
			extfs_make_dots (entry);
		}
	    }
	  read_extfs_continue:
	    g_free (current_file_name);
	    g_free (current_link_name);
	}
    }
    g_free (buffer);

    /* Check if extfs 'list' returned 0 */
    if (pclose (extfsd) != 0) {
	extfs_free (current_archive);
	close_error_pipe (D_ERROR, _("Inconsistent extfs archive"));
	return -1;
    }

    close_error_pipe (D_ERROR, NULL);
    *pparc = current_archive;
    return 0;
}

/*
 * Dissect the path and create corresponding superblock.  Note that inname
 * can be changed and the result may point inside the original string.
 */
static char *
extfs_get_path_mangle (struct vfs_class *me, char *inname, struct archive **archive,
		       int do_not_open)
{
    char *local, *op;
    const char *archive_name;
    int result = -1;
    struct archive *parc;
    int fstype;

    archive_name = inname;
    vfs_split (inname, &local, &op);

    fstype = extfs_which (me, op);

    if (fstype == -1)
	return NULL;
    if (!local)
	local = inname + strlen (inname);

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

    result =
	do_not_open ? -1 : extfs_read_archive (fstype, archive_name,
					       &parc);
    if (result == -1)
	ERRNOR (EIO, NULL);

  return_success:
    *archive = parc;
    return local;
}


/*
 * Dissect the path and create corresponding superblock.
 * The result should be freed.
 */
static char *
extfs_get_path (struct vfs_class *me, const char *inname, struct archive **archive,
		int do_not_open)
{
    char *buf = g_strdup (inname);
    char *res = extfs_get_path_mangle (me, buf, archive, do_not_open);
    char *res2 = NULL;
    if (res)
	res2 = g_strdup (res);
    g_free (buf);
    return res2;
}


/* Return allocated path (without leading slash) inside the archive  */
static char *extfs_get_path_from_entry (struct entry *entry)
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
extfs_find_entry_int (struct entry *dir, char *name,
		    struct loop_protect *list, int make_dirs, int make_file);

static struct entry *
extfs_resolve_symlinks_int (struct entry *entry, 
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
    pent = extfs_find_entry_int (entry->dir, entry->inode->linkname, looping, 0, 0);
    g_free (looping);
    if (pent == NULL)
    	my_errno = ENOENT;
    return pent;
}

static struct entry *extfs_resolve_symlinks (struct entry *entry)
{
    struct entry *res;
    
    errloop = 0;
    notadir = 0;
    res = extfs_resolve_symlinks_int (entry, NULL);
    if (res == NULL) {
    	if (errloop)
    	    my_errno = ELOOP;
    	else if (notadir)
    	    my_errno = ENOTDIR;
    }
    return res;
}

static const char *
extfs_get_archive_name (struct archive *archive)
{
    const char *archive_name;

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
    char *quoted_localname;
    char *archive_name;
    char *mc_extfsdir;
    char *cmd;
    int retval;

    file = extfs_get_path_from_entry (entry);
    quoted_file = name_quote (file, 0);
    g_free (file);
    archive_name = name_quote (extfs_get_archive_name (archive), 0);
    quoted_localname = name_quote (localname, 0);

    mc_extfsdir = mhl_str_dir_plus_file (mc_home, "extfs" PATH_SEP_STR);
    cmd = g_strconcat (mc_extfsdir, extfs_prefixes[archive->fstype],
		       extfs_cmd, archive_name, " ", quoted_file, " ",
		       quoted_localname, (char *) NULL);
    g_free (quoted_file);
    g_free (quoted_localname);
    g_free (mc_extfsdir);
    g_free (archive_name);

    open_error_pipe ();
    retval = my_system (EXECUTE_AS_SHELL, shell, cmd);
    g_free (cmd);
    close_error_pipe (D_ERROR, NULL);
    return retval;
}

static void
extfs_run (struct vfs_class *me, const char *file)
{
    struct archive *archive = NULL;
    char *p, *q, *archive_name, *mc_extfsdir;
    char *cmd;

    if ((p = extfs_get_path (me, file, &archive, 0)) == NULL)
	return;
    q = name_quote (p, 0);
    g_free (p);

    archive_name = name_quote (extfs_get_archive_name (archive), 0);
    mc_extfsdir = mhl_str_dir_plus_file (mc_home, "extfs" PATH_SEP_STR);
    cmd = g_strconcat (mc_extfsdir, extfs_prefixes[archive->fstype],
		       " run ", archive_name, " ", q, (char *) NULL);
    g_free (mc_extfsdir);
    g_free (archive_name);
    g_free (q);
    shell_execute (cmd, 0);
    g_free (cmd);
}

static void *
extfs_open (struct vfs_class *me, const char *file, int flags, int mode)
{
    struct pseudofile *extfs_info;
    struct archive *archive = NULL;
    char *q;
    struct entry *entry;
    int local_handle;
    int created = 0;

    if ((q = extfs_get_path (me, file, &archive, 0)) == NULL)
	return NULL;
    entry = extfs_find_entry (archive->root_entry, q, 0, 0);
    if (entry == NULL && (flags & O_CREAT)) {
	/* Create new entry */
	entry = extfs_find_entry (archive->root_entry, q, 0, 1);
	created = (entry != NULL);
    }

    g_free (q);
    if (entry == NULL)
	return NULL;
    if ((entry = extfs_resolve_symlinks (entry)) == NULL)
	return NULL;

    if (S_ISDIR (entry->inode->mode))
	ERRNOR (EISDIR, NULL);

    if (entry->inode->local_filename == NULL) {
	char *local_filename;

	local_handle = vfs_mkstemps (&local_filename, "extfs", entry->name);

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

    local_handle =
	open (entry->inode->local_filename, NO_LINEAR (flags), mode);
    if (local_handle == -1)
	ERRNOR (EIO, NULL);

    extfs_info = g_new (struct pseudofile, 1);
    extfs_info->archive = archive;
    extfs_info->entry = entry;
    extfs_info->has_changed = created;
    extfs_info->local_handle = local_handle;

    /* i.e. we had no open files and now we have one */
    vfs_rmstamp (&vfs_extfs_ops, (vfsid) archive);
    archive->fd_usage++;
    return extfs_info;
}

static ssize_t extfs_read (void *data, char *buffer, int count)
{
    struct pseudofile *file = (struct pseudofile *)data;

    return read (file->local_handle, buffer, count);
}

static int
extfs_close (void *data)
{
    struct pseudofile *file;
    int errno_code = 0;
    file = (struct pseudofile *) data;

    close (file->local_handle);

    /* Commit the file if it has changed */
    if (file->has_changed) {
	if (extfs_cmd
	    (" copyin ", file->archive, file->entry,
	     file->entry->inode->local_filename))
	    errno_code = EIO;
	{
	    struct stat file_status;
	    if (stat (file->entry->inode->local_filename, &file_status) !=
		0)
		errno_code = EIO;
	    else
		file->entry->inode->size = file_status.st_size;
	}

	file->entry->inode->mtime = time (NULL);
    }

    file->archive->fd_usage--;
    if (!file->archive->fd_usage)
	vfs_stamp_create (&vfs_extfs_ops, file->archive);

    g_free (data);
    if (errno_code)
	ERRNOR (EIO, -1);
    return 0;
}

#define RECORDSIZE 512

static struct entry*
extfs_find_entry_int (struct entry *dir, char *name, 
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
		if ((pent = extfs_resolve_symlinks_int (pent, list))==NULL){
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
		    pent = extfs_generate_entry (dir->inode->archive, p, pdir, S_IFDIR | 0777);
		}
		if (pent == NULL && make_file) { 
		    pent = extfs_generate_entry (dir->inode->archive, p, pdir, S_IFREG | 0666);
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

static struct entry *extfs_find_entry (struct entry *dir, char *name, int make_dirs, int make_file)
{
    struct entry *res;
    
    errloop = 0;
    notadir = 0;
    res = extfs_find_entry_int (dir, name, NULL, make_dirs, make_file);
    if (res == NULL) {
    	if (errloop)
    	    my_errno = ELOOP;
    	else if (notadir)
    	    my_errno = ENOTDIR;
    }
    return res;
}


static int extfs_errno (struct vfs_class *me)
{
    (void) me;

    return my_errno;
}

static void * extfs_opendir (struct vfs_class *me, const char *dirname)
{
    struct archive *archive = NULL;
    char *q;
    struct entry *entry;
    struct entry **info;

    if ((q = extfs_get_path (me, dirname, &archive, 0)) == NULL)
	return NULL;
    entry = extfs_find_entry (archive->root_entry, q, 0, 0);
    g_free (q);
    if (entry == NULL)
    	return NULL;
    if ((entry = extfs_resolve_symlinks (entry)) == NULL)
	return NULL;
    if (!S_ISDIR (entry->inode->mode)) ERRNOR (ENOTDIR, NULL);

    info = g_new (struct entry *, 2);
    info[0] = entry->inode->first_in_subdir;
    info[1] = entry->inode->first_in_subdir;

    return info;
}

static void * extfs_readdir(void *data)
{
    static union vfs_dirent dir;
    struct entry **info = (struct entry **) data;

    if (!*info)
	return NULL;

    g_strlcpy(dir.dent.d_name, (*info)->name, MC_MAXPATHLEN);

    compute_namelen(&dir.dent);
    *info = (*info)->next_in_dir;

    return (void *) &dir;
}

static int extfs_closedir (void *data)
{
    g_free (data);
    return 0;
}

static void extfs_stat_move (struct stat *buf, const struct inode *inode)
{
    buf->st_dev = inode->dev;
    buf->st_ino = inode->inode;
    buf->st_mode = inode->mode;
    buf->st_nlink = inode->nlink;
    buf->st_uid = inode->uid;
    buf->st_gid = inode->gid;
#ifdef HAVE_STRUCT_STAT_ST_RDEV
    buf->st_rdev = inode->rdev;
#endif
    buf->st_size = inode->size;
#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
    buf->st_blksize = RECORDSIZE;
#endif
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
    buf->st_blocks = (inode->size + RECORDSIZE - 1) / RECORDSIZE;
#endif
    buf->st_atime = inode->atime;
    buf->st_mtime = inode->mtime;
    buf->st_ctime = inode->ctime;
}

static int
extfs_internal_stat (struct vfs_class *me, const char *path, struct stat *buf,
		     int resolve)
{
    struct archive *archive;
    char *q;
    struct entry *entry;
    char *path2 = g_strdup (path);
    int result = -1;

    if ((q = extfs_get_path_mangle (me, path2, &archive, 0)) == NULL)
        goto cleanup;
    entry = extfs_find_entry (archive->root_entry, q, 0, 0);
    if (entry == NULL)
        goto cleanup;
    if (resolve && (entry = extfs_resolve_symlinks (entry)) == NULL)
        goto cleanup;
    extfs_stat_move (buf, entry->inode);
    result = 0;
cleanup:
    g_free (path2);
    return result;
}

static int extfs_stat (struct vfs_class *me, const char *path, struct stat *buf)
{
    return extfs_internal_stat (me, path, buf, 1);
}

static int extfs_lstat (struct vfs_class *me, const char *path, struct stat *buf)
{
    return extfs_internal_stat (me, path, buf, 0);
}

static int extfs_fstat (void *data, struct stat *buf)
{
    struct pseudofile *file = (struct pseudofile *)data;
    
    extfs_stat_move (buf, file->entry->inode);
    return 0;
}

static int
extfs_readlink (struct vfs_class *me, const char *path, char *buf, size_t size)
{
    struct archive *archive;
    char *q;
    size_t len;
    struct entry *entry;
    char *mpath = g_strdup (path);
    int result = -1;

    if ((q = extfs_get_path_mangle (me, mpath, &archive, 0)) == NULL)
	goto cleanup;
    entry = extfs_find_entry (archive->root_entry, q, 0, 0);
    if (entry == NULL)
        goto cleanup;
    if (!S_ISLNK (entry->inode->mode)) {
    	me->verrno = EINVAL;
    	goto cleanup;
    }
    len = strlen (entry->inode->linkname);
    if (size < len)
       len = size;
    /* readlink() does not append a NUL character to buf */
    memcpy (buf, entry->inode->linkname, result = len);
cleanup:
    g_free (mpath);
    return result;
}

static int extfs_chmod (struct vfs_class *me, const char *path, int mode)
{
    (void) me;
    (void) path;
    (void) mode;
    return 0;
}

static ssize_t extfs_write (void *data, const char *buf, int nbyte)
{
    struct pseudofile *file = (struct pseudofile *)data;

    file->has_changed = 1;
    return write (file->local_handle, buf, nbyte);
}

static int extfs_unlink (struct vfs_class *me, const char *file)
{
    struct archive *archive;
    char *q, *mpath = g_strdup (file);
    struct entry *entry;
    int result = -1;

    if ((q = extfs_get_path_mangle (me, mpath, &archive, 0)) == NULL)
	goto cleanup;
    entry = extfs_find_entry (archive->root_entry, q, 0, 0);
    if (entry == NULL)
        goto cleanup;
    if ((entry = extfs_resolve_symlinks (entry)) == NULL)
        goto cleanup;
    if (S_ISDIR (entry->inode->mode)) {
    	me->verrno = EISDIR;
    	goto cleanup;
    }
    if (extfs_cmd (" rm ", archive, entry, "")){
        my_errno = EIO;
        goto cleanup;
    }
    extfs_remove_entry (entry);
    result = 0;
cleanup:
    g_free (mpath);
    return result;
}

static int extfs_mkdir (struct vfs_class *me, const char *path, mode_t mode)
{
    struct archive *archive;
    char *q, *mpath = g_strdup(path);
    struct entry *entry;
    int result = -1;

    (void) mode;

    if ((q = extfs_get_path_mangle (me, mpath, &archive, 0)) == NULL)
	goto cleanup;
    entry = extfs_find_entry (archive->root_entry, q, 0, 0);
    if (entry != NULL) {
    	me->verrno = EEXIST;
    	goto cleanup;
    }
    entry = extfs_find_entry (archive->root_entry, q, 1, 0);
    if (entry == NULL)
        goto cleanup;
    if ((entry = extfs_resolve_symlinks (entry)) == NULL)
        goto cleanup;
    if (!S_ISDIR (entry->inode->mode)) {
    	me->verrno = ENOTDIR;
    	goto cleanup;
    }

    if (extfs_cmd (" mkdir ", archive, entry, "")){
	my_errno = EIO;
	extfs_remove_entry (entry);
	goto cleanup;
    }
    result = 0;
cleanup:
    g_free (mpath);
    return result;
}

static int extfs_rmdir (struct vfs_class *me, const char *path)
{
    struct archive *archive;
    char *q, *mpath = g_strdup(path);
    struct entry *entry;
    int result = -1;

    if ((q = extfs_get_path_mangle (me, mpath, &archive, 0)) == NULL)
	goto cleanup;
    entry = extfs_find_entry (archive->root_entry, q, 0, 0);
    if (entry == NULL)
    	goto cleanup;
    if ((entry = extfs_resolve_symlinks (entry)) == NULL)
	goto cleanup;
    if (!S_ISDIR (entry->inode->mode)) {
    	me->verrno = ENOTDIR;
    	goto cleanup;
    }

    if (extfs_cmd (" rmdir ", archive, entry, "")){
        my_errno = EIO;
        goto cleanup;
    }
    extfs_remove_entry (entry);
    result = 0;
cleanup:
    g_free (mpath);
    return result;
}

static int
extfs_chdir (struct vfs_class *me, const char *path)
{
    struct archive *archive = NULL;
    char *q;
    struct entry *entry;

    my_errno = ENOTDIR;
    if ((q = extfs_get_path (me, path, &archive, 0)) == NULL)
	return -1;
    entry = extfs_find_entry (archive->root_entry, q, 0, 0);
    g_free (q);
    if (!entry)
	return -1;
    entry = extfs_resolve_symlinks (entry);
    if ((!entry) || (!S_ISDIR (entry->inode->mode)))
	return -1;
    my_errno = 0;
    return 0;
}

static off_t extfs_lseek (void *data, off_t offset, int whence)
{
    struct pseudofile *file = (struct pseudofile *) data;

    return lseek (file->local_handle, offset, whence);
}

static vfsid
extfs_getid (struct vfs_class *me, const char *path)
{
    struct archive *archive = NULL;
    char *p;

    if (!(p = extfs_get_path (me, path, &archive, 1)))
	return NULL;
    g_free (p);
    return (vfsid) archive;
}

static int extfs_nothingisopen (vfsid id)
{
    if (((struct archive *)id)->fd_usage <= 0)
    	return 1;
    return 0;
}

static void extfs_remove_entry (struct entry *e)
{
    int i = --(e->inode->nlink);
    struct entry *pe, *ent, *prev;

    if (S_ISDIR (e->inode->mode) && e->inode->first_in_subdir != NULL) {
        struct entry *f = e->inode->first_in_subdir;
        e->inode->first_in_subdir = NULL;
	extfs_remove_entry (f);
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
        g_free (e->inode->linkname);
        g_free (e->inode);
    }

    g_free (e->name);
    g_free (e);
}

static void extfs_free_entry (struct entry *e)
{
    int i = --(e->inode->nlink);
    if (S_ISDIR (e->inode->mode) && e->inode->first_in_subdir != NULL) {
        struct entry *f = e->inode->first_in_subdir;
        
        e->inode->first_in_subdir = NULL;
        extfs_free_entry (f);
    }
    if (i <= 0) {
        if (e->inode->local_filename != NULL) {
            unlink (e->inode->local_filename);
            free (e->inode->local_filename);
        }
        g_free (e->inode->linkname);
        g_free (e->inode);
    }
    if (e->next_in_dir != NULL)
        extfs_free_entry (e->next_in_dir);
    g_free (e->name);
    g_free (e);
}

static void extfs_free (vfsid id)
{
    struct archive *parc;
    struct archive *archive = (struct archive *)id;

    if (archive == first_archive) {
        first_archive = archive->next;
    } else {
        for (parc = first_archive; parc != NULL; parc = parc->next)
            if (parc->next == archive) {
                parc->next = archive->next;
                break;
            }
    }
    extfs_free_archive (archive);
}

static char *
extfs_getlocalcopy (struct vfs_class *me, const char *path)
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

static int
extfs_ungetlocalcopy (struct vfs_class *me, const char *path,
		      const char *local, int has_changed)
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
	return 0;
    }
}


static int extfs_init (struct vfs_class *me)
{
    FILE *cfg;
    char *mc_extfsini;
    char key[256];

    (void) me;

    mc_extfsini = mhl_str_dir_plus_file (mc_home, "extfs" PATH_SEP_STR "extfs.ini");
    cfg = fopen (mc_extfsini, "r");

    /* We may not use vfs_die() message or message or similar,
     * UI is not initialized at this time and message would not
     * appear on screen. */
    if (!cfg) {
	fprintf (stderr, _("Warning: file %s not found\n"), mc_extfsini);
	g_free (mc_extfsini);
	return 0;
    }

    extfs_no = 0;
    while (extfs_no < MAXEXTFS && fgets (key, sizeof (key), cfg)) {
	char *c;

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
	if (*key == '#' || *key == '\n')
	    continue;

	if ((c = strchr (key, '\n'))){
	    *c-- = 0;
	} else {	/* Last line without newline or strlen (key) > 255 */
	    c = &key [strlen (key) - 1];
	}
	extfs_need_archive [extfs_no] = !(*c == ':');
	if (*c == ':')
	    *c = 0;
	if (!(*key))
	    continue;

	extfs_prefixes [extfs_no++] = g_strdup (key);
    }
    fclose(cfg);
    g_free (mc_extfsini);
    return 1;
}

static int extfs_which (struct vfs_class *me, const char *path)
{
    int i;

    (void) me;

    for (i = 0; i < extfs_no; i++)
        if (!strcmp (path, extfs_prefixes [i]))
            return i;
    return -1;
}

static void extfs_done (struct vfs_class *me)
{
    int i;
    struct archive *ar;

    (void) me;

    for (ar = first_archive; ar != NULL;) {
	extfs_free ((vfsid) ar);
	ar = first_archive;
    }

    for (i = 0; i < extfs_no; i++ )
	g_free (extfs_prefixes [i]);
    extfs_no = 0;
}

static int
extfs_setctl (struct vfs_class *me, const char *path, int ctlop, void *arg)
{
    (void) arg;

    if (ctlop == VFS_SETCTL_RUN) {
	extfs_run (me, path);
	return 1;
    }
    return 0;
}

void
init_extfs (void)
{
    vfs_extfs_ops.name = "extfs";
    vfs_extfs_ops.init = extfs_init;
    vfs_extfs_ops.done = extfs_done;
    vfs_extfs_ops.fill_names = extfs_fill_names;
    vfs_extfs_ops.which = extfs_which;
    vfs_extfs_ops.open = extfs_open;
    vfs_extfs_ops.close = extfs_close;
    vfs_extfs_ops.read = extfs_read;
    vfs_extfs_ops.write = extfs_write;
    vfs_extfs_ops.opendir = extfs_opendir;
    vfs_extfs_ops.readdir = extfs_readdir;
    vfs_extfs_ops.closedir = extfs_closedir;
    vfs_extfs_ops.stat = extfs_stat;
    vfs_extfs_ops.lstat = extfs_lstat;
    vfs_extfs_ops.fstat = extfs_fstat;
    vfs_extfs_ops.chmod = extfs_chmod;
    vfs_extfs_ops.readlink = extfs_readlink;
    vfs_extfs_ops.unlink = extfs_unlink;
    vfs_extfs_ops.chdir = extfs_chdir;
    vfs_extfs_ops.ferrno = extfs_errno;
    vfs_extfs_ops.lseek = extfs_lseek;
    vfs_extfs_ops.getid = extfs_getid;
    vfs_extfs_ops.nothingisopen = extfs_nothingisopen;
    vfs_extfs_ops.free = extfs_free;
    vfs_extfs_ops.getlocalcopy = extfs_getlocalcopy;
    vfs_extfs_ops.ungetlocalcopy = extfs_ungetlocalcopy;
    vfs_extfs_ops.mkdir = extfs_mkdir;
    vfs_extfs_ops.rmdir = extfs_rmdir;
    vfs_extfs_ops.setctl = extfs_setctl;
    vfs_register_class (&vfs_extfs_ops);
}
