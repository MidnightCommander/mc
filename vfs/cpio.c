/* Virtual File System: GNU Tar file system.
   Copyright (C) 2000 The Free Software Foundation
   
   Written by: 2000 Jan Hudec

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

#include <config.h>
#include <errno.h>
#include "utilvfs.h"
#include "gc.h"		/* vfs_rmstamp */
#include "xdirentry.h"

enum {
    STATUS_START,
    STATUS_OK,
    STATUS_TRAIL,
    STATUS_FAIL,
    STATUS_EOF
};

enum {
    CPIO_UNKNOWN = 0, /* Not determined yet */
    CPIO_BIN,         /* Binary format */
    CPIO_BINRE,       /* Binary format, reverse endianity */
    CPIO_OLDC,        /* Old ASCII format */
    CPIO_NEWC,        /* New ASCII format */
    CPIO_CRC          /* New ASCII format + CRC */
};

static struct vfs_class vfs_cpiofs_ops;

struct old_cpio_header
{
  unsigned short c_magic;
  short c_dev;
  unsigned short c_ino;
  unsigned short c_mode;
  unsigned short c_uid;
  unsigned short c_gid;
  unsigned short c_nlink;
  short c_rdev;
  unsigned short c_mtimes[2];
  unsigned short c_namesize;
  unsigned short c_filesizes[2];
};

struct new_cpio_header
{
  unsigned short c_magic;
  unsigned long c_ino;
  unsigned long c_mode;
  unsigned long c_uid;
  unsigned long c_gid;
  unsigned long c_nlink;
  unsigned long c_mtime;
  unsigned long c_filesize;
  long c_dev;
  long c_devmin;
  long c_rdev;
  long c_rdevmin;
  unsigned long c_namesize;
  unsigned long c_chksum;
};

struct defer_inode {
    struct defer_inode *next;
    unsigned long inumber;
    unsigned short device;
    struct vfs_s_inode *inode;
};

static int cpio_position;

static int cpio_find_head(struct vfs_class *me, struct vfs_s_super *super);
static int cpio_read_bin_head(struct vfs_class *me, struct vfs_s_super *super);
static int cpio_read_oldc_head(struct vfs_class *me, struct vfs_s_super *super);
static int cpio_read_crc_head(struct vfs_class *me, struct vfs_s_super *super);
static int cpio_create_entry(struct vfs_class *me, struct vfs_s_super *super, struct stat *stat, char *name);
static int cpio_read(void *fh, char *buffer, int count);

#define CPIO_POS(super) cpio_position
/* If some time reentrancy should be needed change it to */
/* #define CPIO_POS(super) (super)->u.arch.fd */

#define CPIO_SEEK_SET(super, where) mc_lseek((super)->u.arch.fd, CPIO_POS(super) = (where), SEEK_SET)
#define CPIO_SEEK_CUR(super, where) mc_lseek((super)->u.arch.fd, CPIO_POS(super) += (where), SEEK_SET)

static struct defer_inode *
cpio_defer_find (struct defer_inode *l, struct defer_inode *i)
{
    if (!l)
	return NULL;
    return l->inumber == i->inumber
	&& l->device == i->device ? l : cpio_defer_find (l->next, i);
}

static int cpio_skip_padding(struct vfs_s_super *super)
{
    switch(super->u.arch.type) {
    case CPIO_BIN:
    case CPIO_BINRE:
	return CPIO_SEEK_CUR(super, (2 - (CPIO_POS(super) % 2)) % 2);
    case CPIO_NEWC:
    case CPIO_CRC:
	return CPIO_SEEK_CUR(super, (4 - (CPIO_POS(super) % 4)) % 4);
    case CPIO_OLDC:
	return CPIO_POS(super);
    default:
	g_assert_not_reached();
	return 42; /* & the compiler is happy :-) */
    }
}

static void cpio_free_archive(struct vfs_class *me, struct vfs_s_super *super)
{
    if(super->u.arch.fd != -1)
	mc_close(super->u.arch.fd);
}

