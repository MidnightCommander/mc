
#ifndef MC__VFS_TAR_INTERNAL_H
#define MC__VFS_TAR_INTERNAL_H

#include <inttypes.h>           /* (u)intmax_t */
#include <limits.h>             /* CHAR_BIT, INT_MAX, etc */
#include <sys/stat.h>
#include <sys/types.h>

#include "lib/vfs/xdirentry.h"  /* vfs_s_super */

/*** typedefs(not structures) and defined constants **********************************************/

/* tar files are made in basic blocks of this size.  */
#define BLOCKSIZE 512

#define DEFAULT_BLOCKING 20

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

/* *BEWARE* *BEWARE* *BEWARE* that the following information is still
   boiling, and may change.  Even if the OLDGNU format description should be
   accurate, the so-called GNU format is not yet fully decided.  It is
   surely meant to use only extensions allowed by POSIX, but the sketch
   below repeats some ugliness from the OLDGNU format, which should rather
   go away.  Sparse files should be saved in such a way that they do *not*
   require two passes at archive creation time.  Huge files get some POSIX
   fields to overflow, alternate solutions have to be sought for this.  */

/* This is a dir entry that contains the names of files that were in the
   dir at the time the dump was made.  */
#define GNUTYPE_DUMPDIR 'D'

/* Identifies the *next* file on the tape as having a long linkname.  */
#define GNUTYPE_LONGLINK 'K'

/* Identifies the *next* file on the tape as having a long name.  */
#define GNUTYPE_LONGNAME 'L'


/* These macros work even on ones'-complement hosts (!).
   The extra casts work around some compiler bugs. */
#define TYPE_SIGNED(t) (!((t) 0 < (t) (-1)))
#define TYPE_MINIMUM(t) (TYPE_SIGNED (t) ? ~(t) 0 << (sizeof (t) * CHAR_BIT - 1) : (t) 0)
#define TYPE_MAXIMUM(t) (~(t) 0 - TYPE_MINIMUM (t))

#define OFF_FROM_HEADER(where) off_from_header (where, sizeof (where))

#define isodigit(c) ( ((c) >= '0') && ((c) <= '7') )

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

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
    char atime[12];             /* 345 Incr. archive: atime of the file */
    char ctime[12];             /* 357 Incr. archive: ctime of the file */
    char offset[12];            /* 369 Multivolume archive: the offset of start of this volume */
    char longnames[4];          /* 381 Not used */
    char unused_pad2;           /* 385 */
    struct sparse sp[SPARSES_IN_OLDGNU_HEADER];
                                /* 386 */
    char isextended;            /* 482 Sparse file: Extension sparse header follows */
    char realsize[12];          /* 483 Sparse file: Real size */
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
    TAR_UNKNOWN = 0,            /**< format to be decided later */
    TAR_V7,                     /**< old V7 tar format */
    TAR_OLDGNU,                 /**< GNU format as per before tar 1.12 */
    TAR_USTAR,                  /**< POSIX.1-1988 (ustar) format */
    TAR_POSIX,                  /**< POSIX.1-2001 format */
    TAR_GNU                     /**< almost same as OLDGNU_FORMAT */
};

typedef struct
{
    struct vfs_s_super base;    /* base class */

    int fd;
    struct stat st;
    enum archive_format type;   /**< type of the archive */
    union block *record_start;  /**< start of record of archive */
} tar_super_t;

struct tar_stat_info
{
    char *orig_file_name;       /**< name of file read from the archive header */
    char *file_name;            /**< name of file for the current archive entry after being normalized */
    char *link_name;            /**< name of link for the current archive entry  */
#if 0
    char *uname;                /**< user name of owner */
    char *gname;                /**< group name of owner */
#endif
    struct stat stat;           /**< regular filesystem stat */

    /* stat() doesn't always have access, data modification, and status
       change times in a convenient form, so store them separately.  */
    struct timespec atime;
    struct timespec mtime;
    struct timespec ctime;
};

/*** global variables defined in .c file *********************************************************/

extern const int blocking_factor;
extern const size_t record_size;

extern union block *record_end; /* last+1 block of archive record */
extern union block *current_block;      /* current block of archive */
extern off_t record_start_block;        /* block ordinal at record_start */

extern union block *current_header;

/* Have we hit EOF yet?  */
extern gboolean hit_eof;

extern struct tar_stat_info current_stat_info;

/*** declarations of public functions ************************************************************/

/* tar-internal.c */
void tar_base64_init (void);
void tar_assign_string (char **string, char *value);
void tar_assign_string_dup (char **string, const char *value);
void tar_assign_string_dup_n (char **string, const char *value, size_t n);
intmax_t tar_from_header (const char *where0, size_t digs, char const *type, intmax_t minval,
                          uintmax_t maxval, gboolean octal_only);
off_t off_from_header (const char *p, size_t s);
union block *tar_find_next_block (tar_super_t * archive);
gboolean tar_set_next_block_after (union block *block);
off_t tar_current_block_ordinal (const tar_super_t * archive);
gboolean tar_skip_file (tar_super_t * archive, off_t size);

/*** inline functions ****************************************************************************/

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
    intmax_t nd;

    if (n <= INTMAX_MAX)
        return n;

    /* Avoid signed integer overflow on picky platforms. */
    nd = n - INTMAX_MIN;
    return nd + INTMAX_MIN;
}

#endif /* MC__VFS_TAR_INTERNAL_H */
