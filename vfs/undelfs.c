/* UnDel File System: Midnight Commander file system.

   This file system is intended to be used together with the
   ext2fs library to recover files from ext2fs file systems.

   Parts of this program were taken from the lsdel.c and dump.c files
   written by Ted Ts'o (tytso@mit.edu) for the ext2fs package.
   
   Copyright (C) 1995, 1997 the Free Software Foundation
   Written by: 1995 Miguel de Icaza.
               1997 Norbert Warmuth.
   
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

/* Assumptions:
 *
 * 1. We don't handle directories (thus undelfs_get_path is easy to write).
 * 2. Files are on the local file system (we do not support vfs files
 *    because we would have to provide an io_manager for the ext2fs tools,
 *    and I don't think it would be too useful to undelete files 
 */
 
#include <config.h>
#include <errno.h>
#include "../src/fs.h"
#include "../src/mad.h"
#include "../src/util.h"

#include "../src/mem.h"
#include "vfs.h"

#include <stdio.h>
#include <malloc.h>
#include <fcntl.h>
#include <stdlib.h>
/* asm/types.h defines its own umode_t :-( */
#undef umode_t
#include <linux/ext2_fs.h>
#include <ext2fs/ext2fs.h>
#include <ctype.h>

void print_vfs_message(char *, ...);

struct deleted_info {
    ino_t   ino;
    unsigned short mode;
    unsigned short uid;
    unsigned short gid;
    unsigned long size;
    time_t  dtime;
    int     num_blocks;
    int     free_blocks;
};

struct lsdel_struct {
    ino_t                   inode;
    int                     num_blocks;
    int                     free_blocks;
    int                     bad_blocks;
};

/* We only allow one opened ext2fs */
static char *ext2_fname;
static ext2_filsys fs = NULL;
static struct lsdel_struct     lsd;
static struct deleted_info     *delarray;
static int num_delarray, max_delarray;
static char *block_buf;
static char *undelfserr = " undelfs: error ";
static int readdir_ptr;
static int undelfs_usage;

/* To generate the . and .. entries use -2 */
#define READDIR_PTR_INIT 0

static void
undelfs_shutdown (void)
{
    if (fs)
	ext2fs_close (fs);
    fs = 0;
    if (ext2_fname)
	free (ext2_fname);
    ext2_fname = 0;
    if (delarray)
        free (delarray);
    delarray = 0;
    if (block_buf)
	free (block_buf);
    block_buf = 0;
}

static void
undelfs_get_path (char *dirname, char **ext2_fname, char **file)
{
    char *p;

    /* To look like filesystem, we have virtual directories
       /#undel:XXX, which have no subdirectories. XXX is replaced with
       hda5, sdb8 etc, which is assumed to live under /dev. 
                                                    -- pavel@ucw.cz */

    *ext2_fname = 0;
    
    if (strncmp (dirname, "/#undel:", 8))
	return;
    else
	dirname += 8;

    /* Since we don't allow subdirectories, it's easy to get a filename,
     * just scan backwards for a slash */
    if (*dirname == 0)
	return;
    
    p = dirname + strlen (dirname);
#if 0
    /* Strip trailing ./ 
     */
    if (p - dirname > 2 && *(p-1) == '/' && *(p-2) == '.')
	*(p = p-2) = 0;
#endif
    
    while (p > dirname){
	if (*p == '/'){
	    *file = strdup (p+1);
	    *p = 0;
	    *ext2_fname = copy_strings ("/dev/", dirname, NULL);
	    *p = '/';
	    return;
	}
	p--;
    }
    *file = strdup ("");
    *ext2_fname = copy_strings ("/dev/", dirname, NULL);
    return;
}