static int
cpio_open_cpio_file (struct vfs_class *me, struct vfs_s_super *super,
		     const char *name)
{
    int fd, type;
    mode_t mode;
    struct vfs_s_inode *root;

    if ((fd = mc_open (name, O_RDONLY)) == -1) {
	message (1, MSG_ERROR, _("Cannot open cpio archive\n%s"), name);
	return -1;
    }

    super->name = g_strdup (name);
    super->u.arch.fd = -1;	/* for now */
    mc_stat (name, &(super->u.arch.st));
    super->u.arch.type = CPIO_UNKNOWN;

    type = get_compression_type (fd);
    if (type != COMPRESSION_NONE) {
	char *s;

	mc_close (fd);
	s = g_strconcat (name, decompress_extension (type), NULL);
	if ((fd = mc_open (s, O_RDONLY)) == -1) {
	    message (1, MSG_ERROR, _("Cannot open cpio archive\n%s"), s);
	    g_free (s);
	    return -1;
	}
	g_free (s);
    }

    super->u.arch.fd = fd;
    mode = super->u.arch.st.st_mode & 07777;
    mode |= (mode & 0444) >> 2;	/* set eXec where Read is */
    mode |= S_IFDIR;

    root = vfs_s_new_inode (me, super, &(super->u.arch.st));
    root->st.st_mode = mode;
    root->data_offset = -1;
    root->st.st_nlink++;
    root->st.st_dev = MEDATA->rdev++;

    super->root = root;

    CPIO_SEEK_SET (super, 0);

    return fd;
}

static int cpio_read_head(struct vfs_class *me, struct vfs_s_super *super)
{
    switch(cpio_find_head(me, super)) {
    case CPIO_UNKNOWN:
	return -1;
    case CPIO_BIN:
    case CPIO_BINRE:
	return cpio_read_bin_head(me, super);
    case CPIO_OLDC:
	return cpio_read_oldc_head(me, super);
    case CPIO_NEWC:
    case CPIO_CRC:
	return cpio_read_crc_head(me, super);
    default:
	g_assert_not_reached();
	return 42; /* & the compiler is happy :-) */
    }
}

#define MAGIC_LENGTH (6) /* How many bytes we have to read ahead */
#define SEEKBACK CPIO_SEEK_CUR(super, ptr - top)
#define RETURN(x) return(super->u.arch.type = (x))
#define TYPEIS(x) ((super->u.arch.type == CPIO_UNKNOWN) || (super->u.arch.type == (x)))
static int cpio_find_head(struct vfs_class *me, struct vfs_s_super *super)
{
    char buf[256];
    int ptr = 0;
    int top = 0;
    int tmp;

    top = mc_read(super->u.arch.fd, buf, 256);
    CPIO_POS(super) += top;
    for(;;) {
	if(ptr + MAGIC_LENGTH >= top) {
	    if(top > 128) {
		memmove(buf, buf + top - 128, 128);
		top = 128;
		ptr -= top - 128;
	    }
	    if((tmp = mc_read(super->u.arch.fd, buf, top)) == 0 || tmp == -1) {
		message (1, MSG_ERROR, _("Premature end of cpio archive\n%s"), super->name);
		cpio_free_archive(me, super);
		return CPIO_UNKNOWN;
	    }
	    top += tmp;
	}
	if(TYPEIS(CPIO_BIN) && ((*(unsigned short *)(buf + ptr)) == 070707)) {
	    SEEKBACK; RETURN(CPIO_BIN);
	} else if(TYPEIS(CPIO_BINRE) && ((*(unsigned short *)(buf + ptr)) == GUINT16_SWAP_LE_BE_CONSTANT(070707))) {
	    SEEKBACK; RETURN(CPIO_BINRE);
	} else if(TYPEIS(CPIO_OLDC) && (!strncmp(buf + ptr, "070707", 6))) {
	    SEEKBACK; RETURN(CPIO_OLDC);
	} else if(TYPEIS(CPIO_NEWC) && (!strncmp(buf + ptr, "070701", 6))) {
	    SEEKBACK; RETURN(CPIO_NEWC);
	} else if(TYPEIS(CPIO_CRC) && (!strncmp(buf + ptr, "070702", 6))) {
	    SEEKBACK; RETURN(CPIO_CRC);
	};
	ptr++;
    }
}
#undef RETURN
#undef SEEKBACK

