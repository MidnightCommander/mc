/*
   Virtual File System: GNU Tar file system.

   Copyright (C) 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2006, 2007, 2011
   The Free Software Foundation, Inc.

   Written by:
   Jakub Jelinek, 1995
   Pavel Machek, 1998

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
#include <fcntl.h>

#ifdef hpux
/* major() and minor() macros (among other things) defined here for hpux */
#include <sys/mknod.h>
#endif

#include "lib/global.h"
#include "lib/util.h"
#include "lib/widget.h"         /* message() */

#include "lib/vfs/vfs.h"
#include "lib/vfs/utilvfs.h"
#include "lib/vfs/xdirentry.h"
#include "lib/vfs/gc.h"         /* vfs_rmstamp */

#include "tar.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*
 * Header block on tape.
 *
 * I'm going to use traditional DP naming conventions here.
 * A "block" is a big chunk of stuff that we do I/O on.
 * A "record" is a piece of info that we care about.
 * Typically many "record"s fit into a "block".
 */
#define	RECORDSIZE      512
#define	NAMSIZ          100
#define	PREFIX_SIZE     155
#define	TUNMLEN         32
#define	TGNMLEN         32
#define SPARSE_EXT_HDR  21
#define SPARSE_IN_HDR   4

/* The checksum field is filled with this while the checksum is computed. */
#define	CHKBLANKS       "        "      /* 8 blanks, no null */

/* The magic field is filled with this if uname and gname are valid. */
#define	TMAGIC          "ustar" /* ustar and a null */
#define	OLDGNU_MAGIC    "ustar  "       /* 7 chars and a null */

/* The linkflag defines the type of file */
#define	LF_OLDNORMAL    '\0'    /* Normal disk file, Unix compat */
#define	LF_NORMAL       '0'     /* Normal disk file */
#define	LF_LINK         '1'     /* Link to previously dumped file */
#define	LF_SYMLINK      '2'     /* Symbolic link */
#define	LF_CHR          '3'     /* Character special file */
#define	LF_BLK          '4'     /* Block special file */
#define	LF_DIR          '5'     /* Directory */
#define	LF_FIFO         '6'     /* FIFO special file */
#define	LF_CONTIG       '7'     /* Contiguous file */
#define	LF_EXTHDR       'x'     /* pax Extended Header */
#define	LF_GLOBAL_EXTHDR 'g'    /* pax Global Extended Header */
/* Further link types may be defined later. */

/* Note that the standards committee allows only capital A through
   capital Z for user-defined expansion.  This means that defining something
   as, say '8' is a *bad* idea. */
#define LF_DUMPDIR      'D'     /* This is a dir entry that contains
                                   the names of files that were in
                                   the dir at the time the dump
                                   was made */
#define LF_LONGLINK     'K'     /* Identifies the NEXT file on the tape
                                   as having a long linkname */
#define LF_LONGNAME     'L'     /* Identifies the NEXT file on the tape
                                   as having a long name. */
#define LF_MULTIVOL     'M'     /* This is the continuation
                                   of a file that began on another
                                   volume */
#define LF_NAMES        'N'     /* For storing filenames that didn't
                                   fit in 100 characters */
#define LF_SPARSE       'S'     /* This is for sparse files */
#define LF_VOLHDR       'V'     /* This file is a tape/volume header */
/* Ignore it on extraction */

/*
 * Exit codes from the "tar" program
 */
#define	EX_SUCCESS      0       /* success! */
#define	EX_ARGSBAD      1       /* invalid args */
#define	EX_BADFILE      2       /* invalid filename */
#define	EX_BADARCH      3       /* bad archive */
#define	EX_SYSTEM       4       /* system gave unexpected error */
#define EX_BADVOL       5       /* Special error code means
                                   Tape volume doesn't match the one
                                   specified on the command line */

#define	isodigit(c)     ( ((c) >= '0') && ((c) <= '7') )

/*** file scope type declarations ****************************************************************/

enum
{
    TAR_UNKNOWN = 0,
    TAR_V7,
    TAR_USTAR,
    TAR_POSIX,
    TAR_GNU
};