static int
lsdel_proc(ext2_filsys fs, blk_t *block_nr, int blockcnt, void *private)
{
    struct lsdel_struct *lsd = (struct lsdel_struct *) private;
    
    lsd->num_blocks++;
    
    if (*block_nr < fs->super->s_first_data_block ||
	*block_nr >= fs->super->s_blocks_count) {
	lsd->bad_blocks++;
	return BLOCK_ABORT;
    }
    
    if (!ext2fs_test_block_bitmap(fs->block_map,*block_nr))
	lsd->free_blocks++;
    
    return 0;
}

/* We don't use xmalloc, since we want to recover and not abort the program
 * if we don't have enough memory
 */
static int
undelfs_loaddel (void)
{
    int retval, count;
    ino_t ino;
    struct ext2_inode inode;
    ext2_inode_scan   scan;
    
    max_delarray = 100;
    num_delarray = 0;
    delarray = malloc(max_delarray * sizeof(struct deleted_info));
    if (!delarray) {
	message_1s (1, undelfserr, " not enough memory ");
	return 0;
    }
    block_buf = malloc(fs->blocksize * 3);
    if (!block_buf) {
	message_1s (1, undelfserr, " while allocating block buffer ");
	goto free_delarray;
    }
    if ((retval = ext2fs_open_inode_scan(fs, 0, &scan))){
	message_1s1d (1, undelfserr, " open_inode_scan: %d ", retval);
	goto free_block_buf;
    }
    if ((retval = ext2fs_get_next_inode(scan, &ino, &inode))){
	message_1s1d (1, undelfserr, " while starting inode scan %d ", retval);
	goto error_out;
    }

    count = 0;
    while (ino) {
	if ((count++ % 1024) == 0) 
	    print_vfs_message ("undelfs: loading deleted files information %d inodes", count);
	if (inode.i_dtime == 0)
	    goto next;

	if (S_ISDIR(inode.i_mode))
	    goto next;

	lsd.inode = ino;
	lsd.num_blocks = 0;
	lsd.free_blocks = 0;
	lsd.bad_blocks = 0;

	retval = ext2fs_block_iterate(fs, ino, 0, block_buf,
				      lsdel_proc, &lsd);
	if (retval) {
	    message_1s1d (1, undelfserr, " while calling ext2_block_iterate %d ", retval);
	    goto next;
	}
	if (lsd.free_blocks && !lsd.bad_blocks) {
	    if (num_delarray >= max_delarray) {
		max_delarray += 50;
		delarray = realloc(delarray,
				   max_delarray * sizeof(struct deleted_info));
		if (!delarray) {
		    message_1s (1, undelfserr, " no more memory while reallocating array ");
		    goto error_out;
		}
	    }
	    
	    delarray[num_delarray].ino = ino;
	    delarray[num_delarray].mode = inode.i_mode;
	    delarray[num_delarray].uid = inode.i_uid;
	    delarray[num_delarray].gid = inode.i_gid;
	    delarray[num_delarray].size = inode.i_size;
	    delarray[num_delarray].dtime = inode.i_dtime;
	    delarray[num_delarray].num_blocks = lsd.num_blocks;
	    delarray[num_delarray].free_blocks = lsd.free_blocks;
	    num_delarray++;
	}

    next:
	retval = ext2fs_get_next_inode(scan, &ino, &inode);
	if (retval) {
	    message_1s1d(1, undelfserr, " while doing inode scan %d ", retval);
	    goto error_out;
	}
    }
    readdir_ptr = READDIR_PTR_INIT;
    ext2fs_close_inode_scan (scan);
    return 1;
    
error_out:    
    ext2fs_close_inode_scan (scan);
free_block_buf:
    free (block_buf);
    block_buf = 0;
free_delarray:
    free (delarray);
    delarray = 0;
    return 0;
}

void com_err (const char *str, long err_code, const char *s2, ...)
{
    char *cptr;

    cptr = xmalloc (strlen (s2) + strlen (str) + 60, "com_err");
    sprintf (cptr, " %s (%s: %ld) ", s2, str, err_code);
    message_1s (1, " Ext2lib error ", cptr);
    free (cptr);
}

