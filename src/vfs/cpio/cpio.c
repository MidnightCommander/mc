/*
   Virtual File System: GNU Tar file system.

   Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2007, 2011
   The Free Software Foundation, Inc.

   Written by:
   Jan Hudec, 2000

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 *  \brief Source: Virtual File System: GNU Tar file system.
 *  \author Jan Hudec
 *  \date 2000
 */

#include <config.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/unixcompat.h"
#include "lib/util.h"
#include "lib/widget.h"         /* message() */

#include "lib/vfs/vfs.h"
#include "lib/vfs/utilvfs.h"
#include "lib/vfs/xdirentry.h"
#include "lib/vfs/gc.h"         /* vfs_rmstamp */

#include "cpio.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define CPIO_POS(super) cpio_position
/* If some time reentrancy should be needed change it to */
/* #define CPIO_POS(super) (super)->u.arch.fd */

#define CPIO_SEEK_SET(super, where) \
        mc_lseek (((cpio_super_data_t *)(super)->data)->fd, \
                  CPIO_POS(super) = (where), SEEK_SET)
#define CPIO_SEEK_CUR(super, where) \
        mc_lseek (((cpio_super_data_t *)(super)->data)->fd, \
                  CPIO_POS(super) += (where), SEEK_SET)

#define MAGIC_LENGTH (6)        /* How many bytes we have to read ahead */
#define SEEKBACK CPIO_SEEK_CUR(super, ptr - top)
#define RETURN(x) return (((cpio_super_data_t *)super->data)->type = (x))
#define TYPEIS(x) \
        ((((cpio_super_data_t *)super->data)->type == CPIO_UNKNOWN) || \
         (((cpio_super_data_t *)super->data)->type == (x)))

#define HEAD_LENGTH (26)

/*** file scope type declarations ****************************************************************/

enum
{
    STATUS_START,
    STATUS_OK,
    STATUS_TRAIL,
    STATUS_FAIL,
    STATUS_EOF
};

enum
{
    CPIO_UNKNOWN = 0,           /* Not determined yet */
    CPIO_BIN,                   /* Binary format */
    CPIO_BINRE,                 /* Binary format, reverse endianity */
    CPIO_OLDC,                  /* Old ASCII format */
    CPIO_NEWC,                  /* New ASCII format */
    CPIO_CRC                    /* New ASCII format + CRC */
};

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

typedef struct
{
    unsigned long inumber;
    dev_t device;
    struct vfs_s_inode *inode;
} defer_inode;

typedef struct
{
    int fd;
    struct stat st;
    int type;                   /* Type of the archive */
    GSList *deferred;           /* List of inodes for which another entries may appear */
} cpio_super_data_t;

/*** file scope variables ************************************************************************/

static struct vfs_class vfs_cpiofs_ops;

static off_t cpio_position;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static ssize_t cpio_find_head (struct vfs_class *me, struct vfs_s_super *super);
static ssize_t cpio_read_bin_head (struct vfs_class *me, struct vfs_s_super *super);
static ssize_t cpio_read_oldc_head (struct vfs_class *me, struct vfs_s_super *super);
static ssize_t cpio_read_crc_head (struct vfs_class *me, struct vfs_s_super *super);
static ssize_t cpio_read (void *fh, char *buffer, size_t count);

/* --------------------------------------------------------------------------------------------- */

