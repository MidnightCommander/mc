/*
   Virtual File System: GNU Tar file system.

   Copyright (C) 1995-2023
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

#include <ctype.h>              /* isspace() */
#include <errno.h>
#include <inttypes.h>           /* uintmax_t */
#include <limits.h>             /* CHAR_BIT, INT_MAX, etc */
#include <stdint.h>             /* UINTMAX_MAX, etc */
#include <sys/types.h>
#include <sys/stat.h>
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

/* Log base 2 of common values. */
#define LG_8 3
#define LG_64 6
#define LG_256 8

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

/* These macros work even on ones'-complement hosts (!).
   The extra casts work around some compiler bugs. */
#define TYPE_SIGNED(t) (!((t) 0 < (t) (-1)))
#define TYPE_MINIMUM(t) (TYPE_SIGNED (t) ? ~(t) 0 << (sizeof (t) * CHAR_BIT - 1) : (t) 0)
#define TYPE_MAXIMUM(t) (~(t) 0 - TYPE_MINIMUM (t))

#define GID_FROM_HEADER(where) gid_from_header (where, sizeof (where))
#define MAJOR_FROM_HEADER(where) major_from_header (where, sizeof (where))
#define MINOR_FROM_HEADER(where) minor_from_header (where, sizeof (where))
#define MODE_FROM_HEADER(where,hbits) mode_from_header (where, sizeof (where), hbits)
#define OFF_FROM_HEADER(where) off_from_header (where, sizeof (where))
#define TIME_FROM_HEADER(where) time_from_header (where, sizeof (where))
#define UID_FROM_HEADER(where) uid_from_header (where, sizeof (where))
#define UINTMAX_FROM_HEADER(where) uintmax_from_header (where, sizeof (where))

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
    struct oldgnu_header oldgnu_header;
    struct sparse_header sparse_header;
};

enum archive_format
{
    TAR_UNKNOWN = 0,            /* format to be decided later */
    TAR_V7,                     /* old V7 tar format */
    TAR_OLDGNU,                 /* GNU format as per before tar 1.12 */
    TAR_USTAR,                  /* POSIX.1-1988 (ustar) format */
    TAR_POSIX,                  /* POSIX.1-2001 format */
    TAR_GNU                     /* Almost same as TAR_OLDGNU */
};

typedef enum
{
    HEADER_STILL_UNREAD,        /* for when read_header has not been called */
    HEADER_SUCCESS,             /* header successfully read and checksummed */
    HEADER_ZERO_BLOCK,          /* zero block where header expected */
    HEADER_END_OF_FILE,         /* true end of file while header expected */
    HEADER_FAILURE              /* ill-formed header, or bad checksum */
} read_header;

typedef struct
{
    struct vfs_s_super base;    /* base class */

    int fd;
    struct stat st;
    enum archive_format type;   /* Type of the archive */
} tar_super_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* Base 64 digits; see RFC 2045 Table 1.  */
static char const base_64_digits[64] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
};

/* Table of base 64 digit values indexed by unsigned chars.
   The value is 64 for unsigned chars that are not base 64 digits.  */
static char base64_map[1 + (unsigned char) (-1)];

static struct vfs_s_subclass tarfs_subclass;
static struct vfs_class *vfs_tarfs_ops = VFS_CLASS (&tarfs_subclass);

/* As we open one archive at a time, it is safe to have this static */
static off_t current_tar_position = 0;

