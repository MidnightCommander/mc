/* Virtual File System: External file system.
   Copyright (C) 1995 The Free Software Foundation
   
   Written by: 1995 Jakub Jelinek

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

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
#include "../src/mem.h"
#include "../src/mad.h"
#include "../src/main.h"	/* For shell_execute */
#define WANT_PARSE_LS_LGA
#include "vfs.h"
#include "extfs.h"

static struct extfs_entry *
extfs_find_entry (struct extfs_entry *dir, char *name, int make_dirs, int make_file);

static struct extfs_archive *first_archive = NULL;;
static int extfserrno = 0;
static struct stat hstat;		/* Stat struct corresponding */
static char *current_file_name, *current_link_name;
static char *extfs_current_dir;

enum {
    EFS_ARG_EMPTY,
    EFS_NEED_FILE,
    EFS_NEED_ARG
};

#define MAXEXTFS 32
static char *extfs_prefixes [MAXEXTFS];
static char extfs_need_archive [MAXEXTFS];
static int extfs_no = 0;

struct extfs_ext {
    char *ext;
    struct extfs_ext *next;
};

static struct extfs_ext *extfs_extensions [MAXEXTFS];

void extfs_fill_names (void (*func)(char *))
{
    struct extfs_archive *a = first_archive;
    char *name;
    
    while (a){
	name = copy_strings (extfs_prefixes [a->fstype], ":", 
			    (a->name ? a->name : ""), "/",
			     a->current_dir->name, 0);
	(*func)(name);
	free (name);
	a = a->next;
    }
}