static int
cpio_defer_find (const void *a, const void *b)
{
    const defer_inode *a1 = (const defer_inode *) a;
    const defer_inode *b1 = (const defer_inode *) b;

    return (a1->inumber == b1->inumber && a1->device == b1->device) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */

static ssize_t
cpio_skip_padding (struct vfs_s_super *super)
{
    switch (((cpio_super_data_t *) super->data)->type)
    {
    case CPIO_BIN:
    case CPIO_BINRE:
        return CPIO_SEEK_CUR (super, (2 - (CPIO_POS (super) % 2)) % 2);
    case CPIO_NEWC:
    case CPIO_CRC:
        return CPIO_SEEK_CUR (super, (4 - (CPIO_POS (super) % 4)) % 4);
    case CPIO_OLDC:
        return CPIO_POS (super);
    default:
        g_assert_not_reached ();
        return 42;              /* & the compiler is happy :-) */
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
cpio_free_archive (struct vfs_class *me, struct vfs_s_super *super)
{
    cpio_super_data_t *arch = (cpio_super_data_t *) super->data;

    (void) me;

    if (super->data == NULL)
        return;

    if (arch->fd != -1)
        mc_close (arch->fd);
    arch->fd = -1;
    g_slist_foreach (arch->deferred, (GFunc) g_free, NULL);
    g_slist_free (arch->deferred);
    arch->deferred = NULL;
    g_free (super->data);
    super->data = NULL;
}

/* --------------------------------------------------------------------------------------------- */

static int
cpio_open_cpio_file (struct vfs_class *me, struct vfs_s_super *super, const vfs_path_t * vpath)
{
    int fd, type;
    cpio_super_data_t *arch;
    mode_t mode;
    struct vfs_s_inode *root;

    fd = mc_open (vpath, O_RDONLY);
    if (fd == -1)
    {
        char *name;

        name = vfs_path_to_str (vpath);
        message (D_ERROR, MSG_ERROR, _("Cannot open cpio archive\n%s"), name);
        g_free (name);
        return -1;
    }

    super->name = vfs_path_to_str (vpath);
    super->data = g_new (cpio_super_data_t, 1);
    arch = (cpio_super_data_t *) super->data;
    arch->fd = -1;              /* for now */
    mc_stat (vpath, &arch->st);
    arch->type = CPIO_UNKNOWN;
    arch->deferred = NULL;

    type = get_compression_type (fd, super->name);
    if (type != COMPRESSION_NONE)
    {
        char *s;
        vfs_path_t *tmp_vpath;

        mc_close (fd);
        s = g_strconcat (super->name, decompress_extension (type), (char *) NULL);
        tmp_vpath = vfs_path_from_str_flags (s, VPF_NO_CANON);
        fd = mc_open (tmp_vpath, O_RDONLY);
        vfs_path_free (tmp_vpath);
        if (fd == -1)
        {
            message (D_ERROR, MSG_ERROR, _("Cannot open cpio archive\n%s"), s);
            g_free (s);
            g_free (super->name);
            super->name = NULL;
            return -1;
        }
        g_free (s);
    }

    arch->fd = fd;
    mode = arch->st.st_mode & 07777;
    mode |= (mode & 0444) >> 2; /* set eXec where Read is */
    mode |= S_IFDIR;

    root = vfs_s_new_inode (me, super, &arch->st);
    root->st.st_mode = mode;
    root->data_offset = -1;
    root->st.st_nlink++;
    root->st.st_dev = MEDATA->rdev++;

    super->root = root;

    CPIO_SEEK_SET (super, 0);

    return fd;
}

/* --------------------------------------------------------------------------------------------- */

static ssize_t
cpio_read_head (struct vfs_class *me, struct vfs_s_super *super)
{
    switch (cpio_find_head (me, super))
    {
    case CPIO_UNKNOWN:
        return -1;
    case CPIO_BIN:
    case CPIO_BINRE:
        return cpio_read_bin_head (me, super);
    case CPIO_OLDC:
        return cpio_read_oldc_head (me, super);
    case CPIO_NEWC:
    case CPIO_CRC:
        return cpio_read_crc_head (me, super);
    default:
        g_assert_not_reached ();
        return 42;              /* & the compiler is happy :-) */
    }
}

/* --------------------------------------------------------------------------------------------- */

static ssize_t
cpio_find_head (struct vfs_class *me, struct vfs_s_super *super)
{
    cpio_super_data_t *arch = (cpio_super_data_t *) super->data;
    char buf[BUF_SMALL * 2];
    ssize_t ptr = 0;
    ssize_t top;
    ssize_t tmp;

    top = mc_read (arch->fd, buf, sizeof (buf));
    if (top > 0)
        CPIO_POS (super) += top;

    while (TRUE)
    {
        if (ptr + MAGIC_LENGTH >= top)
        {
            if (top > (ssize_t) (sizeof (buf) / 2))
            {
                memmove (buf, buf + top - sizeof (buf) / 2, sizeof (buf) / 2);
                ptr -= top - sizeof (buf) / 2;
                top = sizeof (buf) / 2;
            }
            tmp = mc_read (arch->fd, buf, top);
            if (tmp == 0 || tmp == -1)
            {
                message (D_ERROR, MSG_ERROR, _("Premature end of cpio archive\n%s"), super->name);
                cpio_free_archive (me, super);
                return CPIO_UNKNOWN;
            }
            top += tmp;
        }
        if (TYPEIS (CPIO_BIN) && ((*(unsigned short *) (buf + ptr)) == 070707))
        {
            SEEKBACK;
            RETURN (CPIO_BIN);
        }
        else if (TYPEIS (CPIO_BINRE)
                 && ((*(unsigned short *) (buf + ptr)) == GUINT16_SWAP_LE_BE_CONSTANT (070707)))
        {
            SEEKBACK;
            RETURN (CPIO_BINRE);
        }
        else if (TYPEIS (CPIO_OLDC) && (strncmp (buf + ptr, "070707", 6) == 0))
        {
            SEEKBACK;
            RETURN (CPIO_OLDC);
        }
        else if (TYPEIS (CPIO_NEWC) && (strncmp (buf + ptr, "070701", 6) == 0))
        {
            SEEKBACK;
            RETURN (CPIO_NEWC);
        }
        else if (TYPEIS (CPIO_CRC) && (strncmp (buf + ptr, "070702", 6) == 0))
        {
            SEEKBACK;
            RETURN (CPIO_CRC);
        };
        ptr++;
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
cpio_create_entry (struct vfs_class *me, struct vfs_s_super *super, struct stat *st, char *name)
{
    cpio_super_data_t *arch = (cpio_super_data_t *) super->data;
    struct vfs_s_inode *inode = NULL;
    struct vfs_s_inode *root = super->root;
    struct vfs_s_entry *entry = NULL;
    char *tn;

    switch (st->st_mode & S_IFMT)
    {                           /* For case of HP/UX archives */
    case S_IFCHR:
    case S_IFBLK:
#ifdef S_IFSOCK
    case S_IFSOCK:
#endif
#ifdef S_IFIFO
    case S_IFIFO:
#endif
#ifdef S_IFNAM
    case S_IFNAM:
#endif
        if ((st->st_size != 0) && (st->st_rdev == 0x0001))
        {
            /* FIXME: representation of major/minor differs between */
            /* different operating systems. */
            st->st_rdev = (unsigned) st->st_size;
            st->st_size = 0;
        }
        break;
    default:
        break;
    }

    if ((st->st_nlink > 1) && ((arch->type == CPIO_NEWC) || (arch->type == CPIO_CRC)))
    {                           /* For case of hardlinked files */
        defer_inode i = { st->st_ino, st->st_dev, NULL };
        GSList *l;

        l = g_slist_find_custom (arch->deferred, &i, cpio_defer_find);
        if (l != NULL)
        {
            inode = ((defer_inode *) l->data)->inode;
            if (inode->st.st_size != 0 && st->st_size != 0 && (inode->st.st_size != st->st_size))
            {
                message (D_ERROR, MSG_ERROR,
                         _("Inconsistent hardlinks of\n%s\nin cpio archive\n%s"),
                         name, super->name);
                inode = NULL;
            }
            else if (inode->st.st_size == 0)
                inode->st.st_size = st->st_size;
        }
    }

    /* remove trailing slashes */
    for (tn = name + strlen (name) - 1; tn >= name && *tn == PATH_SEP; tn--)
        *tn = '\0';

    tn = strrchr (name, PATH_SEP);
    if (tn == NULL)
        tn = name;
    else if (tn == name + 1)
    {
        /* started with "./" -- directory in the root of archive */
        tn++;
    }
    else
    {
        *tn = '\0';
        root = vfs_s_find_inode (me, super, name, LINK_FOLLOW, FL_MKDIR);
        *tn = PATH_SEP;
        tn++;
    }

    entry = MEDATA->find_entry (me, root, tn, LINK_FOLLOW, FL_NONE);    /* In case entry is already there */

    if (entry != NULL)
    {
        /* This shouldn't happen! (well, it can happen if there is a record for a
           file and than a record for a directory it is in; cpio would die with
           'No such file or directory' is such case) */

        if (!S_ISDIR (entry->ino->st.st_mode))
        {
            /* This can be considered archive inconsistency */
            message (D_ERROR, MSG_ERROR,
                     _("%s contains duplicate entries! Skipping!"), super->name);
        }
        else
        {
            entry->ino->st.st_mode = st->st_mode;
            entry->ino->st.st_uid = st->st_uid;
            entry->ino->st.st_gid = st->st_gid;
            entry->ino->st.st_atime = st->st_atime;
            entry->ino->st.st_mtime = st->st_mtime;
            entry->ino->st.st_ctime = st->st_ctime;
        }

        g_free (name);
    }
    else
    {                           /* !entry */
        if (inode == NULL)
        {
            inode = vfs_s_new_inode (me, super, st);
            if ((st->st_nlink > 0) && ((arch->type == CPIO_NEWC) || (arch->type == CPIO_CRC)))
            {
                /* For case of hardlinked files */
                defer_inode *i;

                i = g_new (defer_inode, 1);
                i->inumber = st->st_ino;
                i->device = st->st_dev;
                i->inode = inode;

                arch->deferred = g_slist_prepend (arch->deferred, i);
            }
        }

        if (st->st_size != 0)
            inode->data_offset = CPIO_POS (super);

        entry = vfs_s_new_entry (me, tn, inode);
        vfs_s_insert_entry (me, root, entry);

        g_free (name);

        if (!S_ISLNK (st->st_mode))
            CPIO_SEEK_CUR (super, st->st_size);
        else
        {
            inode->linkname = g_malloc (st->st_size + 1);

            if (mc_read (arch->fd, inode->linkname, st->st_size) < st->st_size)
            {
                inode->linkname[0] = '\0';
                return STATUS_EOF;
            }

            inode->linkname[st->st_size] = '\0';        /* Linkname stored without terminating \0 !!! */
            CPIO_POS (super) += st->st_size;
            cpio_skip_padding (super);
        }
    }                           /* !entry */

    return STATUS_OK;
}

/* --------------------------------------------------------------------------------------------- */

static ssize_t
cpio_read_bin_head (struct vfs_class *me, struct vfs_s_super *super)
{
    union
    {
        struct old_cpio_header buf;
        short shorts[HEAD_LENGTH >> 1];
    } u;

    cpio_super_data_t *arch = (cpio_super_data_t *) super->data;
    ssize_t len;
    char *name;
    struct stat st;

    len = mc_read (arch->fd, (char *) &u.buf, HEAD_LENGTH);
    if (len < HEAD_LENGTH)
        return STATUS_EOF;
    CPIO_POS (super) += len;
    if (arch->type == CPIO_BINRE)
    {
        int i;
        for (i = 0; i < (HEAD_LENGTH >> 1); i++)
            u.shorts[i] = GUINT16_SWAP_LE_BE_CONSTANT (u.shorts[i]);
    }

    if (u.buf.c_magic != 070707 || u.buf.c_namesize == 0 || u.buf.c_namesize > MC_MAXPATHLEN)
    {
        message (D_ERROR, MSG_ERROR, _("Corrupted cpio header encountered in\n%s"), super->name);
        return STATUS_FAIL;
    }
    name = g_malloc (u.buf.c_namesize);
    len = mc_read (arch->fd, name, u.buf.c_namesize);
    if (len < u.buf.c_namesize)
    {
        g_free (name);
        return STATUS_EOF;
    }
    name[u.buf.c_namesize - 1] = '\0';
    CPIO_POS (super) += len;
    cpio_skip_padding (super);

    if (!strcmp ("TRAILER!!!", name))
    {                           /* We got to the last record */
        g_free (name);
        return STATUS_TRAIL;
    }

    st.st_dev = u.buf.c_dev;
    st.st_ino = u.buf.c_ino;
    st.st_mode = u.buf.c_mode;
    st.st_nlink = u.buf.c_nlink;
    st.st_uid = u.buf.c_uid;
    st.st_gid = u.buf.c_gid;
    st.st_rdev = u.buf.c_rdev;
    st.st_size = (u.buf.c_filesizes[0] << 16) | u.buf.c_filesizes[1];
    st.st_atime = st.st_mtime = st.st_ctime = (u.buf.c_mtimes[0] << 16) | u.buf.c_mtimes[1];

    return cpio_create_entry (me, super, &st, name);
}

/* --------------------------------------------------------------------------------------------- */

#undef HEAD_LENGTH
#define HEAD_LENGTH (76)

static ssize_t
cpio_read_oldc_head (struct vfs_class *me, struct vfs_s_super *super)
{
    cpio_super_data_t *arch = (cpio_super_data_t *) super->data;
    struct new_cpio_header hd;
    union
    {
        struct stat st;
        char buf[HEAD_LENGTH + 1];
    } u;
    ssize_t len;
    char *name;

    if (mc_read (arch->fd, u.buf, HEAD_LENGTH) != HEAD_LENGTH)
        return STATUS_EOF;
    CPIO_POS (super) += HEAD_LENGTH;
    u.buf[HEAD_LENGTH] = 0;

    if (sscanf (u.buf, "070707%6lo%6lo%6lo%6lo%6lo%6lo%6lo%11lo%6lo%11lo",
                (unsigned long *) &hd.c_dev, &hd.c_ino, &hd.c_mode, &hd.c_uid, &hd.c_gid,
                &hd.c_nlink, (unsigned long *) &hd.c_rdev, &hd.c_mtime,
                &hd.c_namesize, &hd.c_filesize) < 10)
    {
        message (D_ERROR, MSG_ERROR, _("Corrupted cpio header encountered in\n%s"), super->name);
        return STATUS_FAIL;
    }

    if (hd.c_namesize == 0 || hd.c_namesize > MC_MAXPATHLEN)
    {
        message (D_ERROR, MSG_ERROR, _("Corrupted cpio header encountered in\n%s"), super->name);
        return STATUS_FAIL;
    }
    name = g_malloc (hd.c_namesize);
    len = mc_read (arch->fd, name, hd.c_namesize);
    if ((len == -1) || ((unsigned long) len < hd.c_namesize))
    {
        g_free (name);
        return STATUS_EOF;
    }
    name[hd.c_namesize - 1] = '\0';
    CPIO_POS (super) += len;
    cpio_skip_padding (super);

    if (!strcmp ("TRAILER!!!", name))
    {                           /* We got to the last record */
        g_free (name);
        return STATUS_TRAIL;
    }

    u.st.st_dev = hd.c_dev;
    u.st.st_ino = hd.c_ino;
    u.st.st_mode = hd.c_mode;
    u.st.st_nlink = hd.c_nlink;
    u.st.st_uid = hd.c_uid;
    u.st.st_gid = hd.c_gid;
    u.st.st_rdev = hd.c_rdev;
    u.st.st_size = hd.c_filesize;
    u.st.st_atime = u.st.st_mtime = u.st.st_ctime = hd.c_mtime;

    return cpio_create_entry (me, super, &u.st, name);
}

/* --------------------------------------------------------------------------------------------- */

#undef HEAD_LENGTH
#define HEAD_LENGTH (110)

static ssize_t
cpio_read_crc_head (struct vfs_class *me, struct vfs_s_super *super)
{
    cpio_super_data_t *arch = (cpio_super_data_t *) super->data;
    struct new_cpio_header hd;
    union
    {
        struct stat st;
        char buf[HEAD_LENGTH + 1];
    } u;
    ssize_t len;
    char *name;

    if (mc_read (arch->fd, u.buf, HEAD_LENGTH) != HEAD_LENGTH)
        return STATUS_EOF;

    CPIO_POS (super) += HEAD_LENGTH;
    u.buf[HEAD_LENGTH] = '\0';

    if (sscanf (u.buf, "%6ho%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx",
                &hd.c_magic, &hd.c_ino, &hd.c_mode, &hd.c_uid, &hd.c_gid,
                &hd.c_nlink, &hd.c_mtime, &hd.c_filesize,
                (unsigned long *) &hd.c_dev, (unsigned long *) &hd.c_devmin,
                (unsigned long *) &hd.c_rdev, (unsigned long *) &hd.c_rdevmin,
                &hd.c_namesize, &hd.c_chksum) < 14)
    {
        message (D_ERROR, MSG_ERROR, _("Corrupted cpio header encountered in\n%s"), super->name);
        return STATUS_FAIL;
    }

    if ((arch->type == CPIO_NEWC && hd.c_magic != 070701) ||
        (arch->type == CPIO_CRC && hd.c_magic != 070702))
        return STATUS_FAIL;

    if (hd.c_namesize == 0 || hd.c_namesize > MC_MAXPATHLEN)
    {
        message (D_ERROR, MSG_ERROR, _("Corrupted cpio header encountered in\n%s"), super->name);
        return STATUS_FAIL;
    }

    name = g_malloc (hd.c_namesize);
    len = mc_read (arch->fd, name, hd.c_namesize);

    if ((len == -1) || ((unsigned long) len < hd.c_namesize))
    {
        g_free (name);
        return STATUS_EOF;
    }
    name[hd.c_namesize - 1] = '\0';
    CPIO_POS (super) += len;
    cpio_skip_padding (super);

    if (strcmp ("TRAILER!!!", name) == 0)
    {                           /* We got to the last record */
        g_free (name);
        return STATUS_TRAIL;
    }

    u.st.st_dev = makedev (hd.c_dev, hd.c_devmin);
    u.st.st_ino = hd.c_ino;
    u.st.st_mode = hd.c_mode;
    u.st.st_nlink = hd.c_nlink;
    u.st.st_uid = hd.c_uid;
    u.st.st_gid = hd.c_gid;
    u.st.st_rdev = makedev (hd.c_rdev, hd.c_rdevmin);
    u.st.st_size = hd.c_filesize;
    u.st.st_atime = u.st.st_mtime = u.st.st_ctime = hd.c_mtime;

    return cpio_create_entry (me, super, &u.st, name);
}

/* --------------------------------------------------------------------------------------------- */
/** Need to CPIO_SEEK_CUR to skip the file at the end of add entry!!!! */

static int
cpio_open_archive (struct vfs_s_super *super, const vfs_path_t * vpath,
                   const vfs_path_element_t * vpath_element)
{
    (void) vpath_element;

    if (cpio_open_cpio_file (vpath_element->class, super, vpath) == -1)
        return -1;

    while (TRUE)
    {
        ssize_t status;

        status = cpio_read_head (vpath_element->class, super);

        switch (status)
        {
        case STATUS_EOF:
            {
                char *archive_name;

                archive_name = vfs_path_to_str (vpath);
                message (D_ERROR, MSG_ERROR, _("Unexpected end of file\n%s"), archive_name);
                g_free (archive_name);
                return 0;
            }
        case STATUS_OK:
            continue;
        case STATUS_TRAIL:
            break;
        }
        break;
    }

    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/** Remaining functions are exactly same as for tarfs (and were in fact just copied) */

static void *
cpio_super_check (const vfs_path_t * vpath)
{
    static struct stat sb;
    int stat_result;

    stat_result = mc_stat (vpath, &sb);
    return (stat_result == 0 ? &sb : NULL);
}

/* --------------------------------------------------------------------------------------------- */

static int
cpio_super_same (const vfs_path_element_t * vpath_element, struct vfs_s_super *parc,
                 const vfs_path_t * vpath, void *cookie)
{
    struct stat *archive_stat = cookie; /* stat of main archive */
    char *archive_name = vfs_path_to_str (vpath);

    (void) vpath_element;

    if (strcmp (parc->name, archive_name))
    {
        g_free (archive_name);
        return 0;
    }
    g_free (archive_name);

    /* Has the cached archive been changed on the disk? */
    if (((cpio_super_data_t *) parc->data)->st.st_mtime < archive_stat->st_mtime)
    {
        /* Yes, reload! */
        (*vfs_cpiofs_ops.free) ((vfsid) parc);
        vfs_rmstamp (&vfs_cpiofs_ops, (vfsid) parc);
        return 2;
    }
    /* Hasn't been modified, give it a new timeout */
    vfs_stamp (&vfs_cpiofs_ops, (vfsid) parc);
    return 1;
}

/* --------------------------------------------------------------------------------------------- */

static ssize_t
cpio_read (void *fh, char *buffer, size_t count)
{
    off_t begin = FH->ino->data_offset;
    int fd = ((cpio_super_data_t *) FH_SUPER->data)->fd;
    struct vfs_class *me = FH_SUPER->me;
    ssize_t res;

    if (mc_lseek (fd, begin + FH->pos, SEEK_SET) != begin + FH->pos)
        ERRNOR (EIO, -1);

    count = MIN (count, (size_t) (FH->ino->st.st_size - FH->pos));

    res = mc_read (fd, buffer, count);
    if (res == -1)
        ERRNOR (errno, -1);

    FH->pos += res;
    return res;
}

/* --------------------------------------------------------------------------------------------- */

static int
cpio_fh_open (struct vfs_class *me, vfs_file_handler_t * fh, int flags, mode_t mode)
{
    (void) mode;

    fh->data = NULL;

    if ((flags & O_ACCMODE) != O_RDONLY)
        ERRNOR (EROFS, -1);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
init_cpiofs (void)
{
    static struct vfs_s_subclass cpio_subclass;

    cpio_subclass.flags = VFS_S_READONLY;       /* FIXME: cpiofs used own temp files */
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

/* --------------------------------------------------------------------------------------------- */
