/* Virtual File System: GNU Tar file system.
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

/* Namespace: vfs_tarfs_ops, tar_gzipped_memlimit */

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
#include "../src/dialog.h"	/* For MSG_ERROR */
#include "../src/mem.h"
#include "../src/mad.h"
#include "../src/main.h"
#include "vfs.h"
#include "tar.h"
#include "names.h"

/* The limit for a compressed tar file to be loaded in core */
int tar_gzipped_memlimit = 1*1024*1024;

/* used to rotate the dash */
static int dash_number = 0;

#define	isodigit(c)	( ((c) >= '0') && ((c) <= '7') )
/*
 * Quick and dirty octal conversion.
 *
 * Result is -1 if the field is invalid (all blank, or nonoctal).
 */
static long from_oct (int digs, char *where)
{
    register long value;

    while (isspace (*where)) {	/* Skip spaces */
	where++;
	if (--digs <= 0)
	    return -1;		/* All blank field */
    }
    value = 0;
    while (digs > 0 && isodigit (*where)) {	/* Scan till nonoctal */
	value = (value << 3) | (*where++ - '0');
	--digs;
    }

    if (digs > 0 && *where && !isspace (*where))
	return -1;		/* Ended on non-space/nul */

    return value;
}

static struct archive *first_archive = NULL;
static int my_errno = 0;
static struct stat hstat;		/* Stat struct corresponding */
static char *current_file_name, *current_link_name;
static struct entry *find_entry (struct entry *dir, char *name, int make_dirs, int make_file);

static void tarfs_fill_names (vfs *me, void (*func)(char *))
{
    struct archive *a = first_archive;
    char *name;
    
    while (a){
	name = copy_strings ( a->name, "#tar/",
			     a->current_dir->name, 0);
	(*func)(name);
	free (name);
	a = a->next;
    }
}

static void make_dot_doubledot (struct entry *ent)
{
    struct entry *entry = (struct entry *)
	xmalloc (sizeof (struct entry), "Tar: entry");
    struct entry *parentry = ent->dir;
    struct inode *inode = ent->inode, *parent;
    
    parent = (parentry != NULL) ? parentry->inode : NULL;
    entry->name = strdup (".");
    entry->has_changed = 0;
    entry->header_offset = -1;
    entry->header_size = 0;
    entry->extended_offset = -1;
    entry->extended_size = 0;
    entry->inode = inode;
    entry->dir = ent;
    inode->first_in_subdir = entry;
    inode->last_in_subdir = entry;
    inode->nlink++;
    entry->next_in_dir = (struct entry *)
	xmalloc (sizeof (struct entry), "Tar: entry");
    entry=entry->next_in_dir;
    entry->name = strdup ("..");
    entry->has_changed = 0;
    entry->header_offset = -1;
    entry->header_size = 0;
    entry->extended_offset = -1;
    entry->extended_size = 0;
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
	xmalloc (sizeof (struct entry), "Tar: entry");
    
    entry->name = strdup (name);
    entry->has_changed = 0;
    entry->header_offset = -1;
    entry->header_size = 0;
    entry->extended_offset = -1;
    entry->extended_size = 0;
    entry->next_in_dir = NULL;
    entry->dir = parentry;
    if (parent != NULL) {
    	parent->last_in_subdir->next_in_dir = entry;
    	parent->last_in_subdir = entry;
    }
    inode = (struct inode *)
	xmalloc (sizeof (struct inode), "Tar: inode");
    entry->inode = inode;
    inode->local_filename = NULL;
    inode->has_changed = 0;
    inode->is_open = 0;
    inode->linkname = 0;
    inode->inode = (archive->__inode_counter)++;
    inode->dev = archive->rdev;
    inode->archive = archive;
    inode->data_offset = -1;
    myumask = umask (022);
    umask (myumask);
    inode->mode = mode & ~myumask;
    mode = inode->mode;
    inode->rdev = 0;
    inode->uid = getuid ();
    inode->gid = getgid ();
    inode->std = 1;
    inode->size = 0;
    inode->mtime = time (NULL);
    inode->atime = inode->mtime;
    inode->ctime = inode->mtime;
    inode->nlink = 1;
    if (S_ISDIR (mode)) {
        inode->linkflag = LF_DIR;
        make_dot_doubledot (entry);
    } else if (S_ISLNK (mode)) {
        inode->linkflag = LF_SYMLINK;
    } else if (S_ISCHR (mode)) {
    	inode->linkflag = LF_CHR;
    } else if (S_ISBLK (mode)) {
    	inode->linkflag = LF_BLK;
    } else if (S_ISFIFO (mode) || S_ISSOCK (mode)) {
    	inode->linkflag = LF_FIFO;
    } else {
    	inode->linkflag = LF_NORMAL;
    }
    return entry;
}