static void *
undelfs_opendir (vfs *me, char *dirname)
{
    char *file, *f;
    
    undelfs_get_path (dirname, &file, &f);
    if (!file)
	return 0;

    /* We don't use the file name */
    free (f);
    
    if (!ext2_fname || strcmp (ext2_fname, file)){
	undelfs_shutdown ();
	ext2_fname = file;
    } else {
	/* To avoid expensive re-scannings */
	readdir_ptr = READDIR_PTR_INIT;
	free (file);
	return fs;
    }

    if (ext2fs_open (ext2_fname, 0, 0, 0, unix_io_manager, &fs)){
	message_2s (1, undelfserr, " Could not open file %s ", ext2_fname);
	return 0;
    }
    print_vfs_message ("undelfs: reading inode bitmap...");
    if (ext2fs_read_inode_bitmap (fs)){
	message_2s (1, undelfserr,
		 " Could not load inode bitmap from: \n %s \n", ext2_fname);
	goto quit_opendir;
    }
    print_vfs_message ("undelfs: reading block bitmap...");
    if (ext2fs_read_block_bitmap (fs)){
	message_2s (1, undelfserr,
		 " Could not load block bitmap from: \n %s \n", ext2_fname);
	goto quit_opendir;
    }
    /* Now load the deleted information */
    if (!undelfs_loaddel ())
	goto quit_opendir;
    print_vfs_message ("undelfs: done.");
    return fs;
quit_opendir:
    print_vfs_message ("undelfs: failure");
    ext2fs_close (fs);
    fs = NULL;
    return 0;
}

/* Explanation:
 * On some operating systems (Slowaris 2 for example)
 * the d_name member is just a char long (Nice trick that break everything,
 * so we need to set up some space for the filename.
 */
static struct {
    struct dirent dent;
#ifdef NEED_EXTRA_DIRENT_BUFFER
    char extra_buffer [MC_MAXPATHLEN];
#endif
} undelfs_readdir_data;

static void *
undelfs_readdir (void *vfs_info)
{
    char *dirent_dest;
    
    if (vfs_info != fs){
	message_1s (1, " delfs: internal error ",
		 " vfs_info is not fs! ");
	return NULL;
    }
    if (readdir_ptr == num_delarray)
	return NULL;
    dirent_dest = &(undelfs_readdir_data.dent.d_name [0]);
    if (readdir_ptr < 0)
	sprintf (dirent_dest, "%s", readdir_ptr == -2 ? "." : "..");
    else
	sprintf (dirent_dest, "%ld:%d",
		 (long)delarray [readdir_ptr].ino,
		 delarray [readdir_ptr].num_blocks);
    readdir_ptr++;
    
#ifndef DIRENT_LENGTH_COMPUTED
    undelfs_readdir_data.dent.d_namlen = strlen (undelfs_readdir_data.dent.d_name);
#endif
    return &undelfs_readdir_data;
}

static int
undelfs_closedir (void *vfs_info)
{
    return 0;
}

typedef struct {
    int  f_index;			/* file index into delarray */
    char *buf;
    int  error_code;		/*  */
    int  pos;			/* file position */
    int  current;		/* used to determine current position in itereate */
    int  finished;
    ino_t inode;
    int  bytes_read;
    long size;
    
    /* Used by undelfs_read: */
    char *dest_buffer;		/* destination buffer */
    int  count;			/* bytes to read */
} undelfs_file;