struct sparse
{
    char offset[12];
    char numbytes[12];
};

struct sp_array
{
    int offset;
    int numbytes;
};

union record
{
    char charptr[RECORDSIZE];
    struct header
    {
        char arch_name[NAMSIZ];
        char mode[8];
        char uid[8];
        char gid[8];
        char size[12];
        char mtime[12];
        char chksum[8];
        char linkflag;
        char arch_linkname[NAMSIZ];
        char magic[8];
        char uname[TUNMLEN];
        char gname[TGNMLEN];
        char devmajor[8];
        char devminor[8];
        /* The following bytes of the tar header record were originally unused.

           Archives following the ustar specification use almost all of those
           bytes to support pathnames of 256 characters in length.

           GNU tar archives use the "unused" space to support incremental
           archives and sparse files. */
        union unused
        {
            char prefix[PREFIX_SIZE];
            /* GNU extensions to the ustar (POSIX.1-1988) archive format. */
            struct oldgnu
            {
                char atime[12];
                char ctime[12];
                char offset[12];
                char longnames[4];
                char pad;
                struct sparse sp[SPARSE_IN_HDR];
                char isextended;
                char realsize[12];      /* true size of the sparse file */
            } oldgnu;
        } unused;
    } header;
    struct extended_header
    {
        struct sparse sp[21];
        char isextended;
    } ext_hdr;
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
    int fd;
    struct stat st;
    int type;                   /* Type of the archive */
} tar_super_data_t;

/*** file scope variables ************************************************************************/

static struct vfs_class vfs_tarfs_ops;

/* As we open one archive at a time, it is safe to have this static */
static off_t current_tar_position = 0;

static union record rec_buf;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Quick and dirty octal conversion.
 *
 * Result is -1 if the field is invalid (all blank, or nonoctal).
 */
