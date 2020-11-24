/*
   Virtual File System: GNU Tar file system.

   Copyright (C) 1995-2020
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
 *
 * Namespace: init_tarfs
 */

#include <config.h>
#include <sys/types.h>
#include <errno.h>
#include <ctype.h>

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
#include "lib/vfs/xdirentry.h"
#include "lib/vfs/gc.h"         /* vfs_rmstamp */

#include "tar.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define TAR_SUPER(super) ((tar_super_t *) (super))


/* tar Header Block, from POSIX 1003.1-1990.  */

/* The magic field is filled with this if uname and gname are valid. */
#define TMAGIC "ustar"          /* ustar and a null */

#define XHDTYPE 'x'             /* Extended header referring to the next file in the archive */
#define XGLTYPE 'g'             /* Global extended header */

/* Values used in typeflag field.  */
#define LNKTYPE  '1'            /* link */
#define SYMTYPE  '2'            /* symbolic link */
#define CHRTYPE  '3'            /* character special */
#define BLKTYPE  '4'            /* block special */
#define DIRTYPE  '5'            /* directory */
#define FIFOTYPE '6'            /* FIFO special */


/* tar Header Block, GNU extensions.  */

/* *BEWARE* *BEWARE* *BEWARE* that the following information is still
   boiling, and may change.  Even if the OLDGNU format description should be
   accurate, the so-called GNU format is not yet fully decided.  It is
   surely meant to use only extensions allowed by POSIX, but the sketch
   below repeats some ugliness from the OLDGNU format, which should rather
   go away.  Sparse files should be saved in such a way that they do *not*
   require two passes at archive creation time.  Huge files get some POSIX
   fields to overflow, alternate solutions have to be sought for this.  */


/* Sparse files are not supported in POSIX ustar format.  For sparse files
   with a POSIX header, a GNU extra header is provided which holds overall
   sparse information and a few sparse descriptors.  When an old GNU header
   replaces both the POSIX header and the GNU extra header, it holds some
   sparse descriptors too.  Whether POSIX or not, if more sparse descriptors
   are still needed, they are put into as many successive sparse headers as
   necessary.  The following constants tell how many sparse descriptors fit
   in each kind of header able to hold them.  */

#define SPARSES_IN_EXTRA_HEADER  16
#define SPARSES_IN_OLDGNU_HEADER 4
#define SPARSES_IN_SPARSE_HEADER 21

/* OLDGNU_MAGIC uses both magic and version fields, which are contiguous.
   Found in an archive, it indicates an old GNU header format, which will be
   hopefully become obsolescent.  With OLDGNU_MAGIC, uname and gname are
   valid, though the header is not truly POSIX conforming.  */
#define OLDGNU_MAGIC "ustar  "  /* 7 chars and a null */

/* The standards committee allows only capital A through capital Z for user-defined expansion.  */

/* This is a dir entry that contains the names of files that were in the
   dir at the time the dump was made.  */
#define GNUTYPE_DUMPDIR 'D'

/* Identifies the *next* file on the tape as having a long linkname.  */
#define GNUTYPE_LONGLINK 'K'

/* Identifies the *next* file on the tape as having a long name.  */
#define GNUTYPE_LONGNAME 'L'


/* tar Header Block, overall structure.  */

/* tar files are made in basic blocks of this size.  */
#define BLOCKSIZE 512


#define isodigit(c) ( ((c) >= '0') && ((c) <= '7') )

/*** file scope type declarations ****************************************************************/

/* *INDENT-OFF* */

/* POSIX header */
struct posix_header
{                               /* byte offset */
    char name[100];             /*   0 */
    char mode[8];               /* 100 */
    char uid[8];                /* 108 */
    char gid[8];                /* 116 */
    char size[12];              /* 124 */
    char mtime[12];             /* 136 */
    char chksum[8];             /* 148 */
    char typeflag;              /* 156 */
    char linkname[100];         /* 157 */
    char magic[6];              /* 257 */
    char version[2];            /* 263 */
    char uname[32];             /* 265 */
    char gname[32];             /* 297 */
    char devmajor[8];           /* 329 */
    char devminor[8];           /* 337 */
    char prefix[155];           /* 345 */
                                /* 500 */
};