/* We do not support lseek */
static void *
undelfs_open (vfs *me, char *fname, int flags, int mode)
{
    char *file, *f;
    ino_t  inode, i;
    undelfs_file *p;
    
    /* Only allow reads on this file system */
    undelfs_get_path (fname, &file, &f);
    if (!file)
	return 0;
    
    if (!ext2_fname || strcmp (ext2_fname, file)){
	message_1s (1, undelfserr, " You have to chdir to extract files first ");
	free (file);
	free (f);
	return 0;
    }
    inode = atol (f);

    /* Search the file into delarray */
    for (i = 0; i < num_delarray; i++){
	if (inode != delarray [i].ino)
	    continue;

	/* Found: setup all the structures needed by read */
	p = (void *) malloc (sizeof (undelfs_file));
	if (!p){
	    free (file);
	    free (f);
	    return 0;
	}
	p->buf = malloc(fs->blocksize);
	if (!p->buf){
	    free (p);
	    free (file);
	    free (f);
	    return 0;
	}
	p->inode = inode;
	p->finished = 0;
	p->f_index = i;
	p->error_code = 0;
	p->pos = 0;
	p->size = delarray [i].size;
    }
    free (file);
    free (f);
    undelfs_usage++;
    return p;
}

static 	int
undelfs_close (void *vfs_info)
{
    undelfs_file *p = vfs_info;
    free (p->buf);
    free (p);
    undelfs_usage--;
    return 0;
}

static int
dump_read(ext2_filsys fs, blk_t *blocknr, int blockcnt, void *private)
{
    int     copy_count;
    undelfs_file *p = (undelfs_file *) private;
    
    if (blockcnt < 0)
	return 0;
    
    if (*blocknr) {
	p->error_code = io_channel_read_blk(fs->io, *blocknr,
					   1, p->buf);
	if (p->error_code)
	    return BLOCK_ABORT;
    } else
	memset(p->buf, 0, fs->blocksize);

    if (p->pos + p->count < p->current){
	p->finished = 1;
	return BLOCK_ABORT;
    }
    if (p->pos > p->current + fs->blocksize){
	p->current += fs->blocksize;
	return 0;		/* we have not arrived yet */
    }

    /* Now, we know we have to extract some data */
    if (p->pos >= p->current){

	/* First case: starting pointer inside this block */
	if (p->pos + p->count <= p->current + fs->blocksize){
	    /* Fully contained */
	    copy_count = p->count;
	    p->finished = p->count;
	} else {
	    /* Still some more data */
	    copy_count = fs->blocksize-(p->pos-p->current);
	}
	memcpy (p->dest_buffer, p->buf + (p->pos-p->current), copy_count);
    } else {
	/* Second case: we already have passed p->pos */
	if (p->pos+p->count < p->current+fs->blocksize){
	    copy_count = (p->pos + p->count) - p->current;
	    p->finished = p->count;
	} else {
	    copy_count = fs->blocksize;
	}
	memcpy (p->dest_buffer, p->buf, copy_count);
    }
    p->dest_buffer += copy_count;
    p->current += fs->blocksize;
    if (p->finished){
	return BLOCK_ABORT;
    }
    return 0;
}

static int
undelfs_read (void *vfs_info, char *buffer, int count)
{
    undelfs_file *p = vfs_info;
    int retval;

    p->dest_buffer = buffer;
    p->current     = 0;
    p->finished    = 0;
    p->count       = count;

    if (p->pos + p->count > p->size){
	p->count = p->size - p->pos;
    }
    retval = ext2fs_block_iterate(fs, p->inode, 0, NULL,
				  dump_read, p);
    if (retval){
	message_1s (1, undelfserr, " while iterating over blocks ");
	return -1;
    }
    if (p->error_code && !p->finished)
	return 0;
    p->pos = p->pos + (p->dest_buffer - buffer);
    return p->dest_buffer - buffer;
}

static long
undelfs_getindex (char *path)
{
    ino_t inode = atol (path);
    int i;

    for (i = 0; i < num_delarray; i++){
	if (delarray [i].ino == inode)
	    return i;
    }
    return -1;
}

static int
do_stat (int inode_index, struct stat *buf)
{
    buf->st_dev   = 0;
    buf->st_ino   = delarray [inode_index].ino;
    buf->st_mode  = delarray [inode_index].mode;
    buf->st_nlink = 1;
    buf->st_uid   = delarray [inode_index].uid;
    buf->st_gid   = delarray [inode_index].gid;
    buf->st_size  = delarray [inode_index].size;
    buf->st_atime = delarray [inode_index].dtime;
    buf->st_ctime = delarray [inode_index].dtime;
    buf->st_mtime = delarray [inode_index].dtime;
    return 0;
}