static void free_entries (struct entry *entry)
{
    return;
}

static void free_archive (struct archive *archive)
{
    long l;

    if (archive->is_gzipped == targz_growing) {
    	free (archive->block_first);
    	if (archive->block_ptr != NULL) {
    	    for (l = 0; l < archive->count_blocks; l++)
    	        free (archive->block_ptr [l]);
    	    free (archive->block_ptr);
    	}
    } else {
        if (archive->tmpname)
    	    free (archive->tmpname);
	if (archive->fd != -1)
	    mc_close(archive->fd);
    }
    free_entries (archive->root_entry);
    
    free (archive->name);
    free (archive);
}

static INLINE int gzip_limit_ok (int size)
{
    return (size <= tar_gzipped_memlimit || tar_gzipped_memlimit < 0);
}

/* So we have to decompress it... 
 * It is not that easy, because we would like to handle all the files
 * from all the vfs's. So, we do this like this:
 * we run a pipe:
 *    for (;;) mc_read | gzip -cdf | store into growing buffer
 *
 * Returns: 0 on failure
 */
static INLINE int load_compressed_tar (struct archive *current_archive,
					int size, int fd, int type)
{	
    int pipehandle, i;
    long l, l2;
    union record *ur;
    pid_t p;
    char  *cmd, *cmd_flags;
    void *tmp;
    
    l2 = 0;
    current_archive->is_gzipped = targz_growing;
    size = (size + RECORDSIZE - 1) / RECORDSIZE * RECORDSIZE;
    current_archive->count_first = size / RECORDSIZE;
    current_archive->count_blocks = 0;
    current_archive->block_ptr = NULL;
    current_archive->root_entry = NULL;

    decompress_command_and_arg (type, &cmd, &cmd_flags);
    pipehandle = mc_doublepopen (fd, -1, &p, cmd, cmd, cmd_flags, NULL);
    if (pipehandle == -1)
    {
	free (current_archive);
	mc_close (fd);
	return 0;
    }

    /* On some systems it's better to allocate the memory for the tar-file
       this way:
       - allocate some memory we don't need (size 100k).
       - allocate the memory for the uncompressed tar-file (the size of 
         this memory block can be really big - five times the size of the 
         compressed tar-file). 
       - free the 100k from the first step

       Without the extra steps a few malloc/free implementations can't give 
       back memory to the operating system when the memory for the tar-file is
       freed after the vfs-timeout.
     */
    tmp = malloc (100*1024);
    current_archive->block_first = (union record *) malloc (size);
    if (tmp)
       free (tmp); /* make a hole in size of 100k */
    if (0 == current_archive->block_first) {
    	mc_doublepclose (pipehandle, p);
	free (current_archive);
	mc_close (fd);
	return 0;
    }

    ur = current_archive->block_first;
    l = 0;
    while ((i = read (pipehandle, (char *) ur, RECORDSIZE)) == RECORDSIZE) {
	l++;
	if (l >= current_archive->count_first) {
	    l2 = l - current_archive->count_first;
	    if (l2 % TAR_GROWING_CHUNK_SIZE)
		ur++;
	    else {
		union record **tmp = (union record **)
		    xmalloc ((++current_archive->count_blocks) *
			     sizeof (union record **), "Tar: Growing buffers");
		
		if (current_archive->block_ptr != NULL) {
		    bcopy (current_archive->block_ptr, tmp, 
			   (current_archive->count_blocks - 1) *
			   sizeof (union record **));
		    free (current_archive->block_ptr);
		}
		current_archive->block_ptr = tmp;
		ur = (union record *)
		    xmalloc (TAR_GROWING_CHUNK_SIZE * RECORDSIZE,
			     "Tar: Growing buffers");
		current_archive->block_ptr [current_archive->count_blocks - 1] = ur;
	    }
	} else
	    ur++;
	if ((dash_number++ % 64) == 0)
	    rotate_dash ();
    }
    i = mc_doublepclose (pipehandle, p);
    mc_close (fd);
    if (i == -1) {
	free_archive (current_archive);
	return 0;
    }
    current_archive->current_record = current_archive->block_first;
    return 1;
}