#define HEAD_LENGTH (26)
static int cpio_read_bin_head(struct vfs_class *me, struct vfs_s_super *super)
{
    struct old_cpio_header buf;
    int len;
    char *name;
    struct stat stat;

    if((len = mc_read(super->u.arch.fd, (char *)&buf, HEAD_LENGTH)) < HEAD_LENGTH)
	return STATUS_EOF;
    CPIO_POS(super) += len;
    if(super->u.arch.type == CPIO_BINRE) {
	int i;
	for(i = 0; i < (HEAD_LENGTH >> 1); i++)
	    ((short *)&buf)[i] = GUINT16_SWAP_LE_BE(((short *)&buf)[i]);
    }
    g_assert(buf.c_magic == 070707);

    name = g_malloc(buf.c_namesize);
    if((len = mc_read(super->u.arch.fd, name, buf.c_namesize)) < buf.c_namesize) {
	g_free(name);
	return STATUS_EOF;
    }
    CPIO_POS(super) += len;
    cpio_skip_padding(super);

    if(!strcmp("TRAILER!!!", name)) { /* We got to the last record */
	g_free(name);
	return STATUS_TRAIL;
    }

    stat.st_dev = buf.c_dev;
    stat.st_ino = buf.c_ino;
    stat.st_mode = buf.c_mode;
    stat.st_nlink = buf.c_nlink;
    stat.st_uid = buf.c_uid;
    stat.st_gid = buf.c_gid;
    stat.st_rdev = buf.c_rdev;
    stat.st_size = (buf.c_filesizes[0] << 16) | buf.c_filesizes[1];
    stat.st_atime = stat.st_mtime = stat.st_ctime = (buf.c_mtimes[0] << 16) | buf.c_mtimes[1];

    return cpio_create_entry(me, super, &stat, name);
}
#undef HEAD_LENGTH

#define HEAD_LENGTH (76)
static int cpio_read_oldc_head(struct vfs_class *me, struct vfs_s_super *super)
{
    struct new_cpio_header hd;
    struct stat stat;
    char buf[HEAD_LENGTH + 1];
    int len;
    char *name;

    if((len = mc_read(super->u.arch.fd, buf, HEAD_LENGTH)) < HEAD_LENGTH)
	return STATUS_EOF;
    CPIO_POS(super) += len;
    buf[HEAD_LENGTH] = 0;

    if(sscanf(buf, "070707%6lo%6lo%6lo%6lo%6lo%6lo%6lo%11lo%6lo%11lo",
	      &hd.c_dev, &hd.c_ino, &hd.c_mode, &hd.c_uid, &hd.c_gid,
	      &hd.c_nlink, &hd.c_rdev, &hd.c_mtime,
	      &hd.c_namesize, &hd.c_filesize) < 10) {
	message (1, MSG_ERROR, _("Corrupted cpio header encountered in\n%s"), super->name);
	return STATUS_FAIL;
    }

    name = g_malloc(hd.c_namesize);
    if((len = mc_read(super->u.arch.fd, name, hd.c_namesize)) < hd.c_namesize) {
	g_free (name);
	return STATUS_EOF;
    }
    CPIO_POS(super) +=  len;
    cpio_skip_padding(super);

    if(!strcmp("TRAILER!!!", name)) { /* We got to the last record */
	g_free(name);
	return STATUS_TRAIL;
    }

    stat.st_dev = hd.c_dev;
    stat.st_ino = hd.c_ino;
    stat.st_mode = hd.c_mode;
    stat.st_nlink = hd.c_nlink;
    stat.st_uid = hd.c_uid;
    stat.st_gid = hd.c_gid;
    stat.st_rdev = hd.c_rdev;
    stat.st_size = hd.c_filesize;
    stat.st_atime = stat.st_mtime = stat.st_ctime = hd.c_mtime;

    return cpio_create_entry(me, super, &stat, name);
}
#undef HEAD_LENGTH

