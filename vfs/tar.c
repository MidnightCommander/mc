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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Namespace: vfs_tarfs_ops */

#include <config.h>
#include <errno.h>

#include "utilvfs.h"
#include "xdirentry.h"

#include "../src/dialog.h"	/* For MSG_ERROR */
#include "tar.h"
#include "names.h"

static struct vfs_class vfs_tarfs_ops;

#define	isodigit(c)	( ((c) >= '0') && ((c) <= '7') )
/*
 * Quick and dirty octal conversion.
 *
 * Result is -1 if the field is invalid (all blank, or nonoctal).
 */
static long from_oct (int digs, char *where)
{
    register long value;

    while (isspace ((unsigned char) *where)) {	/* Skip spaces */
	where++;
	if (--digs <= 0)
	    return -1;		/* All blank field */
    }
    value = 0;
    while (digs > 0 && isodigit (*where)) {	/* Scan till nonoctal */
	value = (value << 3) | (*where++ - '0');
	--digs;
    }

    if (digs > 0 && *where && !isspace ((unsigned char) *where))
	return -1;		/* Ended on non-space/nul */

    return value;
}

static struct stat hstat;		/* Stat struct corresponding */

static void tar_free_archive (struct vfs_class *me, struct vfs_s_super *archive)
{
    if (archive->u.arch.fd != -1)
	mc_close(archive->u.arch.fd);
}

/* As we open one archive at a time, it is safe to have this static */
static int current_tar_position = 0;

/* Returns fd of the open tar file */
static int tar_open_archive (struct vfs_class *me, char *name, struct vfs_s_super *archive)
{
    int result, type;
    mode_t mode;
    struct vfs_s_inode *root;
    
    result = mc_open (name, O_RDONLY);
    if (result == -1) {
        message_2s (1, MSG_ERROR, _("Cannot open tar archive\n%s"), name);
	ERRNOR (ENOENT, -1);
    }
    
    archive->name = g_strdup (name);
    mc_stat (name, &(archive->u.arch.st));
    archive->u.arch.fd = -1;

    /* Find out the method to handle this tar file */
    type = get_compression_type (result);
    mc_lseek (result, 0, SEEK_SET);
    if (type != COMPRESSION_NONE) {
	char *s;
	mc_close( result );
	s = g_strconcat ( archive->name, decompress_extension (type), NULL );
	result = mc_open (s, O_RDONLY);
	if (result == -1) 
	    message_2s (1, MSG_ERROR, _("Cannot open tar archive\n%s"), s);
	g_free(s);
	if (result == -1)
	    ERRNOR (ENOENT, -1);
    }
   
    archive->u.arch.fd = result;
    mode = archive->u.arch.st.st_mode & 07777;
    if (mode & 0400) mode |= 0100;
    if (mode & 0040) mode |= 0010;
    if (mode & 0004) mode |= 0001;
    mode |= S_IFDIR;

    root = vfs_s_new_inode (me, archive, &archive->u.arch.st);
    root->st.st_mode = mode;
    root->data_offset = -1;
    root->st.st_nlink++;
    root->st.st_dev = MEDATA->rdev++;

    vfs_s_add_dots (me, root, NULL);
    archive->root = root;

    return result;
}

static union record rec_buf;

static union record *
get_next_record (struct vfs_s_super *archive, int tard)
{
    int n;

    n = mc_read (tard, rec_buf.charptr, RECORDSIZE);
    if (n != RECORDSIZE)
	return NULL;		/* An error has occurred */
    current_tar_position += RECORDSIZE;
    return &rec_buf;
}

static void skip_n_records (struct vfs_s_super *archive, int tard, int n)
{
    mc_lseek (tard, n * RECORDSIZE, SEEK_CUR);
    current_tar_position += n * RECORDSIZE;
}

static void fill_stat_from_header (struct vfs_class *me, struct stat *st, union record *header) 
{
    st->st_mode = from_oct (8, header->header.mode);

    /* Adjust st->st_mode because there are tar-files with
     * linkflag==LF_SYMLINK and S_ISLNK(mod)==0. I don't 
     * know about the other modes but I think I cause no new
     * problem when I adjust them, too. -- Norbert.
     */
    if (header->header.linkflag == LF_DIR) {
	st->st_mode |= S_IFDIR;
    } else if (header->header.linkflag == LF_SYMLINK) {
	st->st_mode |= S_IFLNK;
    } else if (header->header.linkflag == LF_CHR) {
	st->st_mode |= S_IFCHR;
    } else if (header->header.linkflag == LF_BLK) {
	st->st_mode |= S_IFBLK;
    } else if (header->header.linkflag == LF_FIFO) {
	st->st_mode |= S_IFIFO;
    } else 
	st->st_mode |= S_IFREG;

    st->st_rdev = 0;
    if (!strcmp (header->header.magic, TMAGIC)) {
	st->st_uid = *header->header.uname ? finduid (header->header.uname) :
	    from_oct (8, header->header.uid);
	st->st_gid = *header->header.gname ? findgid (header->header.gname) :
	    from_oct (8, header->header.gid);
	switch (header->header.linkflag) {
	case LF_BLK:
	case LF_CHR:
	    st->st_rdev = (from_oct (8, header->header.devmajor) << 8) |
		from_oct (8, header->header.devminor);
	}
    } else { /* Old Unix tar */
	st->st_uid = from_oct (8, header->header.uid);
	st->st_gid = from_oct (8, header->header.gid);
    }
    st->st_size = hstat.st_size;
    st->st_mtime = from_oct (1 + 12, header->header.mtime);
    st->st_atime = from_oct (1 + 12, header->header.atime);
    st->st_ctime = from_oct (1 + 12, header->header.ctime);
}