/* Returns a file handle of the opened local tar file or -1 on error */
static INLINE int uncompress_tar_file  (struct archive *current_archive,
					int size, int fd, int type)
{
    int result;

  /* From time we have sfs this function is pretty trivial */
    current_archive->is_gzipped = tar_uncompressed_local;
    current_archive->tmpname = copy_strings( current_archive->name, 
					     decompress_extension (type), NULL );
    
    result = mc_open (current_archive->tmpname, O_RDONLY);
    if (result == -1)
	free_archive (current_archive);
    return result;
}

/* Returns fd of the open tar file */
static int open_tar_archive (char *name, struct archive **pparc)
{
    static dev_t __tar_no = 0;
    int result, type;
    long size;
    mode_t mode;
    struct archive *current_archive;
    static struct entry *root_entry;
    
    result = mc_open (name, O_RDONLY);
    if (result == -1) {
        message_2s (1, MSG_ERROR, _("Couldn't open tar archive\n%s"), name);
    	return -1;
    }
    
    current_archive = (struct archive *)
                      xmalloc (sizeof (struct archive), "Tar archive");
    current_archive->current_tar_position = 0;
    current_archive->name = strdup (name);
    current_archive->__inode_counter = 0;
    mc_stat (name, &(current_archive->tarstat));
    current_archive->rdev = __tar_no++;
    current_archive->next = first_archive;
    current_archive->fd_usage = 0;
    current_archive->fd = -1;
    current_archive->tmpname = NULL;
    size = is_gunzipable (result, &type);
    mc_lseek (result, 0, SEEK_SET);

    /* Find out the method to handle this tar file */
    if (size > 0 && gzip_limit_ok (size)) {
        if (load_compressed_tar (current_archive, size, result, type) == 0) {
	    message_2s (1, MSG_ERROR, _("Couldn't load compressed tar\n%s"), name);
	    return -1;
	}
	result = 0;
    } else if (size > 0) {
	result = uncompress_tar_file (current_archive, size, result, type);
	if (result == -1) {
	    message_2s (1, MSG_ERROR, _("Couldn't uncompress tar archive\n%s"), name);
	    return -1;
	}
    } else {
    	current_archive->is_gzipped = tar_normal;
    }
   
    current_archive->fd = result;
    first_archive = current_archive;
    mode = current_archive->tarstat.st_mode & 07777;
    if (mode & 0400)
    	mode |= 0100;
    if (mode & 0040)
    	mode |= 0010;
    if (mode & 0004)
    	mode |= 0001;
    mode |= S_IFDIR;
    root_entry = generate_entry (current_archive, "/", NULL, mode);
    root_entry->inode->uid = current_archive->tarstat.st_uid;
    root_entry->inode->gid = current_archive->tarstat.st_gid;
    root_entry->inode->atime = current_archive->tarstat.st_atime;
    root_entry->inode->ctime = current_archive->tarstat.st_ctime;
    root_entry->inode->mtime = current_archive->tarstat.st_mtime;
    current_archive->root_entry = root_entry;
    current_archive->current_dir = root_entry;
    
    *pparc = current_archive;

    return result;
}

static int get_current_position (struct archive *archive, int tard)
{
    return archive->current_tar_position;
}

static union record *find_current_record (struct archive *archive, long pos)
{
    long l, l2;
    static union record *ur;
        
