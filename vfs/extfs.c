/* Virtual File System: External file system.
   Copyright (C) 1995 The Free Software Foundation
   
   Written by: 1995 Jakub Jelinek
   Rewritten by: 1998 Pavel Machek

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* Namespace: exports only vfs_extfs_ops */

#include <config.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include <errno.h>
#ifdef SCO_FLAVOR
#include <sys/timeb.h>	/* alex: for struct timeb definition */
#endif /* SCO_FLAVOR */
#include <time.h>
#include "../src/fs.h"
#include "../src/util.h"
#include "../src/dialog.h"
#include "../src/mem.h"
#include "../src/mad.h"
#include "../src/main.h"	/* For shell_execute */
#include "vfs.h"
#include "extfs.h"

#define ERRNOR(x,y) do { my_errno = x; return y; } while(0)

static struct entry *
find_entry (struct entry *dir, char *name, int make_dirs, int make_file);
static int extfs_which (vfs *me, char *path);

static struct archive *first_archive = NULL;
static int my_errno = 0;
static struct stat hstat;		/* Stat struct corresponding */

#define MAXEXTFS 32
static char *extfs_prefixes [MAXEXTFS];
static char extfs_need_archive [MAXEXTFS];
static int extfs_no = 0;

static void extfs_fill_names (vfs *me, void (*func)(char *))
{
    struct archive *a = first_archive;
    char *name;
    
    while (a){
	name = copy_strings (extfs_prefixes [a->fstype], "#", 
			    (a->name ? a->name : ""), "/",
			     a->current_dir->name, 0);
	(*func)(name);
	free (name);
	a = a->next;
    }
}

static void make_dot_doubledot (struct entry *ent)
{
    struct entry *entry = (struct entry *)
	xmalloc (sizeof (struct entry), "Extfs: entry");
    struct entry *parentry = ent->dir;
    struct inode *inode = ent->inode, *parent;
    
    parent = (parentry != NULL) ? parentry->inode : NULL;
    entry->name = strdup (".");
    entry->has_changed = 0;
    entry->inode = inode;
    entry->dir = ent;
    inode->local_filename = NULL;
    inode->first_in_subdir = entry;
    inode->last_in_subdir = entry;
    inode->nlink++;
    entry->next_in_dir = (struct entry *)
	xmalloc (sizeof (struct entry), "Extfs: entry");
    entry=entry->next_in_dir;
    entry->name = strdup ("..");
    entry->has_changed = 0;
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
    entry = (struct entry *)
	xmalloc (sizeof (struct entry), "Extfs: entry");
    
    entry->name = strdup (name);
    entry->has_changed = 0;
    entry->next_in_dir = NULL;
    entry->dir = parentry;
    if (parent != NULL) {
    	parent->last_in_subdir->next_in_dir = entry;
    	parent->last_in_subdir = entry;
    }
    inode = (struct inode *)
	xmalloc (sizeof (struct inode), "Extfs: inode");
    entry->inode = inode;
    inode->has_changed = 0;
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
	free (archive->name);
    free (archive);
}

static FILE *open_archive (int fstype, char *name, struct archive **pparc)
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
    int uses_archive = extfs_need_archive [fstype];
    
    if (uses_archive){
	if (mc_stat (name, &mystat) == -1)
	    return NULL;
	if (!vfs_file_is_local (name)) {
	    local_name = mc_getlocalcopy (name);
	    if (local_name == NULL)
		return NULL;
	}
        tmp = name_quote (name, 0);
    }

#if 0
    /* Sorry, what is this good for? */
    if (uses_archive == EFS_NEED_ARG){
        tmp = name_quote (name, 0);
    }