static long
tar_from_oct (int digs, char *where)
{
    register long value;

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

static void
tar_free_archive (struct vfs_class *me, struct vfs_s_super *archive)
{
    (void) me;

    if (archive->data != NULL)
    {
        tar_super_data_t *arch = (tar_super_data_t *) archive->data;

        if (arch->fd != -1)
            mc_close (arch->fd);
        g_free (archive->data);
    }
}

/* --------------------------------------------------------------------------------------------- */

/* Returns fd of the open tar file */
static int
tar_open_archive_int (struct vfs_class *me, const vfs_path_t * vpath, struct vfs_s_super *archive)
{
    int result, type;
    tar_super_data_t *arch;
    mode_t mode;
    struct vfs_s_inode *root;

    result = mc_open (vpath, O_RDONLY);
    if (result == -1)
    {
        char *name;

        name = vfs_path_to_str (vpath);
        message (D_ERROR, MSG_ERROR, _("Cannot open tar archive\n%s"), name);
        g_free (name);
        ERRNOR (ENOENT, -1);
    }

    archive->name = vfs_path_to_str (vpath);
    archive->data = g_new (tar_super_data_t, 1);
    arch = (tar_super_data_t *) archive->data;
    mc_stat (vpath, &arch->st);
    arch->fd = -1;
    arch->type = TAR_UNKNOWN;

    /* Find out the method to handle this tar file */
    type = get_compression_type (result, archive->name);
    mc_lseek (result, 0, SEEK_SET);
    if (type != COMPRESSION_NONE)
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
            g_free (archive->name);
            archive->name = NULL;
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
    root->st.st_dev = MEDATA->rdev++;

    archive->root = root;

    return result;
}

/* --------------------------------------------------------------------------------------------- */

static union record *
tar_get_next_record (struct vfs_s_super *archive, int tard)
{
    int n;

    (void) archive;

    n = mc_read (tard, rec_buf.charptr, RECORDSIZE);
    if (n != RECORDSIZE)
        return NULL;            /* An error has occurred */
    current_tar_position += RECORDSIZE;
    return &rec_buf;
}

/* --------------------------------------------------------------------------------------------- */

static void
tar_skip_n_records (struct vfs_s_super *archive, int tard, size_t n)
{
    (void) archive;

    mc_lseek (tard, n * RECORDSIZE, SEEK_CUR);
    current_tar_position += n * RECORDSIZE;
}

/* --------------------------------------------------------------------------------------------- */

static void
tar_fill_stat (struct vfs_s_super *archive, struct stat *st, union record *header, size_t h_size)
{
    tar_super_data_t *arch = (tar_super_data_t *) archive->data;

    st->st_mode = tar_from_oct (8, header->header.mode);

    /* Adjust st->st_mode because there are tar-files with
     * linkflag==LF_SYMLINK and S_ISLNK(mod)==0. I don't 
     * know about the other modes but I think I cause no new
     * problem when I adjust them, too. -- Norbert.
     */
    if (header->header.linkflag == LF_DIR)
        st->st_mode |= S_IFDIR;
    else if (header->header.linkflag == LF_SYMLINK)
        st->st_mode |= S_IFLNK;
    else if (header->header.linkflag == LF_CHR)
        st->st_mode |= S_IFCHR;
    else if (header->header.linkflag == LF_BLK)
        st->st_mode |= S_IFBLK;
    else if (header->header.linkflag == LF_FIFO)
        st->st_mode |= S_IFIFO;
    else
        st->st_mode |= S_IFREG;

    st->st_rdev = 0;
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

        switch (header->header.linkflag)
        {
        case LF_BLK:
        case LF_CHR:
            st->st_rdev =
                (tar_from_oct (8, header->header.devmajor) << 8) |
                tar_from_oct (8, header->header.devminor);
        }
    default:
        st->st_uid = tar_from_oct (8, header->header.uid);
        st->st_gid = tar_from_oct (8, header->header.gid);
    }
    st->st_size = h_size;
    st->st_mtime = tar_from_oct (1 + 12, header->header.mtime);
    st->st_atime = 0;
    st->st_ctime = 0;
    if (arch->type == TAR_GNU)
    {
        st->st_atime = tar_from_oct (1 + 12, header->header.unused.oldgnu.atime);
        st->st_ctime = tar_from_oct (1 + 12, header->header.unused.oldgnu.ctime);
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Return 1 for success, 0 if the checksum is bad, EOF on eof,
 * 2 for a record full of zeros (EOF marker).
 *
 */
static ReadStatus
tar_read_header (struct vfs_class *me, struct vfs_s_super *archive, int tard, size_t * h_size)
{
    tar_super_data_t *arch = (tar_super_data_t *) archive->data;

    register int i;
    register long sum, signed_sum, recsum;
    register char *p;
    register union record *header;
    static char *next_long_name = NULL, *next_long_link = NULL;

  recurse:

    header = tar_get_next_record (archive, tard);
    if (NULL == header)
        return STATUS_EOF;

    recsum = tar_from_oct (8, header->header.chksum);

    sum = 0;
    signed_sum = 0;
    p = header->charptr;
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
     * Try to determine the archive format.
     */
    if (arch->type == TAR_UNKNOWN)
    {
        if (strcmp (header->header.magic, TMAGIC) == 0)
        {
            if (header->header.linkflag == LF_GLOBAL_EXTHDR)
                arch->type = TAR_POSIX;
            else
                arch->type = TAR_USTAR;
        }
        else if (strcmp (header->header.magic, OLDGNU_MAGIC) == 0)
            arch->type = TAR_GNU;
    }

    /*
     * linkflag on BSDI tar (pax) always '\000'
     */
    if (header->header.linkflag == '\000')
    {
        size_t len;

        if (header->header.arch_name[NAMSIZ - 1] != '\0')
            len = NAMSIZ;
        else
            len = strlen (header->header.arch_name);

        if (len != 0 && header->header.arch_name[len - 1] == '/')
            header->header.linkflag = LF_DIR;
    }

    /*
     * Good record.  Decode file size and return.
     */
    if (header->header.linkflag == LF_LINK || header->header.linkflag == LF_DIR)
        *h_size = 0;            /* Links 0 size on tape */
    else
        *h_size = tar_from_oct (1 + 12, header->header.size);

    /*
     * Skip over directory snapshot info records that
     * are stored in incremental tar archives.
     */
    if (header->header.linkflag == LF_DUMPDIR)
    {
        if (arch->type == TAR_UNKNOWN)
            arch->type = TAR_GNU;
        return STATUS_SUCCESS;
    }

    /*
     * Skip over pax extended header and global extended
     * header records.
     */
    if (header->header.linkflag == LF_EXTHDR || header->header.linkflag == LF_GLOBAL_EXTHDR)
    {
        if (arch->type == TAR_UNKNOWN)
            arch->type = TAR_POSIX;
        return STATUS_SUCCESS;
    }

    if (header->header.linkflag == LF_LONGNAME || header->header.linkflag == LF_LONGLINK)
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

        longp = ((header->header.linkflag == LF_LONGNAME) ? &next_long_name : &next_long_link);

        g_free (*longp);
        bp = *longp = g_malloc (*h_size + 1);

        for (size = *h_size; size > 0; size -= written)
        {
            data = tar_get_next_record (archive, tard)->charptr;
            if (data == NULL)
            {
                g_free (*longp);
                *longp = NULL;
                message (D_ERROR, MSG_ERROR, _("Unexpected EOF on archive file"));
                return STATUS_BADCHECKSUM;
            }
            written = RECORDSIZE;
            if ((off_t) written > size)
                written = (size_t) size;

            memcpy (bp, data, written);
            bp += written;
        }

        if (bp - *longp == MC_MAXPATHLEN && bp[-1] != '\0')
        {
            g_free (*longp);
            *longp = NULL;
            message (D_ERROR, MSG_ERROR, _("Inconsistent tar archive"));
            return STATUS_BADCHECKSUM;
        }
        *bp = 0;
        goto recurse;
    }
    else
    {
        struct stat st;
        struct vfs_s_entry *entry;
        struct vfs_s_inode *inode = NULL, *parent;
        off_t data_position;
        char *q;
        size_t len;
        char *current_file_name, *current_link_name;

        current_link_name =
            (next_long_link ? next_long_link : g_strndup (header->header.arch_linkname, NAMSIZ));
        len = strlen (current_link_name);
        if (len > 1 && current_link_name[len - 1] == '/')
            current_link_name[len - 1] = 0;

        current_file_name = NULL;
        switch (arch->type)
        {
        case TAR_USTAR:
        case TAR_POSIX:
            /* The ustar archive format supports pathnames of upto 256
             * characters in length. This is achieved by concatenating
             * the contents of the `prefix' and `arch_name' fields like
             * this:
             *
             *   prefix + path_separator + arch_name
             *
             * If the `prefix' field contains an empty string i.e. its
             * first characters is '\0' the prefix field is ignored.
             */
            if (header->header.unused.prefix[0] != '\0')
            {
                char *temp_name, *temp_prefix;

                temp_name = g_strndup (header->header.arch_name, NAMSIZ);
                temp_prefix = g_strndup (header->header.unused.prefix, PREFIX_SIZE);
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
                current_file_name = g_strndup (header->header.arch_name, NAMSIZ);
        }

        canonicalize_pathname (current_file_name);
        len = strlen (current_file_name);

        data_position = current_tar_position;

        p = strrchr (current_file_name, '/');
        if (p == NULL)
        {
            p = current_file_name;
            q = current_file_name + len;        /* "" */
        }
        else
        {
            *(p++) = 0;
            q = current_file_name;
        }

        parent = vfs_s_find_inode (me, archive, q, LINK_NO_FOLLOW, FL_MKDIR);
        if (parent == NULL)
        {
            message (D_ERROR, MSG_ERROR, _("Inconsistent tar archive"));
            return STATUS_BADCHECKSUM;
        }

        if (header->header.linkflag == LF_LINK)
        {
            inode = vfs_s_find_inode (me, archive, current_link_name, LINK_NO_FOLLOW, 0);
            if (inode == NULL)
            {
                message (D_ERROR, MSG_ERROR, _("Inconsistent tar archive"));
            }
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
            entry = MEDATA->find_entry (me, parent, p, LINK_NO_FOLLOW, FL_NONE);
            if (entry)
                goto done;
        }
        inode = vfs_s_new_inode (me, archive, &st);

        inode->data_offset = data_position;
        if (*current_link_name)
        {
            inode->linkname = current_link_name;
        }
        else if (current_link_name != next_long_link)
        {
            g_free (current_link_name);
        }
        entry = vfs_s_new_entry (me, p, inode);

        vfs_s_insert_entry (me, parent, entry);
        g_free (current_file_name);

      done:
        next_long_link = next_long_name = NULL;

        if (arch->type == TAR_GNU && header->header.unused.oldgnu.isextended)
        {
            while (tar_get_next_record (archive, tard)->ext_hdr.isextended != 0)
                ;
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
    ReadStatus prev_status;
    int tard;

    current_tar_position = 0;
    /* Open for reading */
    tard = tar_open_archive_int (vpath_element->class, vpath, archive);
    if (tard == -1)
        return -1;

    while (TRUE)
    {
        size_t h_size;

        prev_status = status;
        status = tar_read_header (vpath_element->class, archive, tard, &h_size);

        switch (status)
        {
        case STATUS_SUCCESS:
            tar_skip_n_records (archive, tard, (h_size + RECORDSIZE - 1) / RECORDSIZE);
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
                /* Error on first record */
            case STATUS_EOFMARK:
                {
                    char *archive_name = vfs_path_to_str (vpath);
                    message (D_ERROR, MSG_ERROR, _("%s\ndoesn't look like a tar archive."),
                             archive_name);
                    g_free (archive_name);
                    /* FALL THRU */

                    /* Error after header rec */
                }
            case STATUS_SUCCESS:
                /* Error after error */

            case STATUS_BADCHECKSUM:
                return -1;

            case STATUS_EOF:
                return 0;
            }

            /* Record of zeroes */
        case STATUS_EOFMARK:   /* If error after 0's */
            /* FALL THRU */
            /* exit from loop */
        case STATUS_EOF:       /* End of archive */
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
    char *archive_name = vfs_path_to_str (vpath);

    (void) vpath_element;

    if (strcmp (parc->name, archive_name) != 0)
    {
        g_free (archive_name);
        return 0;
    }
    g_free (archive_name);

    /* Has the cached archive been changed on the disk? */
    if (((tar_super_data_t *) parc->data)->st.st_mtime < archive_stat->st_mtime)
    {
        /* Yes, reload! */
        (*vfs_tarfs_ops.free) ((vfsid) parc);
        vfs_rmstamp (&vfs_tarfs_ops, (vfsid) parc);
        return 2;
    }
    /* Hasn't been modified, give it a new timeout */
    vfs_stamp (&vfs_tarfs_ops, (vfsid) parc);
    return 1;
}

/* --------------------------------------------------------------------------------------------- */

static ssize_t
tar_read (void *fh, char *buffer, size_t count)
{
    off_t begin = FH->ino->data_offset;
    int fd = ((tar_super_data_t *) FH_SUPER->data)->fd;
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
init_tarfs (void)
{
    static struct vfs_s_subclass tarfs_subclass;

    tarfs_subclass.flags = VFS_S_READONLY;      /* FIXME: tarfs used own temp files */
    tarfs_subclass.archive_check = tar_super_check;
    tarfs_subclass.archive_same = tar_super_same;
    tarfs_subclass.open_archive = tar_open_archive;
    tarfs_subclass.free_archive = tar_free_archive;
    tarfs_subclass.fh_open = tar_fh_open;

    vfs_s_init_class (&vfs_tarfs_ops, &tarfs_subclass);
    vfs_tarfs_ops.name = "tarfs";
    vfs_tarfs_ops.prefix = "utar";
    vfs_tarfs_ops.read = tar_read;
    vfs_tarfs_ops.setctl = NULL;
    vfs_register_class (&vfs_tarfs_ops);
}

/* --------------------------------------------------------------------------------------------- */
