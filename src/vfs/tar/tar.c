/*
   Virtual File System: GNU Tar file system.

   Copyright (C) 1995-2024
   Free Software Foundation, Inc.

   Written by:
   Jakub Jelinek, 1995
   Pavel Machek, 1998
   Slava Zanko <slavazanko@gmail.com>, 2013

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

/**
 * \file
 * \brief Source: Virtual File System: GNU Tar file system
 * \author Jakub Jelinek
 * \author Pavel Machek
 * \date 1995, 1998
 */

#include <config.h>

#include <errno.h>
#include <string.h>             /* memset() */

#ifdef hpux
/* major() and minor() macros (among other things) defined here for hpux */
#include <sys/mknod.h>
#endif

#include "lib/global.h"
#include "lib/util.h"
#include "lib/unixcompat.h"     /* makedev() */
#include "lib/widget.h"         /* message() */

#include "lib/vfs/vfs.h"
#include "lib/vfs/utilvfs.h"
#include "lib/vfs/gc.h"         /* vfs_rmstamp */

#include "tar-internal.h"
#include "tar.h"

/*** global variables ****************************************************************************/

/* Size of each record, once in blocks, once in bytes. Those two variables are always related,
   the second being BLOCKSIZE times the first. */
const int blocking_factor = DEFAULT_BLOCKING;
const size_t record_size = DEFAULT_BLOCKING * BLOCKSIZE;

/* As we open one archive at a time, it is safe to have these static */
union block *record_end;        /* last+1 block of archive record */
union block *current_block;     /* current block of archive */
off_t record_start_block;       /* block ordinal at record_start */

union block *current_header;

/* Have we hit EOF yet?  */
gboolean hit_eof;

struct tar_stat_info current_stat_info;

/*** file scope macro definitions ****************************************************************/

#define TAR_SUPER(super) ((tar_super_t *) (super))

/* tar Header Block, from POSIX 1003.1-1990.  */

/* The magic field is filled with this if uname and gname are valid. */
#define TMAGIC "ustar"          /* ustar and a null */

#define XHDTYPE 'x'             /* Extended header referring to the next file in the archive */
#define XGLTYPE 'g'             /* Global extended header */

/* Values used in typeflag field.  */
#define REGTYPE  '0'            /* regular file */
#define AREGTYPE '\0'           /* regular file */
#define LNKTYPE  '1'            /* link */
#define SYMTYPE  '2'            /* symbolic link */
#define CHRTYPE  '3'            /* character special */
#define BLKTYPE  '4'            /* block special */
#define DIRTYPE  '5'            /* directory */
#define FIFOTYPE '6'            /* FIFO special */


/* OLDGNU_MAGIC uses both magic and version fields, which are contiguous.
   Found in an archive, it indicates an old GNU header format, which will be
   hopefully become obsolescent.  With OLDGNU_MAGIC, uname and gname are
   valid, though the header is not truly POSIX conforming.  */
#define OLDGNU_MAGIC "ustar  "  /* 7 chars and a null */


/* Bits used in the mode field, values in octal.  */
#define TSUID    04000          /* set UID on execution */
#define TSGID    02000          /* set GID on execution */
#define TSVTX    01000          /* reserved */
                                /* file permissions */
#define TUREAD   00400          /* read by owner */
#define TUWRITE  00200          /* write by owner */
#define TUEXEC   00100          /* execute/search by owner */
#define TGREAD   00040          /* read by group */
#define TGWRITE  00020          /* write by group */
#define TGEXEC   00010          /* execute/search by group */
#define TOREAD   00004          /* read by other */
#define TOWRITE  00002          /* write by other */
#define TOEXEC   00001          /* execute/search by other */

#define GID_FROM_HEADER(where) gid_from_header (where, sizeof (where))
#define MAJOR_FROM_HEADER(where) major_from_header (where, sizeof (where))
#define MINOR_FROM_HEADER(where) minor_from_header (where, sizeof (where))
#define MODE_FROM_HEADER(where,hbits) mode_from_header (where, sizeof (where), hbits)
#define TIME_FROM_HEADER(where) time_from_header (where, sizeof (where))
#define UID_FROM_HEADER(where) uid_from_header (where, sizeof (where))

/*** file scope type declarations ****************************************************************/

typedef enum
{
    HEADER_STILL_UNREAD,        /* for when read_header has not been called */
    HEADER_SUCCESS,             /* header successfully read and checksummed */
    HEADER_ZERO_BLOCK,          /* zero block where header expected */
    HEADER_END_OF_FILE,         /* true end of file while header expected */
    HEADER_FAILURE              /* ill-formed header, or bad checksum */
} read_header;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static struct vfs_s_subclass tarfs_subclass;
static struct vfs_class *vfs_tarfs_ops = VFS_CLASS (&tarfs_subclass);