    l = pos / RECORDSIZE;
    if (l >= archive->count_first) {
	l2 = l - archive->count_first;
        ur = archive->block_ptr [l2 / TAR_GROWING_CHUNK_SIZE] 
             + (l2 % TAR_GROWING_CHUNK_SIZE);
    } else
        ur = archive->block_first + l;
    return ur;
}

static union record rec_buf;

static union record *get_next_record (struct archive *archive, int tard)
{
    if (archive->is_gzipped == targz_growing) {
    	bcopy (archive->current_record, rec_buf.charptr, RECORDSIZE);
    	archive->current_record = find_current_record (archive, archive->current_tar_position + RECORDSIZE); 
    } else if (mc_read (tard, rec_buf.charptr, RECORDSIZE) != RECORDSIZE)
	return NULL;		/* An error has occurred */
    archive->current_tar_position += RECORDSIZE;
    return &rec_buf;
}

static void skip_n_records (struct archive *archive, int tard, int n)
{
    if (archive->is_gzipped == targz_growing)
    	archive->current_record = find_current_record (archive, archive->current_tar_position + n * RECORDSIZE);
    else
        mc_lseek (tard, n * RECORDSIZE, SEEK_CUR);
    archive->current_tar_position += n * RECORDSIZE;
}

/*
 * Return 1 for success, 0 if the checksum is bad, EOF on eof,
 * 2 for a record full of zeros (EOF marker).
 *
 */