static union block block_buf;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
tar_base64_init (void)
{
    size_t i;

    memset (base64_map, 64, sizeof base64_map);
    for (i = 0; i < 64; i++)
        base64_map[(int) base_64_digits[i]] = i;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Represent @n using a signed integer I such that (uintmax_t) I == @n.
   With a good optimizing compiler, this is equivalent to (intmax_t) i
   and requires zero machine instructions.  */
#if !(UINTMAX_MAX / 2 <= INTMAX_MAX)
#error "tar_represent_uintmax() returns intmax_t to represent uintmax_t"
#endif
static inline intmax_t
tar_represent_uintmax (uintmax_t n)
{
    if (n <= INTMAX_MAX)
        return n;

    {
        /* Avoid signed integer overflow on picky platforms.  */
        intmax_t nd = n - INTMAX_MIN;
        return nd + INTMAX_MIN;
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Convert buffer at @where0 of size @digs from external format to intmax_t.
 * @digs must be positive.
 * If @type is non-NULL, data are of type @type.
 * The buffer must represent a value in the range -@minval through @maxval;
 * if the mathematically correct result V would be greater than INTMAX_MAX,
 * return a negative integer V such that (uintmax_t) V yields the correct result.
 * If @octal_only, allow only octal numbers instead of the other GNU extensions.
 *
 * Result is -1 if the field is invalid.
 */
#if !(INTMAX_MAX <= UINTMAX_MAX && - (INTMAX_MIN + 1) <= UINTMAX_MAX)
#error "tar_from_header() internally represents intmax_t as uintmax_t + sign"
#endif
#if !(UINTMAX_MAX / 2 <= INTMAX_MAX)
#error "tar_from_header() returns intmax_t to represent uintmax_t"
#endif
static intmax_t
tar_from_header (const char *where0, size_t digs, char const *type, intmax_t minval,
                 uintmax_t maxval, gboolean octal_only)
{
    uintmax_t value = 0;
    uintmax_t uminval = minval;
    uintmax_t minus_minval = -uminval;
    const char *where = where0;
    char const *lim = where + digs;
    gboolean negative = FALSE;

    /* Accommodate buggy tar of unknown vintage, which outputs leading
       NUL if the previous field overflows. */
    if (*where == '\0')
        where++;

    /* Accommodate older tars, which output leading spaces. */
    while (TRUE)
    {
        if (where == lim)
            return (-1);

        if (!isspace ((unsigned char) *where))
            break;

        where++;
    }

    if (isodigit (*where))
    {
        char const *where1 = where;
        gboolean overflow = FALSE;

        while (TRUE)
        {
            value += *where++ - '0';
            if (where == lim || !isodigit (*where))
                break;
            overflow |= value != (value << LG_8 >> LG_8);
            value <<= LG_8;
        }

        /* Parse the output of older, unportable tars, which generate
           negative values in two's complement octal. If the leading
           nonzero digit is 1, we can't recover the original value
           reliably; so do this only if the digit is 2 or more. This
           catches the common case of 32-bit negative time stamps. */
        if ((overflow || maxval < value) && *where1 >= 2 && type != NULL)
        {
            /* Compute the negative of the input value, assuming two's complement. */
            int digit;

            digit = (*where1 - '0') | 4;
            overflow = FALSE;
            value = 0;
            where = where1;

            while (TRUE)
            {
                value += 7 - digit;
                where++;
                if (where == lim || !isodigit (*where))
                    break;
                digit = *where - '0';
                overflow |= value != (value << LG_8 >> LG_8);
                value <<= LG_8;
            }

            value++;
            overflow |= value == 0;

            if (!overflow && value <= minus_minval)
                negative = TRUE;
        }

        if (overflow)
            return (-1);
    }
    else if (octal_only)
    {
        /* Suppress the following extensions. */
    }
    else if (*where == '-' || *where == '+')
    {
        /* Parse base-64 output produced only by tar test versions
           1.13.6 (1999-08-11) through 1.13.11 (1999-08-23).
           Support for this will be withdrawn in future tar releases. */
        int dig;

        negative = *where++ == '-';

        while (where != lim && (dig = base64_map[(unsigned char) *where]) < 64)
        {
            if (value << LG_64 >> LG_64 != value)
                return (-1);
            value = (value << LG_64) | dig;
            where++;
        }
    }
    else if (where <= lim - 2 && (*where == '\200'      /* positive base-256 */
                                  || *where == '\377' /* negative base-256 */ ))
    {
        /* Parse base-256 output.  A nonnegative number N is
           represented as (256**DIGS)/2 + N; a negative number -N is
           represented as (256**DIGS) - N, i.e. as two's complement.
           The representation guarantees that the leading bit is
           always on, so that we don't confuse this format with the
           others (assuming ASCII bytes of 8 bits or more). */

        int signbit;
        uintmax_t topbits;

        signbit = *where & (1 << (LG_256 - 2));
        topbits =
            (((uintmax_t) - signbit) << (CHAR_BIT * sizeof (uintmax_t) - LG_256 - (LG_256 - 2)));

        value = (*where++ & ((1 << (LG_256 - 2)) - 1)) - signbit;

        while (TRUE)
        {
            value = (value << LG_256) + (unsigned char) *where++;
            if (where == lim)
                break;

            if (((value << LG_256 >> LG_256) | topbits) != value)
                return (-1);
        }

        negative = signbit != 0;
        if (negative)
            value = -value;
    }

    if (where != lim && *where != '\0' && !isspace ((unsigned char) *where))
        return (-1);

    if (value <= (negative ? minus_minval : maxval))
        return tar_represent_uintmax (negative ? -value : value);

    return (-1);
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

static inline off_t
off_from_header (const char *p, size_t s)
{
    /* Negative offsets are not allowed in tar files, so invoke
       from_header with minimum value 0, not TYPE_MINIMUM (off_t). */
    return tar_from_header (p, s, "off_t", 0, TYPE_MAXIMUM (off_t), FALSE);
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

static inline uintmax_t
uintmax_from_header (const char *p, size_t s)
{
    return tar_from_header (p, s, "uintmax_t", 0, UINTMAX_MAX, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static union block *
tar_find_next_block (tar_super_t * archive)
{
    int n;

    n = mc_read (archive->fd, block_buf.buffer, sizeof (block_buf.buffer));
    if (n != sizeof (block_buf.buffer))
        return NULL;            /* An error has occurred */
    current_tar_position += sizeof (block_buf.buffer);
    return &block_buf;
}

/* --------------------------------------------------------------------------------------------- */

static void
tar_skip_n_records (tar_super_t * archive, off_t n)
{
    mc_lseek (archive->fd, n * sizeof (block_buf.buffer), SEEK_CUR);
    current_tar_position += n * sizeof (block_buf.buffer);
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
tar_decode_header (union block *header, tar_super_t * arch, struct stat *st)
{
    gboolean hbits = FALSE;

    st->st_mode = MODE_FROM_HEADER (header->header.mode, &hbits);

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
tar_fill_stat (struct vfs_s_super *archive, struct stat *st, union block *header)
{
    tar_super_t *arch = TAR_SUPER (archive);

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
    case TAR_OLDGNU:
        /* *INDENT-OFF* */
        st->st_uid = *header->header.uname != '\0'
            ? (uid_t) vfs_finduid (header->header.uname)
            : UID_FROM_HEADER (header->header.uid);
        st->st_gid = *header->header.gname != '\0'
            ? (gid_t) vfs_findgid (header->header.gname)
            : GID_FROM_HEADER (header->header.gid);
        /* *INDENT-ON* */

        switch (header->header.typeflag)
        {
        case BLKTYPE:
        case CHRTYPE:
#ifdef HAVE_STRUCT_STAT_ST_RDEV
            st->st_rdev =
                makedev (MAJOR_FROM_HEADER (header->header.devmajor),
                         MINOR_FROM_HEADER (header->header.devminor));
#endif
            break;
        default:
            break;
        }
        break;

    default:
        st->st_uid = UID_FROM_HEADER (header->header.uid);
        st->st_gid = GID_FROM_HEADER (header->header.gid);
        break;
    }

#ifdef HAVE_STRUCT_STAT_ST_MTIM
    st->st_atim.tv_nsec = st->st_mtim.tv_nsec = st->st_ctim.tv_nsec = 0;
#endif
    st->st_mtime = TIME_FROM_HEADER (header->header.mtime);
    st->st_atime = 0;
    st->st_ctime = 0;
    if (arch->type == TAR_GNU || arch->type == TAR_OLDGNU)
    {
        st->st_atime = TIME_FROM_HEADER (header->oldgnu_header.atime);
        st->st_ctime = TIME_FROM_HEADER (header->oldgnu_header.ctime);
    }

#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
    st->st_blksize = 8 * 1024;  /* FIXME */
#endif
    vfs_adjust_stat (st);
}

/* --------------------------------------------------------------------------------------------- */

static read_header
tar_read_header (struct vfs_class *me, struct vfs_s_super *archive, size_t * h_size)
{
    tar_super_t *arch = TAR_SUPER (archive);
    union block *header;
    struct stat st;
    static char *next_long_name = NULL, *next_long_link = NULL;
    read_header status;

    while (TRUE)
    {
        header = tar_find_next_block (arch);
        if (header == NULL)
            return HEADER_END_OF_FILE;

        status = tar_checksum (header);
        if (status != HEADER_SUCCESS)
            return status;

        if (header->header.typeflag == LNKTYPE || header->header.typeflag == DIRTYPE)
            *h_size = 0;        /* Links 0 size on tape */
        else
            *h_size = OFF_FROM_HEADER (header->header.size);

        memset (&st, 0, sizeof (st));
        st.st_size = *h_size;
        tar_decode_header (header, arch, &st);
        tar_fill_stat (archive, &st, header);

        /* Skip over pax extended header and global extended header records. */
        if (header->header.typeflag == XHDTYPE || header->header.typeflag == XGLTYPE)
        {
            if (arch->type == TAR_UNKNOWN)
                arch->type = TAR_POSIX;
            return HEADER_SUCCESS;
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
                return HEADER_FAILURE;
            }

            longp = header->header.typeflag == GNUTYPE_LONGNAME ? &next_long_name : &next_long_link;

            g_free (*longp);
            bp = *longp = g_malloc (*h_size + 1);

            for (size = *h_size; size > 0; size -= written)
            {
                data = tar_find_next_block (arch)->buffer;
                if (data == NULL)
                {
                    MC_PTR_FREE (*longp);
                    message (D_ERROR, MSG_ERROR, _("Unexpected EOF on archive file"));
                    return HEADER_FAILURE;
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
                return HEADER_FAILURE;
            }

            *bp = '\0';
        }
        else
            break;
    }

    {
        struct vfs_s_entry *entry;
        struct vfs_s_inode *inode = NULL, *parent;
        off_t data_position;
        char *p, *q;
        size_t len;
        char *recent_long_name, *recent_long_link;

        recent_long_link =
            next_long_link != NULL ? next_long_link : g_strndup (header->header.linkname,
                                                                 sizeof (header->header.linkname));

        recent_long_name = NULL;
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
                recent_long_name = g_strconcat (temp_prefix, PATH_SEP_STR,
                                                temp_name, (char *) NULL);
                g_free (temp_name);
                g_free (temp_prefix);
            }
            break;
        case TAR_GNU:
        case TAR_OLDGNU:
            if (next_long_name != NULL)
                recent_long_name = next_long_name;
            break;
        default:
            break;
        }

        if (recent_long_name == NULL)
        {
            if (next_long_name != NULL)
                recent_long_name = g_strdup (next_long_name);
            else
                recent_long_name = g_strndup (header->header.name, sizeof (header->header.name));
        }

        canonicalize_pathname (recent_long_name);

        data_position = current_tar_position;

        p = strrchr (recent_long_name, PATH_SEP);
        if (p == NULL)
        {
            len = strlen (recent_long_name);
            p = recent_long_name;
            q = recent_long_name + len; /* "" */
        }
        else
        {
            *(p++) = '\0';
            q = recent_long_name;
        }

        parent = vfs_s_find_inode (me, archive, q, LINK_NO_FOLLOW, FL_MKDIR);
        if (parent == NULL)
        {
            message (D_ERROR, MSG_ERROR, _("Inconsistent tar archive"));
            return HEADER_FAILURE;
        }

        if (header->header.typeflag == LNKTYPE)
        {
            if (*recent_long_link == '\0')
                inode = NULL;
            else
            {
                len = strlen (recent_long_link);
                if (IS_PATH_SEP (recent_long_link[len - 1]))
                    recent_long_link[len - 1] = '\0';

                inode = vfs_s_find_inode (me, archive, recent_long_link, LINK_NO_FOLLOW, FL_NONE);
            }

            if (inode == NULL)
                message (D_ERROR, MSG_ERROR, _("Inconsistent tar archive"));
            else
            {
                entry = vfs_s_new_entry (me, p, inode);
                vfs_s_insert_entry (me, parent, entry);
                g_free (recent_long_link);
                goto done;
            }
        }

        if (S_ISDIR (st.st_mode))
        {
            entry = VFS_SUBCLASS (me)->find_entry (me, parent, p, LINK_NO_FOLLOW, FL_NONE);
            if (entry != NULL)
                goto done;
        }

        inode = vfs_s_new_inode (me, archive, &st);
        inode->data_offset = data_position;

        if (*recent_long_link != '\0')
            inode->linkname = recent_long_link;
        else if (recent_long_link != next_long_link)
            g_free (recent_long_link);

        entry = vfs_s_new_entry (me, p, inode);
        vfs_s_insert_entry (me, parent, entry);
        g_free (recent_long_name);

      done:
        next_long_link = next_long_name = NULL;

        if (arch->type == TAR_GNU && header->oldgnu_header.isextended)
        {
            while (tar_find_next_block (arch)->sparse_header.isextended != 0)
                ;

            if (inode != NULL)
                inode->data_offset = current_tar_position;
        }
        return HEADER_SUCCESS;
    }
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

    current_tar_position = 0;
    /* Open for reading */
    if (!tar_open_archive_int (vpath_element->class, vpath, archive))
        return -1;

    while (TRUE)
    {
        size_t h_size = 0;
        read_header prev_status;

        prev_status = status;
        status = tar_read_header (vpath_element->class, archive, &h_size);

        switch (status)
        {
        case HEADER_STILL_UNREAD:
            message (D_ERROR, MSG_ERROR, _("%s\ndoesn't look like a tar archive"),
                     vfs_path_as_str (vpath));
            return -1;

        case HEADER_SUCCESS:
            tar_skip_n_records (archive, (h_size + BLOCKSIZE - 1) / BLOCKSIZE);
            continue;

            /*
             * Invalid header:
             *
             * If the previous header was good, tell them
             * that we are skipping bad ones.
             */
        case HEADER_FAILURE:
            switch (prev_status)
            {
                /* Error on first block */
            case HEADER_ZERO_BLOCK:
                {
                    message (D_ERROR, MSG_ERROR, _("%s\ndoesn't look like a tar archive"),
                             vfs_path_as_str (vpath));
                    MC_FALLTHROUGH;

                    /* Error after header rec */
                }
            case HEADER_SUCCESS:
                /* Error after error */

            case HEADER_FAILURE:
                return -1;

            case HEADER_END_OF_FILE:
                return 0;

            default:
                break;
            }
            MC_FALLTHROUGH;

            /* Record of zeroes */
        case HEADER_ZERO_BLOCK:        /* If error after 0's */
            MC_FALLTHROUGH;
            /* exit from loop */
        case HEADER_END_OF_FILE:       /* End of archive */
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

    tar_base64_init ();
}

/* --------------------------------------------------------------------------------------------- */