static int
undelfs_lstat(vfs *me, char *path, struct stat *buf)
{
    int inode_index;
    char *file, *f;
    
    undelfs_get_path (path, &file, &f);
    if (!file)
	return 0;

    /* When called from save_cwd_stats we get an incorrect file and f here:
       e.g. incorrect                         correct
            path = "undel:/dev/sda1"          path="undel:/dev/sda1/401:1"
            file = "/dev"                     file="/dev/sda1"
	    f    = "sda1"                     f   ="401:1"
       If the first char in f is no digit -> return error */
    if (!isdigit (*f)) {
	free (file);
	free (f);
	return -1;
    }
	
    if (!ext2_fname || strcmp (ext2_fname, file)){
	message_1s (1, undelfserr, " You have to chdir to extract files first ");
	free (file);
	free (f);
	return 0;
    }
    inode_index = undelfs_getindex (f);
    free (file);
    free (f);

    if (inode_index == -1)
	return -1;

    return do_stat (inode_index, buf);
}

static int
undelfs_stat(vfs *me, char *path, struct stat *buf)
{
    return undelfs_lstat (me, path, buf);
}


static int
undelfs_fstat (void *vfs_info, struct stat *buf)
{
    undelfs_file *p = vfs_info;
    
    return do_stat (p->f_index, buf);
}

static int
undelfs_chdir(vfs *me, char *path)
{
    char *file, *f;
    int fd;
    
    undelfs_get_path (path, &file, &f);
    if (!file)
	return -1;

    /* We may use access because ext2 file systems are local */
    /* this could be fixed by making an ext2fs io manager to use */
    /* our vfs, but that is left as an excercise for the reader */
    if ((fd = open (file, O_RDONLY)) == -1){
	message_2s (1, undelfserr, " Could not open file: %s ", file);
	free (f);
	free (file);
	return -1;
    }
    close (fd);
    free (f);
    free (file);
    return 0;
}

/* this has to stay here for now: vfs layer does not know how to emulate it */
static int
undelfs_lseek(void *vfs_info, off_t offset, int whence)
{
    return -1;
}

static vfsid
undelfs_getid(vfs *me, char *path, struct vfs_stamping **parent)
{
    char *ext2_fname, *file;

    /* We run only on the local fs */
    *parent = NULL;
    undelfs_get_path (path, &ext2_fname, &file);

    if (!ext2_fname)
        return (vfsid) -1;
    free (ext2_fname);
    free (file);
    return (vfsid)0;
}

static int
undelfs_nothingisopen(vfsid id)
{
    return !undelfs_usage;
}

static void
undelfs_free(vfsid id)
{
    undelfs_shutdown ();
}

vfs vfs_undelfs_ops = {
    NULL,	/* This is place of next pointer */
    "Undelete filesystem for ext2",
    0,	/* flags */
    "undel:",	/* prefix */
    NULL,	/* data */
    0,		/* errno */
    NULL,
    NULL,
    NULL,	/* fill_names */
    NULL,

    undelfs_open,
    undelfs_close,
    undelfs_read,
    NULL,
    
    undelfs_opendir,
    undelfs_readdir,
    undelfs_closedir,
    NULL,
    NULL,

    undelfs_stat,
    undelfs_lstat,
    undelfs_fstat,

    NULL,
    NULL,
    NULL,

    NULL,	/* readlink */
    NULL,
    NULL,
    NULL,

    NULL,
    undelfs_chdir,
    NULL,
    undelfs_lseek, 
    NULL,
    
    undelfs_getid,
    undelfs_nothingisopen,
    undelfs_free,
    
    NULL,	/* get_local_copy */
    NULL,

    NULL,
    NULL,
    NULL,
    NULL

MMAPNULL
};