static int read_header (struct archive *archive, int tard)
{
    register int i;
    register long sum, signed_sum, recsum;
    register char *p;
    register union record *header;
    char **longp;
    char *bp, *data;
    int size, written;
    static char *next_long_name = NULL, *next_long_link = NULL;
    long header_position = get_current_position (archive, tard);

  recurse:

    header = get_next_record (archive, tard);
    if (NULL == header)
	return EOF;

    recsum = from_oct (8, header->header.chksum);

    sum = 0; signed_sum = 0;
    p = header->charptr;
    for (i = sizeof (*header); --i >= 0;) {
	        /*
		 * We can't use unsigned char here because of old compilers,
		 * e.g. V7.
		 */
	signed_sum += *p;
	sum += 0xFF & *p++;
    }

    /* Adjust checksum to count the "chksum" field as blanks. */
    for (i = sizeof (header->header.chksum); --i >= 0;) {
	sum -= 0xFF & header->header.chksum[i];
	signed_sum -= (char) header->header.chksum[i];
    }
    sum += ' ' * sizeof header->header.chksum;
    signed_sum += ' ' * sizeof header->header.chksum;

    if (sum == 8 * ' ') {
	/*
	 * This is a zeroed record...whole record is 0's except
	 * for the 8 blanks we faked for the checksum field.
	 */
	return 2;
    }
    if (sum != recsum && signed_sum != recsum)
	return 0;
    
    /*
     * linkflag on BSDI tar (pax) always '\000'
     */
    if(header->header.linkflag == '\000' &&
       strlen(header->header.arch_name) &&
       header->header.arch_name[strlen(header->header.arch_name) - 1] == '/')
	header->header.linkflag = LF_DIR;
    
     /*
     * Good record.  Decode file size and return.
     */
    if (header->header.linkflag == LF_LINK || header->header.linkflag == LF_DIR)
	hstat.st_size = 0;	/* Links 0 size on tape */
    else
	hstat.st_size = from_oct (1 + 12, header->header.size);

    header->header.arch_name[NAMSIZ - 1] = '\0';
    if (header->header.linkflag == LF_LONGNAME
	|| header->header.linkflag == LF_LONGLINK) {
	longp = ((header->header.linkflag == LF_LONGNAME)
		 ? &next_long_name
		 : &next_long_link);

	if (*longp)
	    free (*longp);
	bp = *longp = (char *) xmalloc (hstat.st_size, "Tar: Long name");

	for (size = hstat.st_size;
	     size > 0;
	     size -= written) {
	    data = get_next_record (archive, tard)->charptr;
	    if (data == NULL) {
		message_1s (1, MSG_ERROR, _("Unexpected EOF on archive file"));
		return 0;
	    }
	    written = RECORDSIZE;
	    if (written > size)
		written = size;

	    bcopy (data, bp, written);
	    bp += written;
	}
#if 0
	if (hstat.st_size > 1)
	    bp [hstat.st_size - 1] = 0;	/* just to make sure */
#endif
	goto recurse;
    } else {
	struct entry *entry, *pent;
	struct inode *inode;
	long data_position;
	char *p, *q;
	int len;
	int isdir = 0;

	current_file_name = (next_long_name
			     ? next_long_name
			     : strdup (header->header.arch_name));
	len = strlen (current_file_name);
	if (current_file_name[len - 1] == '/') {
	    current_file_name[len - 1] = 0;
	    isdir = 1;
	}

	current_link_name = (next_long_link
			     ? next_long_link
			     : strdup (header->header.arch_linkname));
	len = strlen (current_link_name);
	if (len && current_link_name [len - 1] == '/')
	    current_link_name[len - 1] = 0;

	next_long_link = next_long_name = NULL;

	data_position = get_current_position (archive, tard);
	
	p = strrchr (current_file_name, '/');
	if (p == NULL) {
	    p = current_file_name;
	    q = current_file_name + strlen (current_file_name); /* "" */
	} else {
	    *(p++) = 0;
	    q = current_file_name;
	}
	    
	pent = find_entry (archive->root_entry, q, 1, 0);
	if (pent == NULL) {
	    message_1s (1, MSG_ERROR, _("Inconsistent tar archive"));
	}

	entry = (struct entry *) xmalloc (sizeof (struct entry), "Tar: entry");
	entry->name = strdup (p);
	entry->has_changed = 0;
	entry->header_offset = header_position;
	entry->header_size = data_position - header_position;
	entry->extended_offset = -1;
	entry->extended_size = 0;
	entry->next_in_dir = NULL;
	entry->dir = pent;
	if (pent != NULL) {
	    pent->inode->last_in_subdir->next_in_dir = entry;
	    pent->inode->last_in_subdir = entry;
	}
	free (current_file_name);

	if (header->header.linkflag == LF_LINK) {
	    pent = find_entry (archive->root_entry, current_link_name, 0, 0);
	    if (pent == NULL) {
	        message_1s (1, MSG_ERROR, _("Inconsistent tar archive"));
	    } else {
		entry->inode = pent->inode;
		pent->inode->nlink++;
		free (current_link_name);
		if (header->header.isextended) {
	            entry->extended_offset = data_position;
	            while (get_next_record (archive, tard)->ext_hdr.isextended);
	            data_position = get_current_position (archive, tard);
	            entry->extended_size = data_position - entry->extended_offset;
	        }
	        return 1;
	    }
	}
	inode = (struct inode *) xmalloc (sizeof (struct inode), "Tar: inode");
	entry->inode = inode;
	inode->local_filename = NULL;
	inode->has_changed = 0;
	inode->is_open = 0;
	inode->inode = (archive->__inode_counter)++;
	inode->nlink = 1;
	inode->dev = archive->rdev;
	inode->archive = archive;
	inode->data_offset = data_position;
	inode->mode = from_oct (8, header->header.mode);

        /* Adjust inode->mode because there are tar-files with
         * linkflag==LF_SYMLINK and S_ISLNK(mod)==0. I don't 
         * know about the other modes but I think I cause no new
         * problem when I adjust them, too. -- Norbert.
         */
        if (header->header.linkflag == LF_DIR) {
            inode->mode |= S_IFDIR;
        } else if (header->header.linkflag == LF_SYMLINK) {
            inode->mode |= S_IFLNK;
        } else if (header->header.linkflag == LF_CHR) {
            inode->mode |= S_IFCHR;
        } else if (header->header.linkflag == LF_BLK) {
            inode->mode |= S_IFBLK;
        } else if (header->header.linkflag == LF_FIFO) {
            inode->mode |= S_IFIFO;
        } else 
	    inode->mode |= S_IFREG;

	inode->rdev = 0;
	if (!strcmp (header->header.magic, TMAGIC)) {
	    inode->uid = *header->header.uname ? finduid (header->header.uname) :
	                                         from_oct (8, header->header.uid);
	    inode->gid = *header->header.gname ? findgid (header->header.gname) :
	                                         from_oct (8, header->header.gid);
	    switch (header->header.linkflag) {
	        case LF_BLK:
	        case LF_CHR:
	            inode->rdev = (from_oct (8, header->header.devmajor) << 8) |
	                           from_oct (8, header->header.devminor);
	    }
	    inode->std = 1;
	} else { /* Old Unix tar */
	    inode->uid = from_oct (8, header->header.uid);
	    inode->gid = from_oct (8, header->header.gid);
	    inode->std = 0;
	}
	inode->size = hstat.st_size;
	inode->mtime = from_oct (1 + 12, header->header.mtime);
	inode->atime = from_oct (1 + 12, header->header.atime);
	inode->ctime = from_oct (1 + 12, header->header.ctime);
	inode->linkflag = header->header.linkflag;
	if (*current_link_name) {
	    inode->linkname = current_link_name;
	} else {
	    free (current_link_name);
	    inode->linkname = NULL;
	}
	if (inode->linkflag == LF_DIR || isdir) {
	    inode->mode |= S_IFDIR;
	    make_dot_doubledot (entry);
	}

	if (header->header.isextended) {
	    entry->extended_offset = data_position;
	    while (get_next_record (archive, tard)->ext_hdr.isextended);
	    inode->data_offset = get_current_position (archive, tard);
	    entry->extended_size = inode->data_offset - entry->extended_offset;
	}
	return 1;
    }
}