#define HEAD_LENGTH (110)
static int cpio_read_crc_head(struct vfs_class *me, struct vfs_s_super *super)
{
    struct new_cpio_header hd;
    struct stat stat;
    char buf[HEAD_LENGTH + 1];
    int len;
    char *name;

    if((len = mc_read(super->u.arch.fd, buf, HEAD_LENGTH)) < HEAD_LENGTH)
	return STATUS_EOF;
    CPIO_POS(super) += len;
    buf[HEAD_LENGTH] = 0;

    if(sscanf(buf, "%6ho%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx",
	      &hd.c_magic, &hd.c_ino, &hd.c_mode, &hd.c_uid, &hd.c_gid,
	      &hd.c_nlink,  &hd.c_mtime, &hd.c_filesize,
	      &hd.c_dev, &hd.c_devmin, &hd.c_rdev, &hd.c_rdevmin,
	      &hd.c_namesize, &hd.c_chksum) < 14) {
	message (1, MSG_ERROR, _("Corrupted cpio header encountered in\n%s"),
		   super->name);
	return STATUS_FAIL;
    }

    if((super->u.arch.type == CPIO_NEWC && hd.c_magic != 070701) ||
       (super->u.arch.type == CPIO_CRC && hd.c_magic != 070702))
	return STATUS_FAIL;

    name = g_malloc(hd.c_namesize);
    if((len = mc_read(super->u.arch.fd, name, hd.c_namesize)) < hd.c_namesize) {
	g_free (name);
	return STATUS_EOF;
    }
    CPIO_POS(super) += len;
    cpio_skip_padding(super);

    if(!strcmp("TRAILER!!!", name)) { /* We got to the last record */
	g_free(name);
	return STATUS_TRAIL;
    }

    stat.st_dev = (hd.c_dev << 8) + hd.c_devmin;
    stat.st_ino = hd.c_ino;
    stat.st_mode = hd.c_mode;
    stat.st_nlink = hd.c_nlink;
    stat.st_uid = hd.c_uid;
    stat.st_gid = hd.c_gid;
    stat.st_rdev = (hd.c_rdev << 8) + hd.c_rdevmin;
    stat.st_size = hd.c_filesize;
    stat.st_atime = stat.st_mtime = stat.st_ctime = hd.c_mtime;

    return cpio_create_entry(me, super, &stat, name);
}

static int
cpio_create_entry (struct vfs_class *me, struct vfs_s_super *super,
		   struct stat *stat, char *name)
{
    struct vfs_s_inode *inode = NULL;
    struct vfs_s_inode *root = super->root;
    struct vfs_s_entry *entry = NULL;
    char *tn;

    switch (stat->st_mode & S_IFMT) {	/* For case of HP/UX archives */
    case S_IFCHR:
    case S_IFBLK:
#ifdef S_IFSOCK
    case S_IFSOCK:
#endif
#ifdef S_IFIFO
    case S_IFIFO:
#endif
	if ((stat->st_size != 0) && (stat->st_rdev == 0x0001)) {
	    stat->st_rdev = (unsigned) stat->st_size;
	    stat->st_size = 0;
	}
	break;
    default:
	break;
    }

    if ((stat->st_nlink > 1) && (super->u.arch.type == CPIO_NEWC || super->u.arch.type == CPIO_CRC)) {	/* For case of hardlinked files */
	struct defer_inode i, *l;
	i.inumber = stat->st_ino;
	i.device = stat->st_dev;
	i.inode = NULL;
	if ((l = cpio_defer_find (super->u.arch.deferred, &i)) != NULL) {
	    inode = l->inode;
	    if (inode->st.st_size && stat->st_size
		&& (inode->st.st_size != stat->st_size)) {
		message (1, MSG_ERROR,
			 _
			 ("Inconsistent hardlinks of\n%s\nin cpio archive\n%s"),
			 name, super->name);
		inode = NULL;
	    }
	}
    }

    for (tn = name + strlen (name) - 1; tn >= name && *tn == PATH_SEP; tn--)
	*tn = 0;
    if ((tn = strrchr (name, PATH_SEP))) {
	*tn = 0;
	root = vfs_s_find_inode (me, super, name, LINK_FOLLOW, FL_MKDIR);
	*tn = PATH_SEP;
	tn++;
    } else
	tn = name;

    entry = MEDATA->find_entry (me, root, tn, LINK_FOLLOW, FL_NONE);	/* In case entry is already there */