/* Descriptor for a single file hole */
struct sparse
{                               /* byte offset */
    /* cppcheck-suppress unusedStructMember */
    char offset[12];            /*   0 */
    /* cppcheck-suppress unusedStructMember */
    char numbytes[12];          /*  12 */
                                /*  24 */
};

/* The GNU extra header contains some information GNU tar needs, but not
   foreseen in POSIX header format.  It is only used after a POSIX header
   (and never with old GNU headers), and immediately follows this POSIX
   header, when typeflag is a letter rather than a digit, so signaling a GNU
   extension.  */
struct extra_header
{                               /* byte offset */
    char atime[12];             /*   0 */
    char ctime[12];             /*  12 */
    char offset[12];            /*  24 */
    char realsize[12];          /*  36 */
    char longnames[4];          /*  48 */
    char unused_pad1[68];       /*  52 */
    struct sparse sp[SPARSES_IN_EXTRA_HEADER];
                                /* 120 */
    char isextended;            /* 504 */
                                /* 505 */
};

/* Extension header for sparse files, used immediately after the GNU extra
   header, and used only if all sparse information cannot fit into that
   extra header.  There might even be many such extension headers, one after
   the other, until all sparse information has been recorded.  */
struct sparse_header
{                               /* byte offset */
    struct sparse sp[SPARSES_IN_SPARSE_HEADER];
                                /*   0 */
    char isextended;            /* 504 */
                                /* 505 */
};

/* The old GNU format header conflicts with POSIX format in such a way that
   POSIX archives may fool old GNU tar's, and POSIX tar's might well be
   fooled by old GNU tar archives.  An old GNU format header uses the space
   used by the prefix field in a POSIX header, and cumulates information
   normally found in a GNU extra header.  With an old GNU tar header, we
   never see any POSIX header nor GNU extra header.  Supplementary sparse
   headers are allowed, however.  */
struct oldgnu_header
{                               /* byte offset */
    char unused_pad1[345];      /*   0 */
    char atime[12];             /* 345 */
    char ctime[12];             /* 357 */
    char offset[12];            /* 369 */
    char longnames[4];          /* 381 */
    char unused_pad2;           /* 385 */
    struct sparse sp[SPARSES_IN_OLDGNU_HEADER];
                                /* 386 */
    char isextended;            /* 482 */
    char realsize[12];          /* 483 */
                                /* 495 */
};

/* *INDENT-ON* */

/* tar Header Block, overall structure */
union block
{
    char buffer[BLOCKSIZE];
    struct posix_header header;
    struct extra_header extra_header;
    struct oldgnu_header oldgnu_header;
    struct sparse_header sparse_header;
};

enum archive_format
{
    TAR_UNKNOWN = 0,
    TAR_V7,
    TAR_USTAR,
    TAR_POSIX,
    TAR_GNU
};

typedef enum
{
    STATUS_BADCHECKSUM,
    STATUS_SUCCESS,
    STATUS_EOFMARK,
    STATUS_EOF
} ReadStatus;

typedef struct
{
    struct vfs_s_super base;    /* base class */

    int fd;
    struct stat st;
    enum archive_format type;   /* Type of the archive */
} tar_super_t;

/*** file scope variables ************************************************************************/

static struct vfs_s_subclass tarfs_subclass;
static struct vfs_class *vfs_tarfs_ops = VFS_CLASS (&tarfs_subclass);

/* As we open one archive at a time, it is safe to have this static */
static off_t current_tar_position = 0;

static union block block_buf;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Quick and dirty octal conversion.
 *
 * Result is -1 if the field is invalid (all blank, or nonoctal).
 */
static long
tar_from_oct (int digs, const char *where)
{
    long value;

    while (isspace ((unsigned char) *where))
    {                           /* Skip spaces */
        where++;
        if (--digs <= 0)
            return -1;          /* All blank field */
    }
    value = 0;
    while (digs > 0 && isodigit (*where))
    {                           /* Scan till nonoctal */
        value = (value << 3) | (*where++ - '0');
        --digs;
    }

    if (digs > 0 && *where && !isspace ((unsigned char) *where))
        return -1;              /* Ended on non-space/nul */

    return value;
}