typedef enum {
    STATUS_BADCHECKSUM,
    STATUS_SUCCESS,
    STATUS_EOFMARK,
    STATUS_EOF
} ReadStatus;
/*
 * Return 1 for success, 0 if the checksum is bad, EOF on eof,
 * 2 for a record full of zeros (EOF marker).
 *
 */
static ReadStatus
read_header (struct vfs_class *me, struct vfs_s_super *archive, int tard)
{
    register int i;
    register long sum, signed_sum, recsum;
    register char *p;
    register union record *header;
    static char *next_long_name = NULL, *next_long_link = NULL;

  recurse:

    header = get_next_record (archive, tard);
    if (NULL == header)
	return STATUS_EOF;

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

    /*
     * This is a zeroed record...whole record is 0's except
     * for the 8 blanks we faked for the checksum field.
     */
    if (sum == 8 * ' ')
	return STATUS_EOFMARK;

    if (sum != recsum && signed_sum != recsum)
	return STATUS_BADCHECKSUM;
    
    /*
     * linkflag on BSDI tar (pax) always '\000'
     */
    if (header->header.linkflag == '\000' &&
	(i = strlen(header->header.arch_name)) &&
	header->header.arch_name[i - 1] == '/')
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
	char **longp;
	char *bp, *data;
	int size, written;

	longp = ((header->header.linkflag == LF_LONGNAME)
		 ? &next_long_name
		 : &next_long_link);

	if (*longp)
	    g_free (*longp);
	bp = *longp = g_malloc (hstat.st_size);

	for (size = hstat.st_size;
	     size > 0;
	     size -= written) {
	    data = get_next_record (archive, tard)->charptr;
	    if (data == NULL) {
		message_1s (1, MSG_ERROR, _("Unexpected EOF on archive file"));
		return STATUS_BADCHECKSUM;
	    }
	    written = RECORDSIZE;
	    if (written > size)
		written = size;

	    memcpy (bp, data, written);
	    bp += written;
	}
#if 0
	if (hstat.st_size > 1)
	    bp [hstat.st_size - 1] = 0;	/* just to make sure */
#endif
	goto recurse;
    } else {
	struct stat st;
	struct vfs_s_entry *entry;
	struct vfs_s_inode *inode, *parent;
	long data_position;
	char *q;
	int len;
	char *current_file_name, *current_link_name;

	current_link_name = (next_long_link
			     ? next_long_link
			     : g_strdup (header->header.arch_linkname));
	len = strlen (current_link_name);
	if (len > 1 && current_link_name [len - 1] == '/')
	    current_link_name[len - 1] = 0;

	current_file_name = (next_long_name
			     ? next_long_name
			     : g_strdup (header->header.arch_name));
	len = strlen (current_file_name);
	if (current_file_name[len - 1] == '/') {
	    current_file_name[len - 1] = 0;
	}

	data_position = current_tar_position;
	
	p = strrchr (current_file_name, '/');
	if (p == NULL) {
	    p = current_file_name;
	    q = current_file_name + len; /* "" */
	} else {
	    *(p++) = 0;
	    q = current_file_name;
	}

	parent = vfs_s_find_inode (me, archive->root, q, LINK_NO_FOLLOW, FL_MKDIR);
	if (parent == NULL) {
	    message_1s (1, MSG_ERROR, _("Inconsistent tar archive"));
	    return STATUS_BADCHECKSUM;
	}

	if (header->header.linkflag == LF_LINK) {
	    inode = vfs_s_find_inode (me, archive->root, current_link_name, LINK_NO_FOLLOW, 0);
	    if (inode == NULL) {
	        message_1s (1, MSG_ERROR, _("Inconsistent tar archive"));
	    } else {
		entry = vfs_s_new_entry(me, p, inode);
		vfs_s_insert_entry(me, parent, entry);
		g_free (current_link_name);
		goto done;
	    }
	}

	fill_stat_from_header (me, &st, header);
	inode = vfs_s_new_inode (me, archive, &st);

	inode->data_offset = data_position;
	if (*current_link_name) {
	    inode->linkname = current_link_name;
	} else if (current_link_name != next_long_link) {
	    g_free (current_link_name);
	}
	entry = vfs_s_new_entry (me, p, inode);

	vfs_s_insert_entry (me, parent, entry);
	g_free (current_file_name);

    done:
	next_long_link = next_long_name = NULL;

	if (header->header.isextended) {
	    while (get_next_record (archive, tard)->ext_hdr.isextended);
	    inode->data_offset = current_tar_position;
	}
	return STATUS_SUCCESS;
    }
}