#endif
    
    mc_extfsdir = concat_dir_and_file (mc_home, "extfs/");
    cmd = copy_strings (mc_extfsdir, extfs_prefixes [fstype], 
                        " list ", local_name ? local_name : tmp, 0);
    if (tmp)
	free (tmp);
    result = popen (cmd, "r");
    free (cmd);
    free (mc_extfsdir);
    if (result == NULL) {
        if (local_name != NULL && uses_archive)
            mc_ungetlocalcopy (name, local_name, 0);
    	return NULL;
    }
    
    current_archive = (struct archive *)
                      xmalloc (sizeof (struct archive), "Extfs archive");
    current_archive->fstype = fstype;
    current_archive->name = name ? strdup (name): name;
    current_archive->local_name = local_name;
    
    if (local_name != NULL)
        mc_stat (local_name, &current_archive->local_stat);
    current_archive->__inode_counter = 0;
    current_archive->fd_usage = 0;
    current_archive->extfsstat = mystat;
    current_archive->rdev = __extfs_no++;
    current_archive->next = first_archive;
    first_archive = current_archive;
    mode = current_archive->extfsstat.st_mode & 07777;
    if (mode & 0400)
    	mode |= 0100;
    if (mode & 0040)
    	mode |= 0010;
    if (mode & 0004)
    	mode |= 0001;
    mode |= S_IFDIR;
    root_entry = generate_entry (current_archive, "/", NULL, mode);
    root_entry->inode->uid = current_archive->extfsstat.st_uid;
    root_entry->inode->gid = current_archive->extfsstat.st_gid;
    root_entry->inode->atime = current_archive->extfsstat.st_atime;
    root_entry->inode->ctime = current_archive->extfsstat.st_ctime;
    root_entry->inode->mtime = current_archive->extfsstat.st_mtime;
    current_archive->root_entry = root_entry;
    current_archive->current_dir = root_entry;
    
    *pparc = current_archive;

    return result;
}

/*
 * Main loop for reading an archive.
 * Returns 0 on success, -1 on error.
 */
static int read_archive (int fstype, char *name, struct archive **pparc)
{
    FILE *extfsd;
    char *buffer;
    struct archive *current_archive;
    char *current_file_name, *current_link_name;


    if ((extfsd = open_archive (fstype, name, &current_archive)) == NULL) {
        message_3s (1, MSG_ERROR, _("Couldn't open %s archive\n%s"), 
            extfs_prefixes [fstype], name);
	return -1;
    }

    buffer = xmalloc (4096, "Extfs: buffer");
    while (fgets (buffer, 4096, extfsd) != NULL) {
        current_link_name = NULL;
        if (vfs_parse_ls_lga (buffer, &hstat, &current_file_name, &current_link_name)) {
            struct entry *entry, *pent;
            struct inode *inode;
            char *p, *q, *cfn = current_file_name;
            
            if (*cfn) {
                if (*cfn == '/')
                    cfn++;
                p = strchr (cfn, 0);
                if (p != NULL && p != cfn && *(p - 1) == '/')
                    *(p - 1) = 0;
                p = strrchr (cfn, '/');
                if (p == NULL) {
                    p = cfn;
                    q = strchr (cfn, 0);
                } else {
                    *(p++) = 0;
                    q = cfn;
                }
                if (S_ISDIR (hstat.st_mode) && 
                    (!strcmp (p, ".") || !strcmp (p, "..")))
                    goto read_extfs_continue;
                pent = find_entry (current_archive->root_entry, q, 1, 0) ;
                if (pent == NULL) {
                    message_1s (1, MSG_ERROR, _("Inconsistent extfs archive"));
                    /* FIXME: Should clean everything one day */
                    free (buffer);
		    pclose (extfsd);
                    return -1;
                }
		entry = (struct entry *) xmalloc (sizeof (struct entry), "Extfs: entry");
		entry->name = strdup (p);
		entry->has_changed = 0;
		entry->next_in_dir = NULL;
		entry->dir = pent;
		if (pent != NULL) {
		    if (pent->inode->last_in_subdir){
			pent->inode->last_in_subdir->next_in_dir = entry;
			pent->inode->last_in_subdir = entry;
		    }
		}
		if (!S_ISLNK (hstat.st_mode) && current_link_name != NULL) {
	            pent = find_entry (current_archive->root_entry, current_link_name, 0, 0);
	            if (pent == NULL) {
	                message_1s (1, MSG_ERROR, _("Inconsistent extfs archive"));
                    /* FIXME: Should clean everything one day */
			free (buffer);
			pclose (extfsd);
	                return -1;
	            } else {
			entry->inode = pent->inode;
			pent->inode->nlink++;
	            }
	        } else {
		    inode = (struct inode *) xmalloc (sizeof (struct inode), "Extfs: inode");
		    entry->inode = inode;
		    inode->local_filename = NULL;
		    inode->has_changed = 0;
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
		    if (current_link_name != NULL && S_ISLNK (hstat.st_mode)) {
	    		inode->linkname = current_link_name;
	    		current_link_name = NULL;
		    } else {
		        if (S_ISLNK( hstat.st_mode))
			    inode->mode &= ~S_IFLNK; /* You *DON'T* want to do this always */
	    		inode->linkname = NULL;
		    }
		    if (S_ISDIR (hstat.st_mode))
	    		make_dot_doubledot (entry);
	    	}
            }
 read_extfs_continue:
            free (current_file_name);
            if (current_link_name != NULL)
                free (current_link_name);
        }
    }
    pclose (extfsd);
#ifdef SCO_FLAVOR
	waitpid(-1,NULL,WNOHANG);
#endif /* SCO_FLAVOR */
    *pparc = current_archive;
    free (buffer);
    return 0;
}