static struct timespec start_time;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
tar_stat_destroy (struct tar_stat_info *st)
{
    g_free (st->orig_file_name);
    g_free (st->file_name);
    g_free (st->link_name);
#if 0
    g_free (st->uname);
    g_free (st->gname);
#endif
    if (st->sparse_map != NULL)
    {
        g_array_free (st->sparse_map, TRUE);
        st->sparse_map = NULL;
    }
    tar_xheader_destroy (&st->xhdr);
    memset (st, 0, sizeof (*st));
}

/* --------------------------------------------------------------------------------------------- */

static inline gid_t
gid_from_header (const char *p, size_t s)
{
    return tar_from_header (p, s, "gid_t", TYPE_MINIMUM (gid_t), TYPE_MAXIMUM (gid_t), FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static inline major_t
major_from_header (const char *p, size_t s)
{
    return tar_from_header (p, s, "major_t", TYPE_MINIMUM (major_t), TYPE_MAXIMUM (major_t), FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static inline minor_t
minor_from_header (const char *p, size_t s)
{
    return tar_from_header (p, s, "minor_t", TYPE_MINIMUM (minor_t), TYPE_MAXIMUM (minor_t), FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Convert @p to the file mode, as understood by tar.
 * Store unrecognized mode bits (from 10th up) in @hbits.
 * Set *hbits if there are any unrecognized bits.
 * */
static inline mode_t
mode_from_header (const char *p, size_t s, gboolean * hbits)
{
    unsigned int u;
    mode_t mode;

    /* Do not complain about unrecognized mode bits. */
    u = tar_from_header (p, s, "mode_t", INTMAX_MIN, UINTMAX_MAX, FALSE);

    /* *INDENT-OFF* */
    mode = ((u & TSUID ? S_ISUID : 0)
          | (u & TSGID ? S_ISGID : 0)
          | (u & TSVTX ? S_ISVTX : 0)
          | (u & TUREAD ? S_IRUSR : 0)
          | (u & TUWRITE ? S_IWUSR : 0)
          | (u & TUEXEC ? S_IXUSR : 0)
          | (u & TGREAD ? S_IRGRP : 0)
          | (u & TGWRITE ? S_IWGRP : 0)
          | (u & TGEXEC ? S_IXGRP : 0)
          | (u & TOREAD ? S_IROTH : 0)
          | (u & TOWRITE ? S_IWOTH : 0)
          | (u & TOEXEC ? S_IXOTH : 0));
    /* *INDENT-ON* */

    if (hbits != NULL)
        *hbits = (u & ~07777) != 0;

    return mode;
}

/* --------------------------------------------------------------------------------------------- */

static inline time_t
time_from_header (const char *p, size_t s)
{
    return tar_from_header (p, s, "time_t", TYPE_MINIMUM (time_t), TYPE_MAXIMUM (time_t), FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static inline uid_t
uid_from_header (const char *p, size_t s)
{
    return tar_from_header (p, s, "uid_t", TYPE_MINIMUM (uid_t), TYPE_MAXIMUM (uid_t), FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static void
tar_calc_sparse_offsets (struct vfs_s_inode *inode)
{
    off_t begin = inode->data_offset;
    GArray *sm = (GArray *) inode->user_data;
    size_t i;

    for (i = 0; i < sm->len; i++)
    {
        struct sp_array *sp;

        sp = &g_array_index (sm, struct sp_array, i);
        sp->arch_offset = begin;
        begin += BLOCKSIZE * (sp->numbytes / BLOCKSIZE + sp->numbytes % BLOCKSIZE);
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
tar_skip_member (tar_super_t * archive, struct vfs_s_inode *inode)
{
    char save_typeflag;

    if (current_stat_info.skipped)
        return TRUE;

    save_typeflag = current_header->header.typeflag;

    tar_set_next_block_after (current_header);

    if (current_stat_info.is_sparse)
    {
        if (inode != NULL)
            inode->data_offset = BLOCKSIZE * tar_current_block_ordinal (archive);

        (void) tar_sparse_skip_file (archive, &current_stat_info);

        if (inode != NULL)
        {
            /* use vfs_s_inode::user_data to keep the sparse map */
            inode->user_data = current_stat_info.sparse_map;
            current_stat_info.sparse_map = NULL;

            tar_calc_sparse_offsets (inode);
        }
    }
    else if (save_typeflag != DIRTYPE)
    {
        if (inode != NULL && (save_typeflag == REGTYPE || save_typeflag == AREGTYPE))
            inode->data_offset = BLOCKSIZE * tar_current_block_ordinal (archive);

        return tar_skip_file (archive, current_stat_info.stat.st_size);
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Return the number of bytes comprising the space between @pointer through the end
 * of the current buffer of blocks. This space is available for filling with data,
 * or taking data from. @pointer is usually (but not always) the result previous
 * tar_find_next_block() call.
 */
static inline size_t
tar_available_space_after (const union block *pointer)
{
    return record_end->buffer - pointer->buffer;
}

/* --------------------------------------------------------------------------------------------- */

/** Check header checksum.
 */
static read_header
tar_checksum (const union block *header)
{
    size_t i;
    int unsigned_sum = 0;       /* the POSIX one :-) */
    int signed_sum = 0;         /* the Sun one :-( */
    int recorded_sum;
    int parsed_sum;
    const char *p = header->buffer;

    for (i = sizeof (*header); i-- != 0;)
    {
        unsigned_sum += (unsigned char) *p;
        signed_sum += (signed char) (*p++);
    }

    if (unsigned_sum == 0)
        return HEADER_ZERO_BLOCK;

    /* Adjust checksum to count the "chksum" field as blanks.  */
    for (i = sizeof (header->header.chksum); i-- != 0;)
    {
        unsigned_sum -= (unsigned char) header->header.chksum[i];
        signed_sum -= (signed char) (header->header.chksum[i]);
    }

    unsigned_sum += ' ' * sizeof (header->header.chksum);
    signed_sum += ' ' * sizeof (header->header.chksum);

    parsed_sum =
        tar_from_header (header->header.chksum, sizeof (header->header.chksum), NULL, 0,
                         INT_MAX, TRUE);
    if (parsed_sum < 0)
        return HEADER_FAILURE;

    recorded_sum = parsed_sum;

    if (unsigned_sum != recorded_sum && signed_sum != recorded_sum)
        return HEADER_FAILURE;

    return HEADER_SUCCESS;
}

/* --------------------------------------------------------------------------------------------- */

static void
tar_decode_header (union block *header, tar_super_t * arch)
{
    gboolean hbits = FALSE;

    current_stat_info.stat.st_mode = MODE_FROM_HEADER (header->header.mode, &hbits);

    /*
     * Try to determine the archive format.
     */
    if (arch->type == TAR_UNKNOWN)
    {
        if (strcmp (header->header.magic, TMAGIC) == 0)
        {
            if (header->star_header.prefix[130] == 0
                && is_octal_digit (header->star_header.atime[0])
                && header->star_header.atime[11] == ' '
                && is_octal_digit (header->star_header.ctime[0])
                && header->star_header.ctime[11] == ' ')
                arch->type = TAR_STAR;
            else if (current_stat_info.xhdr.buffer != NULL)
                arch->type = TAR_POSIX;
            else
                arch->type = TAR_USTAR;
        }
        else if (strcmp (header->buffer + offsetof (struct posix_header, magic), OLDGNU_MAGIC) == 0)
            arch->type = hbits ? TAR_OLDGNU : TAR_GNU;
        else
            arch->type = TAR_V7;
    }

    /*
     * typeflag on BSDI tar (pax) always '\000'
     */
    if (header->header.typeflag == '\000')
    {
        size_t len;

        if (header->header.name[sizeof (header->header.name) - 1] != '\0')
            len = sizeof (header->header.name);
        else
            len = strlen (header->header.name);

        if (len != 0 && IS_PATH_SEP (header->header.name[len - 1]))
            header->header.typeflag = DIRTYPE;
    }

    if (header->header.typeflag == GNUTYPE_DUMPDIR)
        if (arch->type == TAR_UNKNOWN)
            arch->type = TAR_GNU;
}

/* --------------------------------------------------------------------------------------------- */

static void
tar_fill_stat (struct vfs_s_super *archive, union block *header)
{
    tar_super_t *arch = TAR_SUPER (archive);

    /* Adjust current_stat_info.stat.st_mode because there are tar-files with
     * typeflag==SYMTYPE and S_ISLNK(mod)==0. I don't
     * know about the other modes but I think I cause no new
     * problem when I adjust them, too. -- Norbert.
     */
    if (header->header.typeflag == DIRTYPE || header->header.typeflag == GNUTYPE_DUMPDIR)
        current_stat_info.stat.st_mode |= S_IFDIR;
    else if (header->header.typeflag == SYMTYPE)
        current_stat_info.stat.st_mode |= S_IFLNK;
    else if (header->header.typeflag == CHRTYPE)
        current_stat_info.stat.st_mode |= S_IFCHR;
    else if (header->header.typeflag == BLKTYPE)
        current_stat_info.stat.st_mode |= S_IFBLK;
    else if (header->header.typeflag == FIFOTYPE)
        current_stat_info.stat.st_mode |= S_IFIFO;
    else
        current_stat_info.stat.st_mode |= S_IFREG;

    current_stat_info.stat.st_dev = 0;
#ifdef HAVE_STRUCT_STAT_ST_RDEV
    current_stat_info.stat.st_rdev = 0;
#endif

    switch (arch->type)
    {
    case TAR_USTAR:
    case TAR_POSIX:
    case TAR_GNU:
    case TAR_OLDGNU:
        /* *INDENT-OFF* */
        current_stat_info.stat.st_uid = *header->header.uname != '\0'
            ? (uid_t) vfs_finduid (header->header.uname)
            : UID_FROM_HEADER (header->header.uid);
        current_stat_info.stat.st_gid = *header->header.gname != '\0'
            ? (gid_t) vfs_findgid (header->header.gname)
            : GID_FROM_HEADER (header->header.gid);
        /* *INDENT-ON* */

        switch (header->header.typeflag)
        {
        case BLKTYPE:
        case CHRTYPE:
#ifdef HAVE_STRUCT_STAT_ST_RDEV
            current_stat_info.stat.st_rdev =
                makedev (MAJOR_FROM_HEADER (header->header.devmajor),
                         MINOR_FROM_HEADER (header->header.devminor));
#endif
            break;
        default:
            break;
        }
        break;

    default:
        current_stat_info.stat.st_uid = UID_FROM_HEADER (header->header.uid);
        current_stat_info.stat.st_gid = GID_FROM_HEADER (header->header.gid);
        break;
    }

    current_stat_info.atime.tv_nsec = 0;
    current_stat_info.mtime.tv_nsec = 0;
    current_stat_info.ctime.tv_nsec = 0;

    current_stat_info.mtime.tv_sec = TIME_FROM_HEADER (header->header.mtime);
    if (arch->type == TAR_GNU || arch->type == TAR_OLDGNU)
    {
        current_stat_info.atime.tv_sec = TIME_FROM_HEADER (header->oldgnu_header.atime);
        current_stat_info.ctime.tv_sec = TIME_FROM_HEADER (header->oldgnu_header.ctime);
    }
    else if (arch->type == TAR_STAR)
    {
        current_stat_info.atime.tv_sec = TIME_FROM_HEADER (header->star_header.atime);
        current_stat_info.ctime.tv_sec = TIME_FROM_HEADER (header->star_header.ctime);
    }
    else
        current_stat_info.atime = current_stat_info.ctime = start_time;

#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
    current_stat_info.stat.st_blksize = 8 * 1024;       /* FIXME */
#endif
    vfs_adjust_stat (&current_stat_info.stat);
}

/* --------------------------------------------------------------------------------------------- */

static void
tar_free_inode (struct vfs_class *me, struct vfs_s_inode *ino)
{
    (void) me;

    /* free sparse_map */
    if (ino->user_data != NULL)
        g_array_free ((GArray *) ino->user_data, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static read_header
tar_insert_entry (struct vfs_class *me, struct vfs_s_super *archive, union block *header,
                  struct vfs_s_inode **inode)
{
    char *p, *q;
    char *file_name = current_stat_info.file_name;
    char *link_name = current_stat_info.link_name;
    size_t len;
    struct vfs_s_inode *parent;
    struct vfs_s_entry *entry;

    p = strrchr (file_name, PATH_SEP);
    if (p == NULL)
    {
        len = strlen (file_name);
        p = file_name;
        q = file_name + len;    /* "" */
    }
    else
    {
        *(p++) = '\0';
        q = file_name;
    }

    parent = vfs_s_find_inode (me, archive, q, LINK_NO_FOLLOW, FL_MKDIR);
    if (parent == NULL)
        return HEADER_FAILURE;

    *inode = NULL;

    if (header->header.typeflag == LNKTYPE)
    {
        if (*link_name != '\0')
        {
            len = strlen (link_name);
            if (IS_PATH_SEP (link_name[len - 1]))
                link_name[len - 1] = '\0';

            *inode = vfs_s_find_inode (me, archive, link_name, LINK_NO_FOLLOW, FL_NONE);
        }

        if (*inode == NULL)
            return HEADER_FAILURE;
    }
    else
    {
        if (S_ISDIR (current_stat_info.stat.st_mode))
        {
            entry = VFS_SUBCLASS (me)->find_entry (me, parent, p, LINK_NO_FOLLOW, FL_NONE);
            if (entry != NULL)
                return HEADER_SUCCESS;
        }

        *inode = vfs_s_new_inode (me, archive, &current_stat_info.stat);
        /* assgin timestamps after decoding of extended headers */
        (*inode)->st.st_mtime = current_stat_info.mtime.tv_sec;
        (*inode)->st.st_atime = current_stat_info.atime.tv_sec;
        (*inode)->st.st_ctime = current_stat_info.ctime.tv_sec;

        if (link_name != NULL && *link_name != '\0')
            (*inode)->linkname = g_strdup (link_name);
    }

    entry = vfs_s_new_entry (me, p, *inode);
    vfs_s_insert_entry (me, parent, entry);

    return HEADER_SUCCESS;
}

/* --------------------------------------------------------------------------------------------- */

static read_header
tar_read_header (struct vfs_class *me, struct vfs_s_super *archive)
{
    tar_super_t *arch = TAR_SUPER (archive);
    union block *header;
    union block *next_long_name = NULL, *next_long_link = NULL;
    read_header status = HEADER_SUCCESS;

    while (TRUE)
    {
        header = tar_find_next_block (arch);
        current_header = header;
        if (header == NULL)
        {
            status = HEADER_END_OF_FILE;
            goto ret;
        }

        status = tar_checksum (header);
        if (status != HEADER_SUCCESS)
            goto ret;

        if (header->header.typeflag == LNKTYPE || header->header.typeflag == DIRTYPE)
            current_stat_info.stat.st_size = 0; /* Links 0 size on tape */
        else
        {
            current_stat_info.stat.st_size = OFF_FROM_HEADER (header->header.size);
            if (current_stat_info.stat.st_size < 0)
            {
                status = HEADER_FAILURE;
                goto ret;
            }
        }

        tar_decode_header (header, arch);
        tar_fill_stat (archive, header);

        if (header->header.typeflag == GNUTYPE_LONGNAME
            || header->header.typeflag == GNUTYPE_LONGLINK)
        {
            size_t name_size = current_stat_info.stat.st_size;
            size_t n;
            off_t size;
            union block *header_copy;
            char *bp;
            size_t written;

            if (arch->type == TAR_UNKNOWN)
                arch->type = TAR_GNU;

            n = name_size % BLOCKSIZE;
            size = name_size + BLOCKSIZE;
            if (n != 0)
                size += BLOCKSIZE - n;
            if ((off_t) name_size != current_stat_info.stat.st_size || size < (off_t) name_size)
            {
                message (D_ERROR, MSG_ERROR, _("Inconsistent tar archive"));
                status = HEADER_FAILURE;
                goto ret;
            }

            header_copy = g_malloc (size + 1);

            if (header->header.typeflag == GNUTYPE_LONGNAME)
            {
                g_free (next_long_name);
                next_long_name = header_copy;
            }
            else
            {
                g_free (next_long_link);
                next_long_link = header_copy;
            }

            tar_set_next_block_after (header);
            *header_copy = *header;
            bp = header_copy->buffer + BLOCKSIZE;

            for (size -= BLOCKSIZE; size > 0; size -= written)
            {
                union block *data_block;

                data_block = tar_find_next_block (arch);
                if (data_block == NULL)
                {
                    g_free (header_copy);
                    message (D_ERROR, MSG_ERROR, _("Unexpected EOF on archive file"));
                    status = HEADER_FAILURE;
                    goto ret;
                }

                written = tar_available_space_after (data_block);
                if ((off_t) written > size)
                    written = (size_t) size;

                memcpy (bp, data_block->buffer, written);
                bp += written;
                tar_set_next_block_after ((union block *) (data_block->buffer + written - 1));
            }

            *bp = '\0';
        }
        else if (header->header.typeflag == XHDTYPE || header->header.typeflag == SOLARIS_XHDTYPE)
        {
            if (arch->type == TAR_UNKNOWN)
                arch->type = TAR_POSIX;
            if (!tar_xheader_read
                (arch, &current_stat_info.xhdr, header, OFF_FROM_HEADER (header->header.size)))
            {
                message (D_ERROR, MSG_ERROR, _("Unexpected EOF on archive file"));
                status = HEADER_FAILURE;
                goto ret;
            }
        }
        else if (header->header.typeflag == XGLTYPE)
        {
            struct xheader xhdr;
            gboolean ok;

            if (arch->type == TAR_UNKNOWN)
                arch->type = TAR_POSIX;

            memset (&xhdr, 0, sizeof (xhdr));
            tar_xheader_read (arch, &xhdr, header, OFF_FROM_HEADER (header->header.size));
            ok = tar_xheader_decode_global (&xhdr);
            tar_xheader_destroy (&xhdr);

            if (!ok)
            {
                message (D_ERROR, MSG_ERROR, _("Inconsistent tar archive"));
                status = HEADER_FAILURE;
                goto ret;
            }
        }
        else
            break;
    }

    {
        static union block *recent_long_name = NULL, *recent_long_link = NULL;
        struct posix_header const *h = &header->header;
        char *file_name = NULL;
        char *link_name;
        struct vfs_s_inode *inode = NULL;

        g_free (recent_long_name);

        if (next_long_name != NULL)
        {
            file_name = g_strdup (next_long_name->buffer + BLOCKSIZE);
            recent_long_name = next_long_name;
        }
        else
        {
            /* Accept file names as specified by POSIX.1-1996 section 10.1.1. */
            char *s1 = NULL;
            char *s2;

            /* Don't parse TAR_OLDGNU incremental headers as POSIX prefixes. */
            if (h->prefix[0] != '\0' && strcmp (h->magic, TMAGIC) == 0)
                s1 = g_strndup (h->prefix, sizeof (h->prefix));

            s2 = g_strndup (h->name, sizeof (h->name));

            if (s1 == NULL)
                file_name = s2;
            else
            {
                file_name = g_strconcat (s1, PATH_SEP_STR, s2, (char *) NULL);
                g_free (s1);
                g_free (s2);
            }

            recent_long_name = NULL;
        }

        tar_assign_string_dup (&current_stat_info.orig_file_name, file_name);
        canonicalize_pathname (file_name);
        tar_assign_string (&current_stat_info.file_name, file_name);

        g_free (recent_long_link);

        if (next_long_link != NULL)
        {
            link_name = g_strdup (next_long_link->buffer + BLOCKSIZE);
            recent_long_link = next_long_link;
        }
        else
        {
            link_name = g_strndup (h->linkname, sizeof (h->linkname));
            recent_long_link = NULL;
        }

        tar_assign_string (&current_stat_info.link_name, link_name);

        if (current_stat_info.xhdr.buffer != NULL && !tar_xheader_decode (&current_stat_info))
        {
            status = HEADER_FAILURE;
            goto ret;
        }

        if (tar_sparse_member_p (arch, &current_stat_info))
        {
            if (!tar_sparse_fixup_header (arch, &current_stat_info))
            {
                status = HEADER_FAILURE;
                goto ret;
            }

            current_stat_info.is_sparse = TRUE;
        }
        else
        {
            current_stat_info.is_sparse = FALSE;

            if (((arch->type == TAR_GNU || arch->type == TAR_OLDGNU)
                 && current_header->header.typeflag == GNUTYPE_DUMPDIR)
                || current_stat_info.dumpdir != NULL)
                current_stat_info.is_dumpdir = TRUE;
        }

        status = tar_insert_entry (me, archive, header, &inode);
        if (status != HEADER_SUCCESS)
        {
            message (D_ERROR, MSG_ERROR, _("Inconsistent tar archive"));
            goto ret;
        }

        if (recent_long_name == next_long_name)
            recent_long_name = NULL;

        if (recent_long_link == next_long_link)
            recent_long_link = NULL;

        if (tar_skip_member (arch, inode))
            status = HEADER_SUCCESS;
        else if (hit_eof)
            status = HEADER_END_OF_FILE;
        else
            status = HEADER_FAILURE;
    }

  ret:
    g_free (next_long_name);
    g_free (next_long_link);

    return status;
}

/* --------------------------------------------------------------------------------------------- */

static struct vfs_s_super *
tar_new_archive (struct vfs_class *me)
{
    tar_super_t *arch;
    gint64 usec;

    arch = g_new0 (tar_super_t, 1);
    arch->base.me = me;
    arch->fd = -1;
    arch->type = TAR_UNKNOWN;

    /* Prepare global data needed for tar_find_next_block: */
    record_start_block = 0;
    arch->record_start = g_malloc (record_size);
    record_end = arch->record_start;    /* set up for 1st record = # 0 */
    current_block = arch->record_start;
    hit_eof = FALSE;

    /* time in microseconds */
    usec = g_get_real_time ();
    /* time in seconds and nanoseconds */
    start_time.tv_sec = usec / G_USEC_PER_SEC;
    start_time.tv_nsec = (usec % G_USEC_PER_SEC) * 1000;

    return VFS_SUPER (arch);
}

/* --------------------------------------------------------------------------------------------- */

static void
tar_free_archive (struct vfs_class *me, struct vfs_s_super *archive)
{
    tar_super_t *arch = TAR_SUPER (archive);

    (void) me;

    if (arch->fd != -1)
    {
        mc_close (arch->fd);
        arch->fd = -1;
    }

    g_free (arch->record_start);
    tar_stat_destroy (&current_stat_info);
}

/* --------------------------------------------------------------------------------------------- */

/* Returns status of the tar archive open */
static gboolean
tar_open_archive_int (struct vfs_class *me, const vfs_path_t * vpath, struct vfs_s_super *archive)
{
    tar_super_t *arch = TAR_SUPER (archive);
    int result, type;
    mode_t mode;
    struct vfs_s_inode *root;

    result = mc_open (vpath, O_RDONLY);
    if (result == -1)
    {
        message (D_ERROR, MSG_ERROR, _("Cannot open tar archive\n%s"), vfs_path_as_str (vpath));
        ERRNOR (ENOENT, FALSE);
    }

    archive->name = g_strdup (vfs_path_as_str (vpath));
    mc_stat (vpath, &arch->st);

    /* Find out the method to handle this tar file */
    type = get_compression_type (result, archive->name);
    if (type == COMPRESSION_NONE)
        mc_lseek (result, 0, SEEK_SET);
    else
    {
        char *s;
        vfs_path_t *tmp_vpath;

        mc_close (result);
        s = g_strconcat (archive->name, decompress_extension (type), (char *) NULL);
        tmp_vpath = vfs_path_from_str_flags (s, VPF_NO_CANON);
        result = mc_open (tmp_vpath, O_RDONLY);
        vfs_path_free (tmp_vpath, TRUE);
        if (result == -1)
            message (D_ERROR, MSG_ERROR, _("Cannot open tar archive\n%s"), s);
        g_free (s);
        if (result == -1)
        {
            MC_PTR_FREE (archive->name);
            ERRNOR (ENOENT, FALSE);
        }
    }

    arch->fd = result;
    mode = arch->st.st_mode & 07777;
    if (mode & 0400)
        mode |= 0100;
    if (mode & 0040)
        mode |= 0010;
    if (mode & 0004)
        mode |= 0001;
    mode |= S_IFDIR;

    root = vfs_s_new_inode (me, archive, &arch->st);
    root->st.st_mode = mode;
    root->data_offset = -1;
    root->st.st_nlink++;
    root->st.st_dev = VFS_SUBCLASS (me)->rdev++;

    archive->root = root;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Main loop for reading an archive.
 * Returns 0 on success, -1 on error.
 */
static int
tar_open_archive (struct vfs_s_super *archive, const vfs_path_t * vpath,
                  const vfs_path_element_t * vpath_element)
{
    tar_super_t *arch = TAR_SUPER (archive);
    /* Initial status at start of archive */
    read_header status = HEADER_STILL_UNREAD;

    /* Open for reading */
    if (!tar_open_archive_int (vpath_element->class, vpath, archive))
        return -1;

    tar_find_next_block (arch);

    while (TRUE)
    {
        read_header prev_status;

        prev_status = status;
        tar_stat_destroy (&current_stat_info);
        status = tar_read_header (vpath_element->class, archive);

        switch (status)
        {
        case HEADER_STILL_UNREAD:
            message (D_ERROR, MSG_ERROR, _("%s\ndoesn't look like a tar archive"),
                     vfs_path_as_str (vpath));
            return -1;

        case HEADER_SUCCESS:
            continue;

            /* Record of zeroes */
        case HEADER_ZERO_BLOCK:
            tar_set_next_block_after (current_header);
            (void) tar_read_header (vpath_element->class, archive);
            status = prev_status;
            continue;

        case HEADER_END_OF_FILE:
            break;

            /* Invalid header:
             * If the previous header was good, tell them that we are skipping bad ones. */
        case HEADER_FAILURE:
            tar_set_next_block_after (current_header);

            switch (prev_status)
            {
            case HEADER_STILL_UNREAD:
                message (D_ERROR, MSG_ERROR, _("%s\ndoesn't look like a tar archive"),
                         vfs_path_as_str (vpath));
                return -1;

            case HEADER_ZERO_BLOCK:
            case HEADER_SUCCESS:
                /* Skipping to next header. */
                break;          /* AB: FIXME */

            case HEADER_END_OF_FILE:
            case HEADER_FAILURE:
                /* We are in the middle of a cascade of errors.  */
                /* AB: FIXME: TODO: show an error message here */
                return -1;

            default:
                break;
            }
            continue;

        default:
            break;
        }
        break;
    }

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static void *
tar_super_check (const vfs_path_t * vpath)
{
    static struct stat stat_buf;
    int stat_result;

    stat_result = mc_stat (vpath, &stat_buf);

    return (stat_result != 0) ? NULL : &stat_buf;
}

/* --------------------------------------------------------------------------------------------- */

static int
tar_super_same (const vfs_path_element_t * vpath_element, struct vfs_s_super *parc,
                const vfs_path_t * vpath, void *cookie)
{
    struct stat *archive_stat = cookie; /* stat of main archive */

    (void) vpath_element;

    if (strcmp (parc->name, vfs_path_as_str (vpath)) != 0)
        return 0;

    /* Has the cached archive been changed on the disk? */
    if (parc != NULL && TAR_SUPER (parc)->st.st_mtime < archive_stat->st_mtime)
    {
        /* Yes, reload! */
        vfs_tarfs_ops->free ((vfsid) parc);
        vfs_rmstamp (vfs_tarfs_ops, (vfsid) parc);
        return 2;
    }
    /* Hasn't been modified, give it a new timeout */
    vfs_stamp (vfs_tarfs_ops, (vfsid) parc);
    return 1;
}

/* --------------------------------------------------------------------------------------------- */
/* Get indes of current data chunk in a sparse file.
 *
 * @param sparse_map map of the sparse file
 * @param offset offset in the sparse file
 *
 * @return an index of ahole or a data chunk
 *      positive: pointer to the data chunk;
 *      negative: pointer to the hole before data chunk;
 *      zero: pointer to the hole after last data chunk
 *
 *         +--------+--------+-------+--------+-----+-------+--------+---------+
 *         |  hole1 | chunk1 | hole2 | chunk2 | ... | holeN | chunkN | holeN+1 |
 *         +--------+--------+-------+--------+-----+-------+--------+---------+
 *             -1       1        -2      2             -N       N         0
 */

static ssize_t
tar_get_sparse_chunk_idx (const GArray * sparse_map, off_t offset)
{
    size_t k;

    for (k = 1; k <= sparse_map->len; k++)
    {
        const struct sp_array *chunk;

        chunk = &g_array_index (sparse_map, struct sp_array, k - 1);

        /* are we in the current chunk? */
        if (offset >= chunk->offset && offset < chunk->offset + chunk->numbytes)
            return k;

        /* are we before the current chunk? */
        if (offset < chunk->offset)
            return -k;
    }

    /* after the last chunk */
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static ssize_t
tar_read_sparse (vfs_file_handler_t * fh, char *buffer, size_t count)
{
    int fd = TAR_SUPER (fh->ino->super)->fd;
    const GArray *sm = (const GArray *) fh->ino->user_data;
    ssize_t chunk_idx;
    const struct sp_array *chunk;
    off_t remain;
    ssize_t res;

    chunk_idx = tar_get_sparse_chunk_idx (sm, fh->pos);
    if (chunk_idx > 0)
    {
        /* we are in the chunk -- read data until chunk end */
        chunk = &g_array_index (sm, struct sp_array, chunk_idx - 1);
        remain = MIN ((off_t) count, chunk->offset + chunk->numbytes - fh->pos);
        res = mc_read (fd, buffer, (size_t) remain);
    }
    else
    {
        if (chunk_idx == 0)
        {
            /* we are in the hole after last chunk -- return zeros until file end */
            remain = MIN ((off_t) count, fh->ino->st.st_size - fh->pos);
            /* FIXME: can remain be negative? */
            remain = MAX (remain, 0);
        }
        else                    /* chunk_idx < 0 */
        {
            /* we are in the hole -- return zeros until next chunk start */
            chunk = &g_array_index (sm, struct sp_array, -chunk_idx - 1);
            remain = MIN ((off_t) count, chunk->offset - fh->pos);
        }

        memset (buffer, 0, (size_t) remain);
        res = (ssize_t) remain;
    }

    return res;
}

/* --------------------------------------------------------------------------------------------- */

static off_t
tar_lseek_sparse (vfs_file_handler_t * fh, off_t offset)
{
    off_t saved_offset = offset;
    int fd = TAR_SUPER (fh->ino->super)->fd;
    const GArray *sm = (const GArray *) fh->ino->user_data;
    ssize_t chunk_idx;
    const struct sp_array *chunk;
    off_t res;

    chunk_idx = tar_get_sparse_chunk_idx (sm, offset);
    if (chunk_idx > 0)
    {
        /* we are in the chunk */

        chunk = &g_array_index (sm, struct sp_array, chunk_idx - 1);
        /* offset in the chunk */
        offset -= chunk->offset;
        /* offset in the archive */
        offset += chunk->arch_offset;
    }
    else
    {
        /* we are in the hole */

        /* we cannot lseek in hole so seek to the hole begin or end */
        switch (chunk_idx)
        {
        case -1:
            offset = fh->ino->data_offset;
            break;

        case 0:
            chunk = &g_array_index (sm, struct sp_array, sm->len - 1);
            /* FIXME: can we seek beyond tar archive EOF here? */
            offset = chunk->arch_offset + chunk->numbytes;
            break;

        default:
            chunk = &g_array_index (sm, struct sp_array, -chunk_idx - 1);
            offset = chunk->arch_offset + chunk->numbytes;
            break;
        }
    }

    res = mc_lseek (fd, offset, SEEK_SET);
    /* return requested offset in success */
    if (res == offset)
        res = saved_offset;

    return res;
}

/* --------------------------------------------------------------------------------------------- */

static ssize_t
tar_read (void *fh, char *buffer, size_t count)
{
    struct vfs_class *me = VFS_FILE_HANDLER_SUPER (fh)->me;
    vfs_file_handler_t *file = VFS_FILE_HANDLER (fh);
    int fd = TAR_SUPER (VFS_FILE_HANDLER_SUPER (fh))->fd;
    off_t begin = file->pos;
    ssize_t res;

    if (file->ino->user_data != NULL)
    {
        if (tar_lseek_sparse (file, begin) != begin)
            ERRNOR (EIO, -1);

        res = tar_read_sparse (file, buffer, count);
    }
    else
    {
        begin += file->ino->data_offset;

        if (mc_lseek (fd, begin, SEEK_SET) != begin)
            ERRNOR (EIO, -1);

        count = (size_t) MIN ((off_t) count, file->ino->st.st_size - file->pos);
        res = mc_read (fd, buffer, count);
    }

    if (res == -1)
        ERRNOR (errno, -1);

    file->pos += res;
    return res;
}

/* --------------------------------------------------------------------------------------------- */

static int
tar_fh_open (struct vfs_class *me, vfs_file_handler_t * fh, int flags, mode_t mode)
{
    (void) fh;
    (void) mode;

    if ((flags & O_ACCMODE) != O_RDONLY)
        ERRNOR (EROFS, -1);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
vfs_init_tarfs (void)
{
    /* FIXME: tarfs used own temp files */
    vfs_init_subclass (&tarfs_subclass, "tarfs", VFSF_READONLY, "utar");
    vfs_tarfs_ops->read = tar_read;
    vfs_tarfs_ops->setctl = NULL;
    tarfs_subclass.archive_check = tar_super_check;
    tarfs_subclass.archive_same = tar_super_same;
    tarfs_subclass.new_archive = tar_new_archive;
    tarfs_subclass.open_archive = tar_open_archive;
    tarfs_subclass.free_archive = tar_free_archive;
    tarfs_subclass.free_inode = tar_free_inode;
    tarfs_subclass.fh_open = tar_fh_open;
    vfs_register_class (vfs_tarfs_ops);

    tar_base64_init ();
}

/* --------------------------------------------------------------------------------------------- */