static void make_dot_doubledot (struct extfs_entry *ent)
{
    struct extfs_entry *entry = (struct extfs_entry *)
	xmalloc (sizeof (struct extfs_entry), "Extfs: extfs_entry");
    struct extfs_entry *parentry = ent->dir;
    struct extfs_inode *inode = ent->inode, *parent;
    
    parent = (parentry != NULL) ? parentry->inode : NULL;
    entry->name = strdup (".");
    entry->has_changed = 0;
    entry->inode = inode;
    entry->dir = ent;
    inode->local_filename = NULL;
    inode->first_in_subdir = entry;
    inode->last_in_subdir = entry;
    inode->nlink++;
    entry->next_in_dir = (struct extfs_entry *)
	xmalloc (sizeof (struct extfs_entry), "Extfs: extfs_entry");
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

static struct extfs_entry *generate_entry (struct extfs_archive *archive, 
    char *name, struct extfs_entry *parentry, mode_t mode)
{
    mode_t myumask;
    struct extfs_inode *inode, *parent; 
    struct extfs_entry *entry;

    parent = (parentry != NULL) ? parentry->inode : NULL;
    entry = (struct extfs_entry *)
	xmalloc (sizeof (struct extfs_entry), "Extfs: extfs_entry");
    
    entry->name = strdup (name);
    entry->has_changed = 0;
    entry->next_in_dir = NULL;
    entry->dir = parentry;
    if (parent != NULL) {
    	parent->last_in_subdir->next_in_dir = entry;
    	parent->last_in_subdir = entry;
    }
    inode = (struct extfs_inode *)
	xmalloc (sizeof (struct extfs_inode), "Extfs: extfs_inode");
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

static void free_entries (struct extfs_entry *entry)
{
    return;
}

static void free_archive (struct extfs_archive *archive)
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

static FILE *open_extfs_archive (int fstype, char *name, struct extfs_archive **pparc)
{
    static dev_t __extfs_no = 0;
    FILE *result;
    mode_t mode;
    char *cmd;
    struct stat mystat;
    struct extfs_archive *current_archive;
    struct extfs_entry *root_entry;
    char *local_name = NULL, *tmp = 0;
    const int uses_archive = extfs_need_archive [fstype];
    
    if (uses_archive == EFS_NEED_FILE){
	if (mc_stat (name, &mystat) == -1)
	    return NULL;
	if (!vfs_file_is_local (name)) {
	    local_name = mc_getlocalcopy (name);
	    if (local_name == NULL)
		return NULL;
	}
        tmp = name_quote (name, 0);
    }

    if (uses_archive == EFS_NEED_ARG){
        tmp = name_quote (name, 0);
    }
    
    cmd = copy_strings (LIBDIR "extfs/", extfs_prefixes [fstype], 
                        " list ", local_name ? local_name : tmp, 0);
    if (tmp)
	free (tmp);
    result = popen (cmd, "r");
    free (cmd);
    if (result == NULL) {
        if (local_name != NULL && uses_archive)
            mc_ungetlocalcopy (name, local_name, 0);
    	return NULL;
    }
    
    current_archive = (struct extfs_archive *)
                      xmalloc (sizeof (struct extfs_archive), "Tar archive");
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
int read_extfs_archive (int fstype, char *name, struct extfs_archive **pparc)
{
    FILE *extfsd;
    char *buffer;
    struct extfs_archive *current_archive;

    if ((extfsd = open_extfs_archive (fstype, name, &current_archive)) == NULL) {
        message_3s (1, " Error ", "Couldn't open %s archive\n%s", 
            extfs_prefixes [fstype], name);
	return -1;
    }

    buffer = xmalloc (4096, "Extfs: buffer");
    while (fgets (buffer, 4096, extfsd) != NULL) {
        current_link_name = NULL;
        if (parse_ls_lga (buffer, &hstat, &current_file_name, &current_link_name)) {
            struct extfs_entry *entry, *pent;
            struct extfs_inode *inode;
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
                pent = extfs_find_entry (current_archive->root_entry, q, 1, 0) ;
                if (pent == NULL) {
                    message_1s (1, " Error ", "Inconsistent extfs archive");
                    /* FIXME: Should clean everything one day */
                    free (buffer);
		    pclose (extfsd);
                    return -1;
                }
		entry = (struct extfs_entry *) xmalloc (sizeof (struct extfs_entry), "Extfs: extfs_entry");
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
	            pent = extfs_find_entry (current_archive->root_entry, current_link_name, 0, 0);
	            if (pent == NULL) {
	                message_1s (1, " Error ", "Inconsistent extfs archive");
                    /* FIXME: Should clean everything one day */
			free (buffer);
			pclose (extfsd);
	                return -1;
	            } else {
			entry->inode = pent->inode;
			pent->inode->nlink++;
	            }
	        } else {
		    inode = (struct extfs_inode *) xmalloc (sizeof (struct extfs_inode), "Extfs: extfs_inode");
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
			inode->mode &= ~S_IFLNK;
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

char *extfs_analysis (char *name, char **archive, int *fstype, int is_dir)
{
    char *p, *local;
    int j;
    char *archive_name = NULL;
    struct extfs_ext *ext;
                                               /*  |  this is len of prefix plus some minimum
                                                *  v  space needed for the extension */
    *fstype = extfs_prefix_to_type (name);
    if (*fstype == -1) {
        extfserrno = ENOENT;
        return NULL;
    }

    if (extfs_need_archive [*fstype] == EFS_ARG_EMPTY){
	*archive = 0;
	return strdup (name + strlen(extfs_prefixes [*fstype]) + 1);
    }

    if (extfs_need_archive [*fstype] == EFS_NEED_ARG){
	char c;
	p = strchr (name + strlen (extfs_prefixes [*fstype]) + 1, '/');
	if (p){
	    c = *p;
	    *p = 0;
	}
	archive_name = strdup (name + strlen (extfs_prefixes [*fstype]) + 1);
	if (p)
	    *p = c;
	*archive = archive_name;
	return p ? strdup (p+1) : NULL;
    }
    for (p = name + strlen (name); p > name; p--)
    	if (*p == '/' || (is_dir && !*p))
    	    for (ext = extfs_extensions [*fstype]; ext; ext = ext->next) {
    	        j = strlen (ext->ext);
    	        if (p - j < name + 1)
    	            continue;
    	        if (!strncmp (p - j, ext->ext, j)) {
    	            char c = *p;
    	            
    	            *p = 0;
    	            archive_name = 
    	                vfs_canon (name + strlen (extfs_prefixes [*fstype]) + 1);
    	            *archive = archive_name;
    	            *p = c;
    	            local = strdup (p);
    	            return local;
    	        }
    	    }
    extfserrno = ENOENT;
    return NULL;
}

/* Returns allocated path inside the archive or NULL */
static char *extfs_get_path (char *inname, struct extfs_archive **archive, int is_dir,
    int do_not_open)
{
    char *local, *archive_name;
    int result = -1;
    struct extfs_archive *parc;
    struct vfs_stamping *parent;
    vfs *v;
    int fstype;
    
    local = extfs_analysis (inname, &archive_name, &fstype, is_dir);
    if (local == NULL) {
    	extfserrno = ENOENT;
    	return NULL;
    }

    /* In the case of the file systems that do not require a local
     * archive, we compare the fstype instead of the archive name,
     * of course, this has the drawback that we can't have more than
     * one external file system for each prefix.
     *
     * Actually, we should implement an alias mechanism that would
     * translate: "a:" to "dos:a.
     *
     */
    for (parc = first_archive; parc != NULL; parc = parc->next)
	if (archive_name){
	    if (parc->name) {
		if (!strcmp (parc->name, archive_name)) {
		    vfs_stamp (&extfs_vfs_ops, (vfsid) parc);
		    goto return_success;
		}
	    }
	} else {
	    if (parc->fstype == fstype){
		vfs_stamp (&extfs_vfs_ops, (vfsid) parc);
		goto return_success;
	    }
	}
	    
    if (do_not_open)
        result = -1;
    else
        result = read_extfs_archive (fstype, archive_name, &parc);
    if (result == -1) {
	extfserrno = EIO;
	free(local);
	if (archive_name)
	    free(archive_name);
	return NULL;
    }
    if (archive_name){
	v = vfs_type (archive_name);
	if (v == &local_vfs_ops) {
	    parent = NULL;
	} else {
	    parent = xmalloc (sizeof (struct vfs_stamping), "vfs stamping");
	    parent->v = v;
	    parent->next = 0;
	    parent->id = (*v->getid) (archive_name, &(parent->parent));
	}
	vfs_add_noncurrent_stamps (&extfs_vfs_ops, (vfsid) parc, parent);
	vfs_rm_parents (parent);
    }
 return_success:
    *archive = parc;
    if (archive_name)
	free (archive_name);
    return local;
}

/* Returns allocated path (without leading slash) inside the archive  */
static char *extfs_get_path_from_entry (struct extfs_entry *entry)
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


struct extfs_loop_protect {
    struct extfs_entry *entry;
    struct extfs_loop_protect *next;
};
static int errloop;
static int notadir;

static struct extfs_entry *
__extfs_find_entry (struct extfs_entry *dir, char *name,
		    struct extfs_loop_protect *list, int make_dirs, int make_file);

static struct extfs_entry *
__extfs_resolve_symlinks (struct extfs_entry *entry, 
			  struct extfs_loop_protect *list)
{
    struct extfs_entry *pent;
    struct extfs_loop_protect *looping;
    
    if (!S_ISLNK (entry->inode->mode))
    	return entry;
    for (looping = list; looping != NULL; looping = looping->next)
    	if (entry == looping->entry) { /* Here we protect us against symlink looping */
    	    errloop = 1;
    	    return NULL;
    	}
    looping = (struct extfs_loop_protect *)
	xmalloc (sizeof (struct extfs_loop_protect), 
		 "Extfs: symlink looping protection");
    looping->entry = entry;
    looping->next = list;
    pent = __extfs_find_entry (entry->dir, entry->inode->linkname, looping, 0, 0);
    free (looping);
    if (pent == NULL)
    	extfserrno = ENOENT;
    return pent;
}

static struct extfs_entry *extfs_resolve_symlinks (struct extfs_entry *entry)
{
    struct extfs_entry *res;
    
    errloop = 0;
    notadir = 0;
    res = __extfs_resolve_symlinks (entry, NULL);
    if (res == NULL) {
    	if (errloop)
    	    extfserrno = ELOOP;
    	else if (notadir)
    	    extfserrno = ENOTDIR;
    }
    return res;
}

static struct extfs_entry*
__extfs_find_entry (struct extfs_entry *dir, char *name, 
		     struct extfs_loop_protect *list, int make_dirs, int make_file)
{
    struct extfs_entry *pent, *pdir;
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
		if ((pent = __extfs_resolve_symlinks (pent, list))==NULL){
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
    	extfserrno = ENOENT;
    return pent;
}

static struct extfs_entry *extfs_find_entry (struct extfs_entry *dir, char *name, int make_dirs, int make_file)
{
    struct extfs_entry *res;
    
    errloop = 0;
    notadir = 0;
    res = __extfs_find_entry (dir, name, NULL, make_dirs, make_file);
    if (res == NULL) {
    	if (errloop)
    	    extfserrno = ELOOP;
    	else if (notadir)
    	    extfserrno = ENOTDIR;
    }
    return res;
}

struct extfs_pseudofile {
    struct extfs_archive *archive;
    unsigned int has_changed:1;
    int local_handle;
    struct extfs_entry *entry;
};

static char *get_archive_name (struct extfs_archive *archive)
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

void extfs_run (char *file)
{
    struct extfs_archive *archive;
    char *p, *q, *cmd, *archive_name;

    if ((p = extfs_get_path (file, &archive, 0, 0)) == NULL)
	return;
    q = name_quote (((*p == '/') ? p + 1 : p), 0);
    
    archive_name = name_quote (get_archive_name(archive), 0);
    cmd = copy_strings (LIBDIR "extfs/", extfs_prefixes [archive->fstype], 
                        " run ", archive_name, " ", q, 0);
    free (archive_name);
    free (q);
    shell_execute(cmd, 0);
    free(cmd);
    free(p);
}

static void *extfs_open (char *file, int flags, int mode)
{
    struct extfs_pseudofile *extfs_info;
    struct extfs_archive *archive;
    char *p, *q;
    struct extfs_entry *entry;
    int local_handle;
    const int do_create = (flags & O_ACCMODE) != O_RDONLY;
    
    if ((p = extfs_get_path (file, &archive, 0, 0)) == NULL)
	return NULL;
    q = (*p == '/') ? p + 1 : p;
    entry = extfs_find_entry (archive->root_entry, q, 0, do_create);
    free (p);
    if (entry == NULL) {
    	return NULL;
    }
    if ((entry = extfs_resolve_symlinks (entry)) == NULL) {
	return NULL;
    }
    if (S_ISDIR (entry->inode->mode)) {
    	extfserrno = EISDIR;
    	return NULL;
    }
    if (entry->inode->local_filename == NULL) {
        char *cmd, *archive_name;
        
        entry->inode->local_filename = strdup (tmpnam (NULL));
	p = extfs_get_path_from_entry (entry);
	q = name_quote (p, 0);
	free (p);
	archive_name = name_quote (get_archive_name (archive), 0);
        cmd = copy_strings (LIBDIR "extfs/", extfs_prefixes [archive->fstype], 
                            " copyout ", 
                            archive_name, 
                            " ", q, " ", entry->inode->local_filename, 0);
	free (q);
	free (archive_name);
        if (my_system (EXECUTE_AS_SHELL, shell, cmd) && !do_create){
            free (entry->inode->local_filename);
            entry->inode->local_filename = NULL;
            free (cmd);
            extfserrno = EIO;
            return NULL;
        }
        free (cmd);
    }
    
    local_handle = open (entry->inode->local_filename, flags, mode);
    if (local_handle == -1) {
        extfserrno = EIO;
        return NULL;
    }
    
    extfs_info = (struct extfs_pseudofile *) xmalloc (sizeof (struct extfs_pseudofile), "Extfs: extfs_open");
    extfs_info->archive = archive;
    extfs_info->entry = entry;
    extfs_info->has_changed = 0;
    extfs_info->local_handle = local_handle;

    /* i.e. we had no open files and now we have one */
    vfs_rmstamp (&extfs_vfs_ops, (vfsid) archive, 1);
    archive->fd_usage++;
    return extfs_info;
}

static int extfs_read (void *data, char *buffer, int count)
{
    struct extfs_pseudofile *file = (struct extfs_pseudofile *)data;

    return read (file->local_handle, buffer, count);
}

static int extfs_close (void *data)
{
    struct extfs_pseudofile *file;
    int    errno_code = 0;
    file = (struct extfs_pseudofile *)data;
    
    close (file->local_handle);

    /* Commit the file if it has changed */
    if (file->has_changed){
	struct extfs_archive *archive;
	char   *archive_name, *file_name;
	char   *cmd;
	char   *p;
	
	archive = file->archive;
	archive_name = name_quote (get_archive_name (archive), 0);
	p = extfs_get_path_from_entry (file->entry);
	file_name = name_quote (p, 0);
	free (p);
	
	cmd = copy_strings (LIBDIR "extfs/",
			    extfs_prefixes [archive->fstype],
			    " copyin ", archive_name, " ",
			    file_name, " ",
			    file->entry->inode->local_filename, 0);
	free (archive_name);
	free (file_name);
	if (my_system (EXECUTE_AS_SHELL, shell, cmd))
	    errno_code = EIO;
	free (cmd);
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
        
	if (!file->archive->name || !*file->archive->name || (v = vfs_type (file->archive->name)) == &local_vfs_ops) {
	    parent = NULL;
	} else {
	    parent = xmalloc (sizeof (struct vfs_stamping), "vfs stamping");
	    parent->v = v;
	    parent->next = 0;
	    parent->id = (*v->getid) (file->archive->name, &(parent->parent));
	}
        vfs_add_noncurrent_stamps (&extfs_vfs_ops, (vfsid) (file->archive), parent);
	vfs_rm_parents (parent);
    }
	
    free (data);
    if (errno_code){
	extfserrno = EIO;	/* Non standard, but who cares? */
	return -1;
    } else
	return 0;
}

static int extfs_errno (void)
{
    return extfserrno;
}

static void *extfs_opendir (char *dirname)
{
    struct extfs_archive *archive;
    char *p, *q;
    struct extfs_entry *entry;
    struct extfs_entry **extfs_info;

    if ((p = extfs_get_path (dirname, &archive, 1, 0)) == NULL)
	return NULL;
    q = (*p == '/') ? p + 1 : p;
    entry = extfs_find_entry (archive->root_entry, q, 0, 0);
    free (p);
    if (entry == NULL)
    	return NULL;
    if ((entry = extfs_resolve_symlinks (entry)) == NULL)
	return NULL;
    if (!S_ISDIR (entry->inode->mode)) {
    	extfserrno = ENOTDIR;
    	return NULL;
    }

    extfs_info = (struct extfs_entry **) xmalloc (sizeof (struct extfs_entry *), "Extfs: extfs_opendir");
    *extfs_info = entry->inode->first_in_subdir;

    return extfs_info;
}

static void *extfs_readdir (void *data)
{
    static struct {
	struct dirent dir;
#ifdef NEED_EXTRA_DIRENT_BUFFER
	char extra_buffer [MC_MAXPATHLEN];
#endif
    } dir;
    
    struct extfs_entry **extfs_info = (struct extfs_entry **) data;
    
    if (*extfs_info == NULL)
    	return NULL;
    
    strcpy (&(dir.dir.d_name [0]), (*extfs_info)->name);
    
#ifndef DIRENT_LENGTH_COMPUTED
    dir.d_namlen = strlen (dir.dir.d_name);
#endif
    *extfs_info = (*extfs_info)->next_in_dir;
    
    return (void *)&dir;
}

static int extfs_closedir (void *data)
{
    free (data);
    return 0;
}

static int _extfs_stat (char *path, struct stat *buf, int resolve)
{
    struct extfs_archive *archive;
    char *p, *q;
    struct extfs_entry *entry;
    struct extfs_inode *inode;

    if ((p = extfs_get_path (path, &archive, 0, 0)) == NULL)
	return -1;
    q = (*p == '/') ? p + 1 : p;
    entry = extfs_find_entry (archive->root_entry, q, 0, 0);
    free (p);
    if (entry == NULL)
    	return -1;
    if (resolve && (entry = extfs_resolve_symlinks (entry)) == NULL)
	return -1;
    inode = entry->inode;
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
    buf->st_blksize = 512;
#endif
#ifdef HAVE_ST_BLOCKS
    buf->st_blocks = (inode->size + 512 - 1) / 512;
#endif
    buf->st_atime = inode->atime;
    buf->st_mtime = inode->mtime;
    buf->st_ctime = inode->ctime;
    return 0;
}

static int extfs_stat (char *path, struct stat *buf)
{
    return _extfs_stat (path, buf, 1);
}

static int extfs_lstat (char *path, struct stat *buf)
{
    return _extfs_stat (path, buf, 0);
}

static int extfs_fstat (void *data, struct stat *buf)
{
    struct extfs_pseudofile *file = (struct extfs_pseudofile *)data;
    struct extfs_inode *inode;
    
    inode = file->entry->inode;
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
    buf->st_blksize = 512;
#endif
#ifdef HAVE_ST_BLOCKS
    buf->st_blocks = (inode->size + 512 - 1) / 512;
#endif
    buf->st_atime = inode->atime;
    buf->st_mtime = inode->mtime;
    buf->st_ctime = inode->ctime;
    return 0;
}

static int extfs_chmod (char *path, int mode)
{
    return 0;
}

static int extfs_chown (char *path, int owner, int group)
{
    return -1;
}

static int extfs_readlink (char *path, char *buf, int size)
{
    struct extfs_archive *archive;
    char *p, *q;
    int i;
    struct extfs_entry *entry;

    if ((p = extfs_get_path (path, &archive, 0, 0)) == NULL)
	return -1;
    q = (*p == '/') ? p + 1 : p;
    entry = extfs_find_entry (archive->root_entry, q, 0, 0);
    free (p);
    if (entry == NULL)
    	return -1;
    if (!S_ISLNK (entry->inode->mode)) {
        extfserrno = EINVAL;
        return -1;
    }
    if (size > (i = strlen (entry->inode->linkname))) {
    	size = i;
    }
    strncpy (buf, entry->inode->linkname, i);
    return i;
}

static int extfs_unlink (char *path)
{
    return -1;
}

static int extfs_symlink (char *n1, char *n2)
{
    return -1;
}

static int extfs_write (void *data, char *buf, int nbyte)
{
    struct extfs_pseudofile *file = (struct extfs_pseudofile *)data;

    file->has_changed = 1;
    return write (file->local_handle, buf, nbyte);
}

static int extfs_rename (char *a, char *b)
{
    return -1;
}

static int extfs_chdir (char *path)
{
    struct extfs_archive *archive;
    char *p, *q, *res;
    struct extfs_entry *entry;

    extfserrno = ENOTDIR;
    if ((p = extfs_get_path (path, &archive, 1, 0)) == NULL)
	return -1;
    q = (*p == '/') ? p + 1 : p;
    entry = extfs_find_entry (archive->root_entry, q, 0, 0);
    if (entry == NULL) {
    	free (p);
    	return -1;
    }
    entry = extfs_resolve_symlinks (entry);
    if (entry == NULL) {
    	free (p);
    	return -1;
    }
    if (!S_ISDIR (entry->inode->mode)) {
    	free (p);
    	return -1;
    }
    entry->inode->archive->current_dir = entry;
    res = copy_strings (extfs_prefixes [entry->inode->archive->fstype], ":",
        entry->inode->archive->name, p, NULL);
    free (p);
    extfserrno = 0;
    if (extfs_current_dir)
	free (extfs_current_dir);
    extfs_current_dir = res;
    return 0;
}

static int extfs_lseek (void *data, off_t offset, int whence)
{
    struct extfs_pseudofile *file = (struct extfs_pseudofile *) data;

    return lseek (file->local_handle, offset, whence);
}

static int extfs_mknod (char *path, int mode, int dev)
{
    return -1;
}

static int extfs_link (char *p1, char *p2)
{
    return -1;
}

static int extfs_mkdir (char *path, mode_t mode)
{
    return -1;
}

static int extfs_rmdir (char *path)
{
    return -1;
}

static vfsid extfs_getid (char *path, struct vfs_stamping **parent)
{
    struct extfs_archive *archive;
    vfs *v;
    vfsid id;
    char *p;
    struct vfs_stamping *par;

    *parent = NULL;
    if ((p = extfs_get_path (path, &archive, 1, 1)) == NULL) {
	return (vfsid) -1;
    }
    free (p);
    if (archive->name){
	v = vfs_type (archive->name);
	id = (*v->getid) (archive->name, &par);
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
    if (((struct extfs_archive *)id)->fd_usage <= 0)
    	return 1;
    else
    	return 0;
}

static void free_entry (struct extfs_entry *e)
{
    int i = --(e->inode->nlink);
    if (S_ISDIR (e->inode->mode) && e->inode->first_in_subdir != NULL) {
        struct extfs_entry *f = e->inode->first_in_subdir;
        
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
    struct extfs_archive *parc;
    struct extfs_archive *archive = (struct extfs_archive *)id;

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

static char *extfs_getlocalcopy (char *path)
{
    struct extfs_pseudofile *fp = 
        (struct extfs_pseudofile *) extfs_open (path, O_RDONLY, 0);
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

static void extfs_ungetlocalcopy (char *path, char *local, int has_changed)
{
    struct extfs_pseudofile *fp = 
        (struct extfs_pseudofile *) extfs_open (path, O_WRONLY, 0);
    
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
        mc_def_ungetlocalcopy (path, local, has_changed);
    }
}

#ifdef HAVE_MMAP
caddr_t extfs_mmap (caddr_t addr, size_t len, int prot, int flags, void *data, off_t offset)
{
    return (caddr_t)-1;
}

int extfs_munmap (caddr_t addr, size_t len, void *data)
{
    return -1;
}
#endif

vfs extfs_vfs_ops =
{
    extfs_open,
    extfs_close,
    extfs_read,
    extfs_write,

    extfs_opendir,
    extfs_readdir,
    extfs_closedir,

    extfs_stat,
    extfs_lstat,
    extfs_fstat,

    extfs_chmod,		/* unimplemented */
    extfs_chown,		/* unimplemented */
    NULL,

    extfs_readlink,
    
    extfs_symlink,		/* unimplemented */
    extfs_link,			/* unimplemented */
    extfs_unlink,		/* unimplemented */

    extfs_rename,		/* unimplemented */
    extfs_chdir,
    extfs_errno,
    extfs_lseek,
    extfs_mknod,		/* unimplemented */
    
    extfs_getid,
    extfs_nothingisopen,
    extfs_free,
    
    extfs_getlocalcopy,
    extfs_ungetlocalcopy,
    
    extfs_mkdir,		/* unimplemented */
    extfs_rmdir,		/* unimplemented */
    NULL,
    NULL,
    NULL
#ifdef HAVE_MMAP
    , extfs_mmap,
    extfs_munmap
#endif    
};

#include "../src/profile.h"
void extfs_init (void)
{
    void *keys = profile_init_iterator ("extfs", LIBDIR "extfs/extfs.ini");
    char *key, *value;
    char *p, *q, c;
    
    extfs_no = 0;
    for (extfs_no = 0; keys != NULL && extfs_no < MAXEXTFS; extfs_no++) {
	keys = profile_iterator_next (keys, &key, &value);

	/* Handle those with a trailing ':', those flag that the
	 * file system does not require an archive to work
	 */

	c = key [strlen (key)-1];
	if (c == ':'){
	    key [strlen (key)-1] = 0;
	    extfs_need_archive [extfs_no] = EFS_ARG_EMPTY;
	} else if (c == '*'){
	    key [strlen (key)-1] = 0;
	    extfs_need_archive [extfs_no] = EFS_NEED_ARG;
	} else 
	    extfs_need_archive [extfs_no] = EFS_NEED_FILE;
	
	extfs_prefixes [extfs_no] = strdup (key);

	value = strdup (value);
	for (q = value; (p = strtok (q, " \t")) != NULL; ){
	    struct extfs_ext *newn;

	    q = NULL;
	    newn = xmalloc (sizeof (struct extfs_ext), "prefixes");
	    newn->next = extfs_extensions [extfs_no];
	    newn->ext = strdup (p);
	    extfs_extensions [extfs_no] = newn;
	}
	free (value);
    }
    free_profile_name (LIBDIR "extfs/extfs.ini");
}

int extfs_prefix_to_type (char *path)
{
    int i, j;

    for (i = 0; i < extfs_no; i++) {
        j = strlen (extfs_prefixes [i]);
        if (!strncmp (path, extfs_prefixes [i], j) && path [j] == ':')
            return i;
    } 
    return -1;
}

char *extfs_get_prefix (int idx)
{
    return extfs_prefixes [idx];
}

void extfs_done (void)
{
    struct extfs_ext *current, *old;
    int i;

    for (i = 0; i < extfs_no; i++ ){
	free (extfs_prefixes [i]);
	current = extfs_extensions [i];
	while (current){
	    old = current;
	    current = current->next;
	    free (old->ext);
	    free (old);
	}
    }
    extfs_no = 0;
    if (extfs_current_dir)
	free (extfs_current_dir);
    extfs_current_dir = 0;
}