/*
 * Main loop for reading an archive.
 * Returns 0 on success, -1 on error.
 */
static int read_tar_archive (char *name, struct archive **pparc)
{
    int status = 3;		/* Initial status at start of archive */
    int prev_status;
    int tard;
    struct archive *archive;

    if ((tard = open_tar_archive (name, &archive)) == -1) {	/* Open for reading */
        message_2s (1, MSG_ERROR, _("Couldn't open tar archive\n%s"), name);
	return -1;
    }

    for (;;) {
	prev_status = status;
	status = read_header (archive, tard);
	switch (status) {

	case 1:		/* Valid header */
		skip_n_records (archive, tard, (hstat.st_size + RECORDSIZE - 1) / RECORDSIZE);
		continue;
	                /*
			 * If the previous header was good, tell them
			 * that we are skipping bad ones.
			 */
	case 0:		/* Invalid header */
	    switch (prev_status) {
	    case 3:		/* Error on first record */
		message_2s (1, MSG_ERROR, _("Hmm,...\n%s\ndoesn't look like a tar archive."), name);
		/* FALL THRU */
	    case 2:		/* Error after record of zeroes */
	    case 1:		/* Error after header rec */
#if 0
		message_1s (0, " Warning ", "Skipping to next file header...");
#endif
	    case 0:		/* Error after error */
		return -1;
	    }

	case 2:		/* Record of zeroes */
	    status = prev_status;	/* If error after 0's */
	    /* FALL THRU */
	case EOF:		/* End of archive */
	    break;
	}
	break;
    };

    *pparc = archive;
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
    char *local, *archive_name;
    int result = -1;
    struct archive *parc;
    struct vfs_stamping *parent;
    vfs *v;
    struct stat stat_buf;
    
    archive_name = inname;
    vfs_split( inname, &local, NULL );
    if (!local)
        local = "";

    mc_stat (archive_name, &stat_buf);

    for (parc = first_archive; parc != NULL; parc = parc->next)
        if (!strcmp (parc->name, archive_name)) {
	    if (vfs_uid && (!(stat_buf.st_mode & 0004)))
		if ((stat_buf.st_gid != vfs_gid) || !(stat_buf.st_mode & 0040))
		    if ((stat_buf.st_uid != vfs_uid) || !(stat_buf.st_mode & 0400))
			return NULL;
/* Has the cached archive been changed on the disk? */
	    if (parc->tarstat.st_mtime < stat_buf.st_mtime) { /* Yes, reload! */
		(*vfs_tarfs_ops.free) ((vfsid) parc);
		vfs_rmstamp (&vfs_tarfs_ops, (vfsid) parc, 0);
		break;
	    }
		    /* Hasn't been modified, give it a new timeout */
	    vfs_stamp (&vfs_tarfs_ops, (vfsid) parc);
	    goto return_success;
	}
    if (do_not_open)
        result = -1;
    else
        result = read_tar_archive (archive_name, &parc);
    if (result == -1) ERRNOR (EIO, NULL);
    v = vfs_type (archive_name);
    if (v == &vfs_local_ops) {
	parent = NULL;
    } else {
	parent = xmalloc (sizeof (struct vfs_stamping), "vfs stamping");
	parent->v = v;
	parent->next = 0;
	parent->id = (*v->getid) (v, archive_name, &(parent->parent));
    }
    vfs_add_noncurrent_stamps (&vfs_tarfs_ops, (vfsid) parc, parent);
    vfs_rm_parents (parent);
return_success:
    *archive = parc;
    return local;
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
		 "Tar: symlink looping protection");
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
    long pos;
    long begin;
    long end;
    struct entry *entry;
};

