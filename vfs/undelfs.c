/* UnDel File System: Midnight Commander file system.

   This file system is intended to be used together with the
   ext2fs library to recover files from ext2fs file systems.

   Parts of this program were taken from the lsdel.c and dump.c files
   written by Ted Ts'o (tytso@mit.edu) for the ext2fs package.
   
   Copyright (C) 1995, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2007 Free Software Foundation, Inc.
   Written by: 1995 Miguel de Icaza.
               1997 Norbert Warmuth.
	       2000 Pavel Machek
   
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

/* Assumptions:
 *
 * 1. We don't handle directories (thus undelfs_get_path is easy to write).
 * 2. Files are on the local file system (we do not support vfs files
 *    because we would have to provide an io_manager for the ext2fs tools,
 *    and I don't think it would be too useful to undelete files 
 */
 
#include <config.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_EXT2FS_EXT2_FS_H
#include <ext2fs/ext2_fs.h>
#else
/* asm/types.h defines its own umode_t */
#undef umode_t
#include <linux/ext2_fs.h>
#endif

#include <ext2fs/ext2fs.h>
#include <ctype.h>

#include "../src/global.h"
#include "../src/tty.h"		/* enable/disable interrupt key */
#include "../src/wtools.h"	/* message() */
#include "../src/main.h"	/* print_vfs_message */
#include "utilvfs.h"
#include "vfs.h"
#include "vfs-impl.h"

struct deleted_info {
    ext2_ino_t   ino;
    unsigned short mode;
    unsigned short uid;
    unsigned short gid;
    unsigned long size;
    time_t  dtime;
    int     num_blocks;
    int     free_blocks;
};

struct lsdel_struct {
    ext2_ino_t                   inode;
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
static const char *undelfserr = N_(" undelfs: error ");
static int readdir_ptr;
static int undelfs_usage;
static struct vfs_class vfs_undelfs_ops;

/* To generate the . and .. entries use -2 */
#define READDIR_PTR_INIT 0

static void
undelfs_shutdown (void)
{
    if (fs)
	ext2fs_close (fs);
    fs = NULL;
    g_free (ext2_fname);
    ext2_fname = NULL;
    g_free (delarray);
    delarray = NULL;
    g_free (block_buf);
    block_buf = NULL;
}

static void
undelfs_get_path (const char *dirname, char **fsname, char **file)
{
    const char *p;

    /* To look like filesystem, we have virtual directories
       /#undel:XXX, which have no subdirectories. XXX is replaced with
       hda5, sdb8 etc, which is assumed to live under /dev. 
                                                    -- pavel@ucw.cz */

    *fsname = NULL;
    
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
	    char *tmp;

	    *file = g_strdup (p+1);
	    tmp = g_strndup (dirname, p - dirname);
	    *fsname = g_strconcat ("/dev/", tmp, (char *) NULL);
	    g_free (tmp);
	    return;
	}
	p--;
    }
    *file = g_strdup ("");
    *fsname = g_strconcat ("/dev/", dirname, (char *) NULL);
    return;
}