/* --------------------------------------------------------------------------------------------- */

static struct vfs_s_super *
tar_new_archive (struct vfs_class *me)
{
    tar_super_t *arch;

    arch = g_new0 (tar_super_t, 1);
    arch->base.me = me;
    arch->fd = -1;
    arch->type = TAR_UNKNOWN;

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
}

/* --------------------------------------------------------------------------------------------- */

/* Returns fd of the open tar file */
static int
tar_open_archive_int (struct vfs_class *me, const vfs_path_t * vpath, struct vfs_s_super *archive)
{
    int result, type;
    tar_super_t *arch;
    mode_t mode;
    struct vfs_s_inode *root;

    result = mc_open (vpath, O_RDONLY);
    if (result == -1)
    {
        message (D_ERROR, MSG_ERROR, _("Cannot open tar archive\n%s"), vfs_path_as_str (vpath));
        ERRNOR (ENOENT, -1);
    }

    archive->name = g_strdup (vfs_path_as_str (vpath));
    arch = TAR_SUPER (archive);
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
        vfs_path_free (tmp_vpath);
        if (result == -1)
            message (D_ERROR, MSG_ERROR, _("Cannot open tar archive\n%s"), s);
        g_free (s);
        if (result == -1)
        {
            MC_PTR_FREE (archive->name);
            ERRNOR (ENOENT, -1);
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

    return result;
}

/* --------------------------------------------------------------------------------------------- */

static union block *
tar_get_next_block (struct vfs_s_super *archive, int tard)
{
    int n;

    (void) archive;

    n = mc_read (tard, block_buf.buffer, sizeof (block_buf.buffer));
    if (n != sizeof (block_buf.buffer))
        return NULL;            /* An error has occurred */
    current_tar_position += sizeof (block_buf.buffer);
    return &block_buf;
}

/* --------------------------------------------------------------------------------------------- */

static void
tar_skip_n_records (struct vfs_s_super *archive, int tard, size_t n)
{
    (void) archive;

    mc_lseek (tard, n * sizeof (block_buf.buffer), SEEK_CUR);
    current_tar_position += n * sizeof (block_buf.buffer);
}

/* --------------------------------------------------------------------------------------------- */

static ReadStatus
tar_checksum (const union block *header)
{
    long recsum;
    long signed_sum = 0;
    long sum = 0;
    int i;
    const char *p = header->buffer;

    recsum = tar_from_oct (8, header->header.chksum);

    for (i = sizeof (*header); --i >= 0;)
    {
        /*
         * We can't use unsigned char here because of old compilers,
         * e.g. V7.
         */
        signed_sum += *p;
        sum += 0xFF & *p++;
    }

    /* Adjust checksum to count the "chksum" field as blanks. */
    for (i = sizeof (header->header.chksum); --i >= 0;)
    {
        sum -= 0xFF & header->header.chksum[i];
        signed_sum -= (char) header->header.chksum[i];
    }

    sum += ' ' * sizeof (header->header.chksum);
    signed_sum += ' ' * sizeof (header->header.chksum);

    /*
     * This is a zeroed block... whole block is 0's except
     * for the 8 blanks we faked for the checksum field.
     */
    if (sum == 8 * ' ')
        return STATUS_EOFMARK;

    if (sum != recsum && signed_sum != recsum)
        return STATUS_BADCHECKSUM;

    return STATUS_SUCCESS;
}

/* --------------------------------------------------------------------------------------------- */

static size_t
tar_decode_header (union block *header, tar_super_t * arch)
{
    size_t size;

    /*
     * Try to determine the archive format.
     */
    if (arch->type == TAR_UNKNOWN)
    {
        if (strcmp (header->header.magic, TMAGIC) == 0)
        {
            if (header->header.typeflag == XGLTYPE)
                arch->type = TAR_POSIX;
            else
                arch->type = TAR_USTAR;
        }
        else if (strcmp (header->header.magic, OLDGNU_MAGIC) == 0)
            arch->type = TAR_GNU;
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

    /*
     * Good block.  Decode file size and return.
     */
    if (header->header.typeflag == LNKTYPE || header->header.typeflag == DIRTYPE)
        size = 0;               /* Links 0 size on tape */
    else
        size = tar_from_oct (1 + 12, header->header.size);

    if (header->header.typeflag == GNUTYPE_DUMPDIR)
        if (arch->type == TAR_UNKNOWN)
            arch->type = TAR_GNU;

    return size;
}

/* --------------------------------------------------------------------------------------------- */

static void
tar_fill_stat (struct vfs_s_super *archive, struct stat *st, union block *header, size_t h_size)
{
    tar_super_t *arch = TAR_SUPER (archive);

    st->st_mode = tar_from_oct (8, header->header.mode);

    /* Adjust st->st_mode because there are tar-files with
     * typeflag==SYMTYPE and S_ISLNK(mod)==0. I don't 
     * know about the other modes but I think I cause no new
     * problem when I adjust them, too. -- Norbert.
     */
    if (header->header.typeflag == DIRTYPE || header->header.typeflag == GNUTYPE_DUMPDIR)
        st->st_mode |= S_IFDIR;
    else if (header->header.typeflag == SYMTYPE)
        st->st_mode |= S_IFLNK;
    else if (header->header.typeflag == CHRTYPE)
        st->st_mode |= S_IFCHR;
    else if (header->header.typeflag == BLKTYPE)
        st->st_mode |= S_IFBLK;
    else if (header->header.typeflag == FIFOTYPE)
        st->st_mode |= S_IFIFO;
    else
        st->st_mode |= S_IFREG;

    st->st_dev = 0;
#ifdef HAVE_STRUCT_STAT_ST_RDEV
    st->st_rdev = 0;
#endif

    switch (arch->type)
    {
    case TAR_USTAR:
    case TAR_POSIX:
    case TAR_GNU:
        /* *INDENT-OFF* */
        st->st_uid = *header->header.uname
            ? vfs_finduid (header->header.uname)
            : tar_from_oct (8, header->header.uid);
        st->st_gid = *header->header.gname
            ? vfs_findgid (header->header.gname)
            : tar_from_oct (8,header->header.gid);
        /* *INDENT-ON* */

        switch (header->header.typeflag)
        {
        case BLKTYPE:
        case CHRTYPE:
#ifdef HAVE_STRUCT_STAT_ST_RDEV
            st->st_rdev =
                makedev (tar_from_oct (8, header->header.devmajor),
                         tar_from_oct (8, header->header.devminor));
#endif
            break;
        default:
            break;
        }
        break;

    default:
        st->st_uid = tar_from_oct (8, header->header.uid);
        st->st_gid = tar_from_oct (8, header->header.gid);
        break;
    }

    st->st_size = h_size;
#ifdef HAVE_STRUCT_STAT_ST_MTIM
    st->st_atim.tv_nsec = st->st_mtim.tv_nsec = st->st_ctim.tv_nsec = 0;
#endif
    st->st_mtime = tar_from_oct (1 + 12, header->header.mtime);
    st->st_atime = 0;
    st->st_ctime = 0;
    if (arch->type == TAR_GNU)
    {
        st->st_atime = tar_from_oct (1 + 12, header->oldgnu_header.atime);
        st->st_ctime = tar_from_oct (1 + 12, header->oldgnu_header.ctime);
    }

#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
    st->st_blksize = 8 * 1024;  /* FIXME */
#endif
    vfs_adjust_stat (st);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Return 1 for success, 0 if the checksum is bad, EOF on eof,
 * 2 for a block full of zeros (EOF marker).
 *
 */
static ReadStatus
tar_read_header (struct vfs_class *me, struct vfs_s_super *archive, int tard, size_t * h_size)
{
    tar_super_t *arch = TAR_SUPER (archive);
    ReadStatus checksum_status;
    union block *header;
    static char *next_long_name = NULL, *next_long_link = NULL;

    while (TRUE)
    {
        header = tar_get_next_block (archive, tard);
        if (header == NULL)
            return STATUS_EOF;

        checksum_status = tar_checksum (header);
        if (checksum_status != STATUS_SUCCESS)
            return checksum_status;

        *h_size = tar_decode_header (header, arch);

        /* Skip over pax extended header and global extended header records. */
        if (header->header.typeflag == XHDTYPE || header->header.typeflag == XGLTYPE)
        {
            if (arch->type == TAR_UNKNOWN)
                arch->type = TAR_POSIX;
            return STATUS_SUCCESS;
        }

        if (header->header.typeflag == GNUTYPE_LONGNAME
            || header->header.typeflag == GNUTYPE_LONGLINK)
        {
            char **longp;
            char *bp, *data;
            off_t size;
            size_t written;

            if (arch->type == TAR_UNKNOWN)
                arch->type = TAR_GNU;

            if (*h_size > MC_MAXPATHLEN)
            {
                message (D_ERROR, MSG_ERROR, _("Inconsistent tar archive"));
                return STATUS_BADCHECKSUM;
            }

            longp = header->header.typeflag == GNUTYPE_LONGNAME ? &next_long_name : &next_long_link;

            g_free (*longp);
            bp = *longp = g_malloc (*h_size + 1);

            for (size = *h_size; size > 0; size -= written)
            {
                data = tar_get_next_block (archive, tard)->buffer;
                if (data == NULL)
                {
                    MC_PTR_FREE (*longp);
                    message (D_ERROR, MSG_ERROR, _("Unexpected EOF on archive file"));
                    return STATUS_BADCHECKSUM;
                }
                written = BLOCKSIZE;
                if ((off_t) written > size)
                    written = (size_t) size;

                memcpy (bp, data, written);
                bp += written;
            }

            if (bp - *longp == MC_MAXPATHLEN && bp[-1] != '\0')
            {
                MC_PTR_FREE (*longp);
                message (D_ERROR, MSG_ERROR, _("Inconsistent tar archive"));
                return STATUS_BADCHECKSUM;
            }

            *bp = '\0';
        }
        else
            break;
    }

    {
        struct stat st;
        struct vfs_s_entry *entry;
        struct vfs_s_inode *inode = NULL, *parent;
        off_t data_position;
        char *p, *q;
        size_t len;
        char *current_file_name, *current_link_name;

        current_link_name =
            next_long_link != NULL ? next_long_link : g_strndup (header->header.linkname,
                                                                 sizeof (header->header.linkname));
        len = strlen (current_link_name);
        if (len > 1 && IS_PATH_SEP (current_link_name[len - 1]))
            current_link_name[len - 1] = '\0';

        current_file_name = NULL;
        switch (arch->type)
        {
        case TAR_USTAR:
        case TAR_POSIX:
            /* The ustar archive format supports pathnames of upto 256
             * characters in length. This is achieved by concatenating
             * the contents of the 'prefix' and 'name' fields like
             * this:
             *
             *   prefix + path_separator + name
             *
             * If the 'prefix' field contains an empty string i.e. its
             * first characters is '\0' the prefix field is ignored.
             */
            if (header->header.prefix[0] != '\0')
            {
                char *temp_name, *temp_prefix;

                temp_name = g_strndup (header->header.name, sizeof (header->header.name));
                temp_prefix = g_strndup (header->header.prefix, sizeof (header->header.prefix));
                current_file_name = g_strconcat (temp_prefix, PATH_SEP_STR,
                                                 temp_name, (char *) NULL);
                g_free (temp_name);
                g_free (temp_prefix);
            }
            break;
        case TAR_GNU:
            if (next_long_name != NULL)
                current_file_name = next_long_name;
            break;
        default:
            break;
        }

        if (current_file_name == NULL)
        {
            if (next_long_name != NULL)
                current_file_name = g_strdup (next_long_name);
            else
                current_file_name = g_strndup (header->header.name, sizeof (header->header.name));
        }

        canonicalize_pathname (current_file_name);
        len = strlen (current_file_name);

        data_position = current_tar_position;

        p = strrchr (current_file_name, PATH_SEP);
        if (p == NULL)
        {
            p = current_file_name;
            q = current_file_name + len;        /* "" */
        }
        else
        {
            *(p++) = '\0';
            q = current_file_name;
        }

        parent = vfs_s_find_inode (me, archive, q, LINK_NO_FOLLOW, FL_MKDIR);
        if (parent == NULL)
        {
            message (D_ERROR, MSG_ERROR, _("Inconsistent tar archive"));
            return STATUS_BADCHECKSUM;
        }

        if (header->header.typeflag == LNKTYPE)
        {
            inode = vfs_s_find_inode (me, archive, current_link_name, LINK_NO_FOLLOW, FL_NONE);
            if (inode == NULL)
                message (D_ERROR, MSG_ERROR, _("Inconsistent tar archive"));
            else
            {
                entry = vfs_s_new_entry (me, p, inode);
                vfs_s_insert_entry (me, parent, entry);
                g_free (current_link_name);
                goto done;
            }
        }

        tar_fill_stat (archive, &st, header, *h_size);
        if (S_ISDIR (st.st_mode))
        {
            entry = VFS_SUBCLASS (me)->find_entry (me, parent, p, LINK_NO_FOLLOW, FL_NONE);
            if (entry != NULL)
                goto done;
        }

        inode = vfs_s_new_inode (me, archive, &st);
        inode->data_offset = data_position;

        if (*current_link_name != '\0')
            inode->linkname = current_link_name;
        else if (current_link_name != next_long_link)
            g_free (current_link_name);

        entry = vfs_s_new_entry (me, p, inode);
        vfs_s_insert_entry (me, parent, entry);
        g_free (current_file_name);

      done:
        next_long_link = next_long_name = NULL;

        if (arch->type == TAR_GNU && header->oldgnu_header.isextended)
        {
            while (tar_get_next_block (archive, tard)->sparse_header.isextended != 0)
                ;

            if (inode != NULL)
                inode->data_offset = current_tar_position;
        }
        return STATUS_SUCCESS;
    }
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
    /* Initial status at start of archive */
    ReadStatus status = STATUS_EOFMARK;
    int tard;

    current_tar_position = 0;
    /* Open for reading */
    tard = tar_open_archive_int (vpath_element->class, vpath, archive);
    if (tard == -1)
        return -1;

    while (TRUE)
    {
        size_t h_size = 0;
        ReadStatus prev_status = status;

        status = tar_read_header (vpath_element->class, archive, tard, &h_size);

        switch (status)
        {
        case STATUS_SUCCESS:
            tar_skip_n_records (archive, tard, (h_size + BLOCKSIZE - 1) / BLOCKSIZE);
            continue;

            /*
             * Invalid header:
             *
             * If the previous header was good, tell them
             * that we are skipping bad ones.
             */
        case STATUS_BADCHECKSUM:
            switch (prev_status)
            {
                /* Error on first block */
            case STATUS_EOFMARK:
                {
                    message (D_ERROR, MSG_ERROR, _("%s\ndoesn't look like a tar archive."),
                             vfs_path_as_str (vpath));
                    MC_FALLTHROUGH;

                    /* Error after header rec */
                }
            case STATUS_SUCCESS:
                /* Error after error */

            case STATUS_BADCHECKSUM:
                return -1;

            case STATUS_EOF:
                return 0;

            default:
                break;
            }
            MC_FALLTHROUGH;

            /* Record of zeroes */
        case STATUS_EOFMARK:   /* If error after 0's */
            MC_FALLTHROUGH;
            /* exit from loop */
        case STATUS_EOF:       /* End of archive */
            break;
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

static ssize_t
tar_read (void *fh, char *buffer, size_t count)
{
    struct vfs_class *me = VFS_FILE_HANDLER_SUPER (fh)->me;
    vfs_file_handler_t *file = VFS_FILE_HANDLER (fh);
    off_t begin = file->ino->data_offset;
    int fd = TAR_SUPER (VFS_FILE_HANDLER_SUPER (fh))->fd;
    ssize_t res;

    if (mc_lseek (fd, begin + file->pos, SEEK_SET) != begin + file->pos)
        ERRNOR (EIO, -1);

    count = MIN (count, (size_t) (file->ino->st.st_size - file->pos));

    res = mc_read (fd, buffer, count);
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
    tarfs_subclass.fh_open = tar_fh_open;
    vfs_register_class (vfs_tarfs_ops);
}

/* --------------------------------------------------------------------------------------------- */