static char *get_path (char *inname, struct archive **archive, int is_dir,
    int do_not_open);

/* Returns path inside argument. Returned char* is inside inname, which is mangled	
 * by this operation (so you must not free it's return value)
 */
static char *get_path_mangle (char *inname, struct archive **archive, int is_dir,
    int do_not_open)
{
    char *local, *archive_name, *op;
    int result = -1;
    struct archive *parc;
    struct vfs_stamping *parent;
    vfs *v;
    int fstype;
    
    archive_name = inname;
    vfs_split( inname, &local, &op );
    fstype = extfs_which( NULL, op ); /* FIXME: we really should pass
					 self pointer. But as we know that extfs_which does not touch vfs
					 *me, it does not matter for now */
    if (!local)
        local = "";
    if (fstype == -1)
        return NULL;

    /* All filesystems should have some local archive, at least
     * it can be '/'.
     *
     * Actually, we should implement an alias mechanism that would
     * translate: "a:" to "dos:a.
     *
     */
    for (parc = first_archive; parc != NULL; parc = parc->next)
        if (parc->name) {
	    if (!strcmp (parc->name, archive_name)) {
		struct stat *s=&(parc->extfsstat);
		if (vfs_uid && (!(s->st_mode & 0004)))
		    if ((s->st_gid != vfs_gid) || !(s->st_mode & 0040))
			if ((s->st_uid != vfs_uid) || !(s->st_mode & 0400))
			    return NULL; 
 /* This is not too secure - in some cases (/#mtools) files created
    under user a are probably visible to everyone else since / usually
    has permissions 755 */
	        vfs_stamp (&vfs_extfs_ops, (vfsid) parc);
		goto return_success;
	    }
	}

    result = do_not_open ? -1 : read_archive (fstype, archive_name, &parc);
    if (result == -1) ERRNOR (EIO, NULL);

    if (archive_name){
	v = vfs_type (archive_name);
	if (v == &vfs_local_ops) {
	    parent = NULL;
	} else {
	    parent = xmalloc (sizeof (struct vfs_stamping), "vfs stamping");
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
	p = xmalloc (sizeof  (struct list), "Extfs: list");
        p->next = head;
	p->name = entry->name;
	head = p;
	len += strlen (entry->name) + 1;
    }
    
    localpath = xmalloc (len, "Extfs: localpath");
    *localpath = '\0';
    for ( ; head; ) {
	strcat (localpath, head->name);
	if (head->next)
	    strcat (localpath, "/");
	p = head;
	head = head->next;
	free (p);
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
    looping = (struct loop_protect *)
	xmalloc (sizeof (struct loop_protect), 
		 "Extfs: symlink looping protection");
    looping->entry = entry;
    looping->next = list;
    pent = __find_entry (entry->dir, entry->inode->linkname, looping, 0, 0);
    free (looping);
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

struct pseudofile {
    struct archive *archive;
    unsigned int has_changed:1;
    int local_handle;
    struct entry *entry;
};

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

/* FIXME: we really should not have non-static procedures - it
 * pollutes namespace.  */

void extfs_run (char *file)
{
    struct archive *archive;
    char *p, *q, *cmd, *archive_name, *mc_extfsdir;

    if ((p = get_path (file, &archive, 0, 0)) == NULL)
	return;
    q = name_quote (p, 0);
    
    archive_name = name_quote (get_archive_name(archive), 0);
    mc_extfsdir = concat_dir_and_file (mc_home, "extfs/");
    cmd = copy_strings (mc_extfsdir, extfs_prefixes [archive->fstype], 
                        " run ", archive_name, " ", q, 0);
    free (mc_extfsdir);
    free (archive_name);
    free (q);
#ifndef VFS_STANDALONE
    shell_execute(cmd, 0);
#else
    vfs_die( "shell_execute: implement me!" );
#endif
    free(cmd);
    free(p);
}

static void *extfs_open (vfs *me, char *file, int flags, int mode)
{
    struct pseudofile *extfs_info;
    struct archive *archive;
    char *q;
    char *mc_extfsdir;
    struct entry *entry;
    int local_handle;
    const int do_create = (flags & O_ACCMODE) != O_RDONLY;
    
    if ((q = get_path_mangle (file, &archive, 0, 0)) == NULL)
	return NULL;
    entry = find_entry (archive->root_entry, q, 0, do_create);
    if (entry == NULL)
    	return NULL;
    if ((entry = my_resolve_symlinks (entry)) == NULL)
	return NULL;
    if (S_ISDIR (entry->inode->mode)) ERRNOR (EISDIR, NULL);
    if (entry->inode->local_filename == NULL) {
        char *cmd, *archive_name, *p;
        
        entry->inode->local_filename = strdup (tempnam (NULL, "extfs"));
	{
	    int handle;

	    handle = open(entry->inode->local_filename, O_RDWR | O_CREAT | O_EXCL, 0600);
	    if (handle == -1)
	      return NULL;
	    close(handle);
	}
	p = get_path_from_entry (entry);
	q = name_quote (p, 0);
	free (p);
	archive_name = name_quote (get_archive_name (archive), 0);

        mc_extfsdir = concat_dir_and_file (mc_home, "extfs/");
        cmd = copy_strings (mc_extfsdir, extfs_prefixes [archive->fstype],
                             " copyout ", 
                            archive_name, 
                            " ", q, " ", entry->inode->local_filename, 0);
	free (q);
	free (mc_extfsdir);
	free (archive_name);
        if (my_system (EXECUTE_AS_SHELL | EXECUTE_SETUID, shell, cmd) && !do_create){
            free (entry->inode->local_filename);
            entry->inode->local_filename = NULL;
            free (cmd);
            my_errno = EIO;
            return NULL;
        }
        free (cmd);
    }
    
    local_handle = open (entry->inode->local_filename, flags, mode);
    if (local_handle == -1) ERRNOR (EIO, NULL);
    
    extfs_info = (struct pseudofile *) xmalloc (sizeof (struct pseudofile), "Extfs: extfs_open");
    extfs_info->archive = archive;
    extfs_info->entry = entry;
    extfs_info->has_changed = 0;
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
    if (file->has_changed){
	struct archive *archive;
	char   *archive_name, *file_name;
	char   *cmd;
	char   *mc_extfsdir;
	char   *p;
	
	archive = file->archive;
	archive_name = name_quote (get_archive_name (archive), 0);
	p = get_path_from_entry (file->entry);
	file_name = name_quote (p, 0);
	free (p);
	
	mc_extfsdir = concat_dir_and_file (mc_home, "extfs/");
	cmd = copy_strings (mc_extfsdir,
			    extfs_prefixes [archive->fstype],
			    " copyin ", archive_name, " ",
			    file_name, " ",
			    file->entry->inode->local_filename, 0);
	free (archive_name);
	free (file_name);
	if (my_system (EXECUTE_AS_SHELL | EXECUTE_SETUID, shell, cmd))
	    errno_code = EIO;
	free (cmd);
	free (mc_extfsdir);
        {
          struct stat file_status;
          if( stat(file->entry->inode->local_filename,&file_status) != 0 )
            errno_code = EIO;
          else  file->entry->inode->size = file_status.st_size;
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
	    parent = xmalloc (sizeof (struct vfs_stamping), "vfs stamping");
	    parent->v = v;
	    parent->next = 0;
	    parent->id = (*v->getid) (v, file->archive->name, &(parent->parent));
	}
        vfs_add_noncurrent_stamps (&vfs_extfs_ops, (vfsid) (file->archive), parent);
	vfs_rm_parents (parent);
    }
	
    free (data);
    if (errno_code) ERRNOR (EIO, -1);
    return 0;
}

#define RECORDSIZE 512
#include "shared_tar_ext.c"

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

static int extfs_chdir (vfs *me, char *path)
{
    struct archive *archive;
    char *q, *res;
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
    res = copy_strings (
        entry->inode->archive->name, "#", extfs_prefixes [entry->inode->archive->fstype], 
	"/", q, NULL);
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
    free(p);
    if (archive->name){
	v = vfs_type (archive->name);
	id = (*v->getid) (v, archive->name, &par);
	if (id != (vfsid)-1) {
	    *parent = xmalloc (sizeof (struct vfs_stamping), "vfs stamping");
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
    else
    	return 0;
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
            free (e->inode->linkname);
        free (e->inode);
    }
    if (e->next_in_dir != NULL)
        free_entry (e->next_in_dir);
    free (e->name);
    free (e);
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
    p = strdup (fp->entry->inode->local_filename);
    fp->archive->fd_usage++;
    extfs_close ((void *) fp);
    return p;
}

static void extfs_ungetlocalcopy (vfs *me, char *path, char *local, int has_changed)
{
    struct pseudofile *fp = 
        (struct pseudofile *) extfs_open (me, path, O_WRONLY, 0);
    
    if (fp == NULL)
        return;
    if (!strcmp (fp->entry->inode->local_filename, local)) {
        fp->entry->inode->has_changed = has_changed;
        fp->archive->fd_usage--;
        extfs_close ((void *) fp);
        return;
    } else {
        /* Should not happen */
        extfs_close ((void *) fp);
        mc_def_ungetlocalcopy (me, path, local, has_changed);
    }
}


#include "../src/profile.h"
static int extfs_init (vfs *me)
{
    FILE *cfg;
    char *mc_extfsini;

    mc_extfsini = concat_dir_and_file (mc_home, "extfs/extfs.ini");
    cfg = fopen (mc_extfsini, "r");
    free (mc_extfsini);

    if (!cfg) {
        fprintf( stderr, "Warning: " LIBDIR "extfs/extfs.ini not found\n" );
        return 0;
    }

    extfs_no = 0;
    while ( extfs_no < MAXEXTFS ) {
        char key[256];
	char *c;

        if (!fgets( key, 250, cfg ))
	    break;

	/* Handle those with a trailing ':', those flag that the
	 * file system does not require an archive to work
	 */

	if (*key == '[') {
	/* We may not use vfs_die() message or message_1s or similar,
         * UI is not initialized at this time and message would not
	 * appear on screen. */
	    fprintf( stderr, "Warning: You need to update your " LIBDIR "extfs/extfs.ini file.\n" );
	    fclose(cfg);
	    return 0;
	}
	if (*key == '#')
	    continue;

	if ((c = strchr( key, '\n')))
	    *c = 0;
	c = &key [strlen (key)-1];
	extfs_need_archive [extfs_no] = !(*c==':');
	if (*c==':') *c = 0;
	if (!(*key))
	    continue;

	extfs_prefixes [extfs_no] = strdup (key);
	extfs_no++;
    }
    fclose(cfg);
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
	free (extfs_prefixes [i]);
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
    "Extended filesystems",
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
    NULL,

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
    
    NULL,		/* mkdir */
    NULL,
    NULL,
    extfs_setctl

MMAPNULL
};