static int
undelfs_lsdel_proc(ext2_filsys fs, blk_t *block_nr, int blockcnt, void *private)
{
    struct lsdel_struct *lsd = (struct lsdel_struct *) private;
    (void) blockcnt;
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

/*
 * Load information about deleted files.
 * Don't abort if there is not enough memory - load as much as we can.
 */
static int
undelfs_loaddel (void)
{
    int retval, count;
    ext2_ino_t ino;
    struct ext2_inode inode;
    ext2_inode_scan scan;

    max_delarray = 100;
    num_delarray = 0;
    delarray = g_try_malloc (sizeof (struct deleted_info) * max_delarray);
    if (!delarray) {
	message (D_ERROR, undelfserr, _(" not enough memory "));
	return 0;
    }
    block_buf = g_try_malloc (fs->blocksize * 3);
    if (!block_buf) {
	message (D_ERROR, undelfserr, _(" while allocating block buffer "));
	goto free_delarray;
    }
    if ((retval = ext2fs_open_inode_scan (fs, 0, &scan))) {
	message (D_ERROR, undelfserr, _(" open_inode_scan: %d "), retval);
	goto free_block_buf;
    }
    if ((retval = ext2fs_get_next_inode (scan, &ino, &inode))) {
	message (D_ERROR, undelfserr, _(" while starting inode scan %d "),
		 retval);
	goto error_out;
    }

    count = 0;
    while (ino) {
	if ((count++ % 1024) == 0)
	    print_vfs_message (_
			       ("undelfs: loading deleted files information %d inodes"),
			       count);
	if (inode.i_dtime == 0)
	    goto next;

	if (S_ISDIR (inode.i_mode))
	    goto next;

	lsd.inode = ino;
	lsd.num_blocks = 0;
	lsd.free_blocks = 0;
	lsd.bad_blocks = 0;

	retval =
	    ext2fs_block_iterate (fs, ino, 0, block_buf,
				  undelfs_lsdel_proc, &lsd);
	if (retval) {
	    message (D_ERROR, undelfserr,
		     _(" while calling ext2_block_iterate %d "), retval);
	    goto next;
	}
	if (lsd.free_blocks && !lsd.bad_blocks) {
	    if (num_delarray >= max_delarray) {
		struct deleted_info *delarray_new =
		    g_try_realloc (delarray,
				   sizeof (struct deleted_info) *
				   (max_delarray + 50));
		if (!delarray_new) {
		    message (D_ERROR, undelfserr,
			     _
			     (" no more memory while reallocating array "));
		    goto error_out;
		}
		delarray = delarray_new;
		max_delarray += 50;
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
	retval = ext2fs_get_next_inode (scan, &ino, &inode);
	if (retval) {
	    message (D_ERROR, undelfserr, _(" while doing inode scan %d "),
		     retval);
	    goto error_out;
	}
    }
    readdir_ptr = READDIR_PTR_INIT;
    ext2fs_close_inode_scan (scan);
    return 1;

  error_out:
    ext2fs_close_inode_scan (scan);
  free_block_buf:
    g_free (block_buf);
    block_buf = NULL;
  free_delarray:
    g_free (delarray);
    delarray = NULL;
    return 0;
}


/*
 * This function overrides com_err() from libcom_err library.
 * It is used in libext2fs to report errors.
 */
void
com_err (const char *whoami, long err_code, const char *fmt, ...)
{
    va_list ap;
    char *str;

    va_start (ap, fmt);
    str = g_strdup_vprintf (fmt, ap);
    va_end (ap);

    message (D_ERROR, _(" Ext2lib error "), " %s (%s: %ld) ", str, whoami,
	     err_code);
    g_free (str);
}

static void *
undelfs_opendir (struct vfs_class *me, const char *dirname)
{
    char *file, *f;
    
    undelfs_get_path (dirname, &file, &f);
    if (!file)
	return 0;

    /* We don't use the file name */
    g_free (f);
    
    if (!ext2_fname || strcmp (ext2_fname, file)){
	undelfs_shutdown ();
	ext2_fname = file;
    } else {
	/* To avoid expensive re-scannings */
	readdir_ptr = READDIR_PTR_INIT;
	g_free (file);
	return fs;
    }

    if (ext2fs_open (ext2_fname, 0, 0, 0, unix_io_manager, &fs)){
	message (D_ERROR, undelfserr, _(" Cannot open file %s "), ext2_fname);
	return 0;
    }
    print_vfs_message (_("undelfs: reading inode bitmap..."));
    if (ext2fs_read_inode_bitmap (fs)){
	message (D_ERROR, undelfserr,
		 _(" Cannot load inode bitmap from: \n %s \n"), ext2_fname);
	goto quit_opendir;
    }
    print_vfs_message (_("undelfs: reading block bitmap..."));
    if (ext2fs_read_block_bitmap (fs)){
	message (D_ERROR, undelfserr,
		 _(" Cannot load block bitmap from: \n %s \n"), ext2_fname);
	goto quit_opendir;
    }
    /* Now load the deleted information */
    if (!undelfs_loaddel ())
	goto quit_opendir;
    print_vfs_message (_("%s: done."), me->name);
    return fs;
quit_opendir:
    print_vfs_message (_("%s: failure"), me->name);
    ext2fs_close (fs);
    fs = NULL;
    return 0;
}


static void *
undelfs_readdir(void *vfs_info)
{
    static union vfs_dirent undelfs_readdir_data;
    static char *const dirent_dest = undelfs_readdir_data.dent.d_name;

    if (vfs_info != fs) {
	message (D_ERROR, undelfserr, _(" vfs_info is not fs! "));
	return NULL;
    }
    if (readdir_ptr == num_delarray)
	return NULL;
    if (readdir_ptr < 0)
	strcpy (dirent_dest, readdir_ptr == -2 ? "." : "..");
    else
	g_snprintf(dirent_dest, MC_MAXPATHLEN, "%ld:%d",
		   (long) delarray[readdir_ptr].ino,
		   delarray[readdir_ptr].num_blocks);
    readdir_ptr++;

    compute_namelen(&undelfs_readdir_data.dent);

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
    ext2_ino_t inode;
    int  bytes_read;
    long size;
    
    /* Used by undelfs_read: */
    char *dest_buffer;		/* destination buffer */
    int  count;			/* bytes to read */
} undelfs_file;

/* We do not support lseek */
static void *
undelfs_open (struct vfs_class *me, const char *fname, int flags, int mode)
{
    char *file, *f;
    ext2_ino_t inode, i;
    undelfs_file *p = NULL;

    /* Only allow reads on this file system */
    undelfs_get_path (fname, &file, &f);
    if (!file)
	return 0;

    if (!ext2_fname || strcmp (ext2_fname, file)) {
	message (D_ERROR, undelfserr,
		 _(" You have to chdir to extract files first "));
	g_free (file);
	g_free (f);
	return 0;
    }
    inode = atol (f);

    /* Search the file into delarray */
    for (i = 0; i < num_delarray; i++) {
	if (inode != delarray[i].ino)
	    continue;

	/* Found: setup all the structures needed by read */
	p = (undelfs_file *) g_try_malloc (((gsize) sizeof (undelfs_file)));
	if (!p) {
	    g_free (file);
	    g_free (f);
	    return 0;
	}
	p->buf = g_try_malloc (fs->blocksize);
	if (!p->buf) {
	    g_free (p);
	    g_free (file);
	    g_free (f);
	    return 0;
	}
	p->inode = inode;
	p->finished = 0;
	p->f_index = i;
	p->error_code = 0;
	p->pos = 0;
	p->size = delarray[i].size;
    }
    g_free (file);
    g_free (f);
    undelfs_usage++;
    return p;
}

static 	int
undelfs_close (void *vfs_info)
{
    undelfs_file *p = vfs_info;
    g_free (p->buf);
    g_free (p);
    undelfs_usage--;
    return 0;
}

static int
undelfs_dump_read(ext2_filsys fs, blk_t *blocknr, int blockcnt, void *private)
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

static ssize_t
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
				  undelfs_dump_read, p);
    if (retval){
	message (D_ERROR, undelfserr, _(" while iterating over blocks "));
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
    ext2_ino_t inode = atol (path);
    int i;

    for (i = 0; i < num_delarray; i++){
	if (delarray [i].ino == inode)
	    return i;
    }
    return -1;
}

static int
undelfs_stat_int (int inode_index, struct stat *buf)
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
undelfs_lstat (struct vfs_class *me, const char *path, struct stat *buf)
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
	g_free (file);
	g_free (f);
	return -1;
    }
	
    if (!ext2_fname || strcmp (ext2_fname, file)){
	message (D_ERROR, undelfserr, _(" You have to chdir to extract files first "));
	g_free (file);
	g_free (f);
	return 0;
    }
    inode_index = undelfs_getindex (f);
    g_free (file);
    g_free (f);

    if (inode_index == -1)
	return -1;

    return undelfs_stat_int (inode_index, buf);
}