static struct entry *find_entry (struct entry *dir, char *name, int make_dirs, int make_file);

static void *tar_open (vfs *me, char *file, int flags, int mode)
{
    struct pseudofile *tar_info;
    struct archive *archive;
    char *q;
    struct entry *entry;

    if ((q = get_path_mangle (file, &archive, 0, 0)) == NULL)
	return NULL;
    entry = find_entry (archive->root_entry, q, 0, 0);
    if (entry == NULL)
    	return NULL;
    if ((entry = my_resolve_symlinks (entry)) == NULL)
	return NULL;
    if (S_ISDIR (entry->inode->mode)) ERRNOR (EISDIR, NULL);
    if ((flags & O_ACCMODE) != O_RDONLY) ERRNOR (EROFS, NULL);
    
    tar_info = (struct pseudofile *) xmalloc (sizeof (struct pseudofile), "Tar: tar_open");
    tar_info->archive = archive;
    tar_info->pos = 0;
    tar_info->begin = entry->inode->data_offset;
    tar_info->end = tar_info->begin + entry->inode->size;
    tar_info->entry = entry;
    entry->inode->is_open++;

     /* i.e. we had no open files and now we have one */
    vfs_rmstamp (&vfs_tarfs_ops, (vfsid) archive, 1);
    archive->fd_usage++;
    return tar_info;
}

static int tar_read (void *data, char *buffer, int count)
{
    struct pseudofile *file = (struct pseudofile *)data;

    if (file->archive->is_gzipped != targz_growing && 
        mc_lseek (file->archive->fd, file->begin + file->pos, SEEK_SET) != 
        file->begin + file->pos) ERRNOR (EIO, -1);

    
    if (count > file->end - file->begin - file->pos)
    	count = file->end - file->begin - file->pos;
    if (file->archive->is_gzipped == targz_growing) {
        char *p = buffer;
        int cnt = count;
        int i = file->begin + file->pos, j;
        
        if (i % RECORDSIZE) {
            j = RECORDSIZE - (i % RECORDSIZE);
            if (cnt < j)
            	j = cnt;
            bcopy (((char *) find_current_record (file->archive, i / RECORDSIZE * RECORDSIZE)) + (i % RECORDSIZE),
                   p, j);
            cnt -= j;
            p += j;
            i += j;
        }
        while (cnt) {
            if (cnt > RECORDSIZE)
            	j = RECORDSIZE;
            else
            	j = cnt;
            bcopy ((char *) find_current_record (file->archive, i),
                   p, j);
            i += j;
            p += j;
            cnt -= j;
        }
    }
    else if ((count = mc_read (file->archive->fd, buffer, count)) == -1) ERRNOR (errno, -1);
    file->pos += count;
    return count;
}