/*
 * Main loop for reading an archive.
 * Returns 0 on success, -1 on error.
 */
static int open_archive (struct vfs_class *me, struct vfs_s_super *archive, char *name, char *op)
{
    ReadStatus status = STATUS_EOFMARK;		/* Initial status at start of archive */
    ReadStatus prev_status;
    int tard;

    current_tar_position = 0;
    if ((tard = tar_open_archive (me, name, archive)) == -1)	/* Open for reading */
	return -1;

    for (;;) {
	prev_status = status;
	status = read_header (me, archive, tard);


	switch (status) {

	case STATUS_SUCCESS:
	    skip_n_records (archive, tard, (hstat.st_size + RECORDSIZE - 1) / RECORDSIZE);
	    continue;

	    /*
	     * Invalid header:
	     *
	     * If the previous header was good, tell them
	     * that we are skipping bad ones.
	     */
	case STATUS_BADCHECKSUM:	
	    switch (prev_status){

		/* Error on first record */
	    case STATUS_EOFMARK:
		message_2s (1, MSG_ERROR, _("Hmm,...\n%s\ndoesn't look like a tar archive."), name);
		/* FALL THRU */

		/* Error after header rec */
	    case STATUS_SUCCESS:
		/* Error after error */

	    case STATUS_BADCHECKSUM:
		return -1;

	    case STATUS_EOF:
		return 0;
	    }

	    /* Record of zeroes */
	case STATUS_EOFMARK:	
	    status = prev_status;	/* If error after 0's */
	    /* FALL THRU */

	case STATUS_EOF:	/* End of archive */
	    break;
	}
	break;
    };
    return 0;
}

static void *tar_super_check(struct vfs_class *me, char *archive_name, char *op)
{
    static struct stat stat_buf;
    if (mc_stat (archive_name, &stat_buf))
	return NULL;
    return &stat_buf;
}

static int
tar_super_same (struct vfs_class *me, struct vfs_s_super *parc, char *archive_name,
		char *op, void *cookie)
{
    struct stat *archive_stat = cookie;	/* stat of main archive */

    if (strcmp (parc->name, archive_name))
	return 0;

    /* Has the cached archive been changed on the disk? */
    if (parc->u.arch.st.st_mtime < archive_stat->st_mtime) {
	/* Yes, reload! */
	(*vfs_tarfs_ops.free) ((vfsid) parc);
	vfs_rmstamp (&vfs_tarfs_ops, (vfsid) parc, 0);
	return 2;
    }
    /* Hasn't been modified, give it a new timeout */
    vfs_stamp (&vfs_tarfs_ops, (vfsid) parc);
    return 1;
}

static int tar_read (void *fh, char *buffer, int count)
{
    off_t begin = FH->ino->data_offset;
    int fd = FH_SUPER->u.arch.fd;
    struct vfs_class *me = FH_SUPER->me;

    if (mc_lseek (fd, begin + FH->pos, SEEK_SET) != 
        begin + FH->pos) ERRNOR (EIO, -1);

    count = MIN(count, FH->ino->st.st_size - FH->pos);

    if ((count = mc_read (fd, buffer, count)) == -1) ERRNOR (errno, -1);

    FH->pos += count;
    return count;
}

static int tar_ungetlocalcopy (struct vfs_class *me, char *path, char *local, int has_changed)
{
/* We do just nothing. (We are read only and do not need to free local,
   since it will be freed when tar archive will be freed */
/* We have to report error if file has changed */
    ERRNOR (EROFS, -has_changed);
}

static int tar_fh_open (struct vfs_class *me, struct vfs_s_fh *fh, int flags, int mode)
{
    if ((flags & O_ACCMODE) != O_RDONLY) ERRNOR (EROFS, -1);
    return 0;
}

static struct vfs_s_data tarfs_data = {
    NULL,
    0,
    0,
    NULL, /* logfile */

    NULL, /* init_inode */
    NULL, /* free_inode */
    NULL, /* init_entry */

    tar_super_check,
    tar_super_same,
    open_archive,
    tar_free_archive,

    tar_fh_open, /* fh_open */
    NULL, /* fh_close */

    vfs_s_find_entry_tree,
    NULL,
    NULL,
    NULL,

    NULL,
    NULL,
    NULL
};

void
init_tarfs (void)
{
    vfs_s_init_class (&vfs_tarfs_ops);
    vfs_tarfs_ops.name = "tarfs";
    vfs_tarfs_ops.prefix = "utar";
    vfs_tarfs_ops.data = &tarfs_data;
    vfs_tarfs_ops.read = tar_read;
    vfs_tarfs_ops.write = NULL;
    vfs_tarfs_ops.ungetlocalcopy = tar_ungetlocalcopy;
    vfs_tarfs_ops.setctl = NULL;
    vfs_register_class (&vfs_tarfs_ops);
}