#define undelfs_stat undelfs_lstat

static int
undelfs_fstat (void *vfs_info, struct stat *buf)
{
    undelfs_file *p = vfs_info;
    
    return undelfs_stat_int (p->f_index, buf);
}

static int
undelfs_chdir(struct vfs_class *me, const char *path)
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
	message (D_ERROR, undelfserr, _(" Cannot open file %s "), file);
	g_free (f);
	g_free (file);
	return -1;
    }
    close (fd);
    g_free (f);
    g_free (file);
    return 0;
}

/* this has to stay here for now: vfs layer does not know how to emulate it */
static off_t
undelfs_lseek(void *vfs_info, off_t offset, int whence)
{
    return -1;
}

static vfsid
undelfs_getid (struct vfs_class *me, const char *path)
{
    char *fname, *fsname;

    undelfs_get_path (path, &fsname, &fname);

    if (!fsname)
	return NULL;
    g_free (fname);
    g_free (fsname);
    return (vfsid) fs;
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

#ifdef	ENABLE_NLS
static int
undelfs_init(struct vfs_class *me) {
    undelfserr = _(undelfserr);
    return 1;
}
#else
#define undelfs_init	NULL
#endif

void
init_undelfs (void)
{
    vfs_undelfs_ops.name = "undelfs";
    vfs_undelfs_ops.prefix = "undel:";
    vfs_undelfs_ops.init = undelfs_init;
    vfs_undelfs_ops.open = undelfs_open;
    vfs_undelfs_ops.close = undelfs_close;
    vfs_undelfs_ops.read = undelfs_read;
    vfs_undelfs_ops.opendir = undelfs_opendir;
    vfs_undelfs_ops.readdir = undelfs_readdir;
    vfs_undelfs_ops.closedir = undelfs_closedir;
    vfs_undelfs_ops.stat = undelfs_stat;
    vfs_undelfs_ops.lstat = undelfs_lstat;
    vfs_undelfs_ops.fstat = undelfs_fstat;
    vfs_undelfs_ops.chdir = undelfs_chdir;
    vfs_undelfs_ops.lseek = undelfs_lseek;
    vfs_undelfs_ops.getid = undelfs_getid;
    vfs_undelfs_ops.nothingisopen = undelfs_nothingisopen;
    vfs_undelfs_ops.free = undelfs_free;
    vfs_register_class (&vfs_undelfs_ops);
}