static int tar_close (void *data)
{
    struct pseudofile *file;

    file = (struct pseudofile *)data;
    
    file->archive->fd_usage--;
    if (!file->archive->fd_usage) {
        struct vfs_stamping *parent;
        vfs *v;
        
	v = vfs_type (file->archive->name);
	if (v == &vfs_local_ops) {
	    parent = NULL;
	} else {
	    parent = xmalloc (sizeof (struct vfs_stamping), "vfs stamping");
	    parent->v = v;
	    parent->next = 0;
	    parent->id = (*v->getid) (v, file->archive->name, &(parent->parent));
	}
        vfs_add_noncurrent_stamps (&vfs_tarfs_ops, (vfsid) (file->archive), parent);
	vfs_rm_parents (parent);
    }
    (file->entry->inode->is_open)--;
	
    free (data);
    return 0;
}

#include "shared_tar_ext.c"

static int tar_chmod (vfs *me, char *path, int mode)
{	/* Fixme: are you sure? IMO this is guaranteed to fail */
    return chmod (path, mode);
}

static int tar_chown (vfs *me, char *path, int owner, int group)
{	/* Fixme: are you sure? IMO this is guaranteed to fail */
    return chown (path, owner, group);
}

static int tar_chdir (vfs *me, char *path)
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

static int tar_lseek (void *data, off_t offset, int whence)
{
    struct pseudofile *file = (struct pseudofile *) data;

    switch (whence) {
    	case SEEK_CUR:
    	    offset += file->pos; break;
    	case SEEK_END:
    	    offset += file->end - file->begin; break;
    }
    if (offset < 0)
    	file->pos = 0;
    else if (offset < file->end - file->begin)
    	file->pos = offset;
    else
    	file->pos = file->end;
    return file->pos;
}

static vfsid tar_getid (vfs *me, char *path, struct vfs_stamping **parent)
{
    struct archive *archive;
    vfs *v;
    char *p;
    vfsid id;
    struct vfs_stamping *par;

    *parent = NULL;
    if (!(p = get_path (path, &archive, 0, 1)))
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

static int tar_nothingisopen (vfsid id)
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
        if (e->inode->linkname != NULL)
            free (e->inode->linkname);
        if (e->inode->local_filename != NULL) {
            unlink (e->inode->local_filename);
            free (e->inode->local_filename);
        }
        free (e->inode);
    }
    if (e->next_in_dir != NULL)
        free_entry (e->next_in_dir);
    free (e->name);
    free (e);
}

static void tar_free (vfsid id)
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

static char *tar_getlocalcopy (vfs *me, char *path)
{
    struct archive *archive;
    char *p, *q;
    struct entry *entry;
    char buf[MC_MAXPATHLEN];

    strcpy( buf, path );
    if ((q = get_path_mangle (path, &archive, 1, 0)) == NULL)
	return NULL;
    entry = find_entry (archive->root_entry, q, 0, 0);
    if (entry == NULL)
    	return NULL;
    if ((entry = my_resolve_symlinks (entry)) == NULL)
	return NULL;

    if (entry->inode->local_filename != NULL)
        return entry->inode->local_filename;
    p = mc_def_getlocalcopy (me, buf);
    if (p != NULL) {
        entry->inode->local_filename = p;
    }
    return p;
}

static void tar_ungetlocalcopy (vfs *me, char *path, char *local, int has_changed)
{
/* We do just nothing. (We are read only and do not need to free local,
   since it will be freed when tar archive will be freed */
}

vfs vfs_tarfs_ops =
{
    NULL,	/* This is place of next pointer */
    "TApe aRchiver decompressor",
    0,		/* flags */
    "utar",	/* prefix */
    NULL,	/* data */
    0,		/* errno */
    NULL,
    NULL,
    tarfs_fill_names,
    NULL,

    tar_open,
    tar_close,
    tar_read,
    NULL,

    s_opendir,
    s_readdir,
    s_closedir,
    s_telldir,
    s_seekdir,

    s_stat,
    s_lstat,
    s_fstat,

    tar_chmod,
    tar_chown,
    NULL,

    s_readlink,
    NULL,
    NULL,
    NULL,

    NULL,
    tar_chdir,
    s_errno,
    tar_lseek,
    NULL,
    
    tar_getid,
    tar_nothingisopen,
    tar_free,
    
    tar_getlocalcopy,
    tar_ungetlocalcopy,
    
    NULL,		/* mkdir */
    NULL,
    NULL,
    NULL

MMAPNULL
};