    if (entry) {		/* This shouldn't happen! (well, it can happen if there is a record for a 
				   file and than a record for a directory it is in; cpio would die with
				   'No such file or directory' is such case) */

	if (!S_ISDIR (entry->ino->st.st_mode)) {	/* This can be considered archive inconsistency */
	    message (1, MSG_ERROR,
		     _("%s contains duplicate entries! Skipping!"),
		     super->name);
	} else {
	    entry->ino->st.st_mode = stat->st_mode;
	    entry->ino->st.st_uid = stat->st_uid;
	    entry->ino->st.st_gid = stat->st_gid;
	    entry->ino->st.st_atime = stat->st_atime;
	    entry->ino->st.st_mtime = stat->st_mtime;
	    entry->ino->st.st_ctime = stat->st_ctime;
	}

    } else {			/* !entry */

	if (!inode) {
	    inode = vfs_s_new_inode (me, super, stat);
	    if ((stat->st_nlink > 0) && (super->u.arch.type == CPIO_NEWC || super->u.arch.type == CPIO_CRC)) {	/* For case of hardlinked files */
		struct defer_inode *i;
		i = g_new (struct defer_inode, 1);
		i->inumber = stat->st_ino;
		i->device = stat->st_dev;
		i->inode = inode;
		i->next = super->u.arch.deferred;
		super->u.arch.deferred = i;
	    }
	}

	if (stat->st_size)
	    inode->data_offset = CPIO_POS (super);

	entry = vfs_s_new_entry (me, tn, inode);
	vfs_s_insert_entry (me, root, entry);

	if (S_ISLNK (stat->st_mode)) {
	    inode->linkname = g_malloc (stat->st_size + 1);
	    if (mc_read (super->u.arch.fd, inode->linkname, stat->st_size)
		< stat->st_size) {
		inode->linkname[0] = 0;
		g_free (name);
		return STATUS_EOF;
	    }
	    inode->linkname[stat->st_size] = 0;	/* Linkname stored without terminating \0 !!! */
	    CPIO_POS (super) += stat->st_size;
	    cpio_skip_padding (super);
	} else {
	    CPIO_SEEK_CUR (super, stat->st_size);
	}

    }				/* !entry */

    g_free (name);
    return STATUS_OK;
}

/* Need to CPIO_SEEK_CUR to skip the file at the end of add entry!!!! */

static int
cpio_open_archive (struct vfs_class *me, struct vfs_s_super *super,
		   const char *name, char *op)
{
    int status = STATUS_START;

    if (cpio_open_cpio_file (me, super, name) == -1)
	return -1;

    for (;;) {
	status = cpio_read_head (me, super);

	switch (status) {
	case STATUS_EOF:
	    message (1, MSG_ERROR, _("Unexpected end of file\n%s"), name);
	    return 0;
	case STATUS_OK:
	    continue;
	case STATUS_TRAIL:
	    break;
	}
	break;
    }

    return 0;
}

/* Remaining functions are exactly same as for tarfs (and were in fact just copied) */
static void *
cpio_super_check (struct vfs_class *me, const char *archive_name, char *op)
{
    static struct stat sb;
    if (mc_stat (archive_name, &sb))
	return NULL;
    return &sb;
}

static int
cpio_super_same (struct vfs_class *me, struct vfs_s_super *parc,
		 const char *archive_name, char *op, void *cookie)
{
    struct stat *archive_stat = cookie;	/* stat of main archive */

    if (strcmp (parc->name, archive_name))
	return 0;

    /* Has the cached archive been changed on the disk? */
    if (parc->u.arch.st.st_mtime < archive_stat->st_mtime) {
	/* Yes, reload! */
	(*vfs_cpiofs_ops.free) ((vfsid) parc);
	vfs_rmstamp (&vfs_cpiofs_ops, (vfsid) parc, 0);
	return 2;
    }
    /* Hasn't been modified, give it a new timeout */
    vfs_stamp (&vfs_cpiofs_ops, (vfsid) parc);
    return 1;
}

static int cpio_read(void *fh, char *buffer, int count)
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

static int cpio_fh_open(struct vfs_class *me, struct vfs_s_fh *fh, int flags, int mode)
{
    if ((flags & O_ACCMODE) != O_RDONLY) ERRNOR (EROFS, -1);
    return 0;
}

void
init_cpiofs (void)
{
    static struct vfs_s_subclass cpio_subclass;

    cpio_subclass.flags = VFS_S_READONLY;
    cpio_subclass.archive_check = cpio_super_check;
    cpio_subclass.archive_same = cpio_super_same;
    cpio_subclass.open_archive = cpio_open_archive;
    cpio_subclass.free_archive = cpio_free_archive;
    cpio_subclass.fh_open = cpio_fh_open;

    vfs_s_init_class (&vfs_cpiofs_ops, &cpio_subclass);
    vfs_cpiofs_ops.name = "cpiofs";
    vfs_cpiofs_ops.prefix = "ucpio";
    vfs_cpiofs_ops.read = cpio_read;
    vfs_cpiofs_ops.setctl = NULL;
    vfs_register_class (&vfs_cpiofs_ops);
}
