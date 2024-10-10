/*
   Virtual File System: GNU Tar file system.

   Copyright (C) 2003-2024
   Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2023

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
 */

/*
 * Avoid following error:
 *   comparison of unsigned expression < 0 is always false [-Werror=type-limits]
 *
 * https://www.boost.org/doc/libs/1_55_0/libs/integer/test/cstdint_test.cpp
 *   We can't suppress this warning on the command line as not all GCC versions support -Wno-type-limits
 */
#if defined(__GNUC__) && (__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 4))
#pragma GCC diagnostic ignored "-Wtype-limits"
#endif

#include <config.h>

#include <inttypes.h>           /* uintmax_t */

#include "lib/global.h"

#include "tar-internal.h"

/* Old GNU Format.
   The sparse file information is stored in the oldgnu_header in the following manner:

   The header is marked with type 'S'. Its 'size' field contains the cumulative size
   of all non-empty blocks of the file. The actual file size is stored in `realsize'
   member of oldgnu_header.

   The map of the file is stored in a list of 'struct sparse'. Each struct contains
   offset to the block of data and its size (both as octal numbers). The first file
   header contains at most 4 such structs (SPARSES_IN_OLDGNU_HEADER). If the map
   contains more structs, then the field 'isextended' of the main header is set to
   1 (binary) and the 'struct sparse_header' header follows, containing at most
   21 following structs (SPARSES_IN_SPARSE_HEADER). If more structs follow, 'isextended'
   field of the extended header is set and next  next extension header follows, etc...
 */

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/* Bound on length of the string representing an unsigned integer
   value representable in B bits.  log10 (2.0) < 146/485.  The
   smallest value of B where this bound is not tight is 2621.  */
#define INT_BITS_STRLEN_BOUND(b) (((b) * 146 + 484) / 485)

/* Bound on buffer size needed to represent an integer type or expression T,
   including the terminating null.  T must not be a bit-field expression.  */
#define INT_BUFSIZE_BOUND(t) (INT_STRLEN_BOUND (t) + 1)

#define UINTMAX_STRSIZE_BOUND INT_BUFSIZE_BOUND (uintmax_t)

#define SPARSES_INIT_COUNT SPARSES_IN_SPARSE_HEADER

#define COPY_BUF(arch,b,buf,src)                                         \
do                                                                       \
{                                                                        \
    char *endp = b->buffer + BLOCKSIZE;                                  \
    char *dst = buf;                                                     \
    do                                                                   \
    {                                                                    \
        if (dst == buf + UINTMAX_STRSIZE_BOUND - 1)                      \
            /* numeric overflow in sparse archive member */              \
            return FALSE;                                                \
        if (src == endp)                                                 \
        {                                                                \
            tar_set_next_block_after (b);                                \
            b = tar_find_next_block (arch);                              \
            if (b == NULL)                                               \
                /* unexpected EOF in archive */                          \
                return FALSE;                                            \
            src = b->buffer;                                             \
            endp = b->buffer + BLOCKSIZE;                                \
        }                                                                \
        *dst = *src++;                                                   \
    }                                                                    \
    while (*dst++ != '\n');                                              \
    dst[-1] = '\0';                                                      \
}                                                                        \
while (FALSE)

/*** file scope type declarations ****************************************************************/

struct tar_sparse_file;

struct tar_sparse_optab
{
    gboolean (*init) (struct tar_sparse_file * file);
    gboolean (*done) (struct tar_sparse_file * file);
    gboolean (*sparse_member_p) (struct tar_sparse_file * file);
    gboolean (*fixup_header) (struct tar_sparse_file * file);
    gboolean (*decode_header) (tar_super_t * archive, struct tar_sparse_file * file);
};

struct tar_sparse_file
{
    int fd;                     /**< File descriptor */
    off_t dumped_size;          /**< Number of bytes actually written to the archive */
    struct tar_stat_info *stat_info;    /**< Information about the file */
    struct tar_sparse_optab const *optab;
    void *closure;              /**< Any additional data optab calls might reqiure */
};

enum oldgnu_add_status
{
    add_ok,
    add_finish,
    add_fail
};

/*** forward declarations (file scope functions) *************************************************/

static gboolean oldgnu_sparse_member_p (struct tar_sparse_file *file);
static gboolean oldgnu_fixup_header (struct tar_sparse_file *file);
static gboolean oldgnu_get_sparse_info (tar_super_t * archive, struct tar_sparse_file *file);

static gboolean star_sparse_member_p (struct tar_sparse_file *file);
static gboolean star_fixup_header (struct tar_sparse_file *file);
static gboolean star_get_sparse_info (tar_super_t * archive, struct tar_sparse_file *file);

static gboolean pax_sparse_member_p (struct tar_sparse_file *file);
static gboolean pax_decode_header (tar_super_t * archive, struct tar_sparse_file *file);

/*** file scope variables ************************************************************************/

/* *INDENT-OFF* */
static struct tar_sparse_optab const oldgnu_optab =
{
    .init = NULL,               /* No init function */
    .done = NULL,               /* No done function */
    .sparse_member_p = oldgnu_sparse_member_p,
    .fixup_header = oldgnu_fixup_header,
    .decode_header = oldgnu_get_sparse_info
};
/* *INDENT-ON* */

/* *INDENT-OFF* */
static struct tar_sparse_optab const star_optab =
{
    .init = NULL,               /* No init function */
    .done = NULL,               /* No done function */
    .sparse_member_p = star_sparse_member_p,
    .fixup_header = star_fixup_header,
    .decode_header = star_get_sparse_info
};
/* *INDENT-ON* */

/* GNU PAX sparse file format. There are several versions:
 * 0.0

 The initial version of sparse format used by tar 1.14-1.15.1.
 The sparse file map is stored in x header:

 GNU.sparse.size      Real size of the stored file
 GNU.sparse.numblocks Number of blocks in the sparse map repeat numblocks time
 GNU.sparse.offset    Offset of the next data block
 GNU.sparse.numbytes  Size of the next data block end repeat

 This has been reported as conflicting with the POSIX specs. The reason is
 that offsets and sizes of non-zero data blocks were stored in multiple instances
 of GNU.sparse.offset/GNU.sparse.numbytes variables, whereas POSIX requires the
 latest occurrence of the variable to override all previous occurrences.

 To avoid this incompatibility two following versions were introduced.

 * 0.1

 Used by tar 1.15.2 -- 1.15.91 (alpha releases).

 The sparse file map is stored in x header:

 GNU.sparse.size      Real size of the stored file
 GNU.sparse.numblocks Number of blocks in the sparse map
 GNU.sparse.map       Map of non-null data chunks. A string consisting of comma-separated
 values "offset,size[,offset,size]..."

 The resulting GNU.sparse.map string can be *very* long. While POSIX does not impose
 any limit on the length of a x header variable, this can confuse some tars.

 * 1.0

 Starting from this version, the exact sparse format version is specified explicitly
 in the header using the following variables:

 GNU.sparse.major     Major version 
 GNU.sparse.minor     Minor version

 X header keeps the following variables:

 GNU.sparse.name      Real file name of the sparse file
 GNU.sparse.realsize  Real size of the stored file (corresponds to the old GNU.sparse.size
 variable)

 The name field of the ustar header is constructed using the pattern "%d/GNUSparseFile.%p/%f".

 The sparse map itself is stored in the file data block, preceding the actual file data.
 It consists of a series of octal numbers of arbitrary length, delimited by newlines.
 The map is padded with nulls to the nearest block boundary.

 The first number gives the number of entries in the map. Following are map entries, each one
 consisting of two numbers giving the offset and size of the data block it describes.

 The format is designed in such a way that non-posix aware tars and tars not supporting
 GNU.sparse.* keywords will extract each sparse file in its condensed form with the file map
 attached and will place it into a separate directory. Then, using a simple program it would be
 possible to expand the file to its original form even without GNU tar.

 Bu default, v.1.0 archives are created. To use other formats, --sparse-version option is provided.
 Additionally, v.0.0 can be obtained by deleting GNU.sparse.map from 0.1 format:
 --sparse-version 0.1 --pax-option delete=GNU.sparse.map
 */

static struct tar_sparse_optab const pax_optab = {
    .init = NULL,               /* No init function */
    .done = NULL,               /* No done function */
    .sparse_member_p = pax_sparse_member_p,
    .fixup_header = NULL,       /* No fixup_header function */
    .decode_header = pax_decode_header
};

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static gboolean
decode_num (uintmax_t *num, const char *arg, uintmax_t maxval)
{
    char *arg_lim;
    gboolean overflow;

    *num = stoint (arg, &arg_lim, &overflow, 0, maxval);
    return !(arg_lim == arg || *arg_lim != '\0' || overflow);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
sparse_select_optab (const tar_super_t *archive, struct tar_sparse_file *file)
{
    switch (archive->type)
    {
    case TAR_V7:
    case TAR_USTAR:
        return FALSE;

    case TAR_OLDGNU:
    case TAR_GNU:              /* FIXME: This one should disappear? */
        file->optab = &oldgnu_optab;
        break;

    case TAR_POSIX:
        file->optab = &pax_optab;
        break;

    case TAR_STAR:
        file->optab = &star_optab;
        break;

    default:
        return FALSE;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
sparse_init (tar_super_t *archive, struct tar_sparse_file *file)
{
    memset (file, 0, sizeof (*file));

    if (!sparse_select_optab (archive, file))
        return FALSE;

    if (file->optab->init != NULL)
        return file->optab->init (file);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
sparse_done (struct tar_sparse_file *file)
{
    if (file->optab->done != NULL)
        return file->optab->done (file);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
sparse_member_p (struct tar_sparse_file *file)
{
    if (file->optab->sparse_member_p != NULL)
        return file->optab->sparse_member_p (file);

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
sparse_fixup_header (struct tar_sparse_file *file)
{
    if (file->optab->fixup_header != NULL)
        return file->optab->fixup_header (file);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
sparse_decode_header (tar_super_t *archive, struct tar_sparse_file *file)
{
    if (file->optab->decode_header != NULL)
        return file->optab->decode_header (archive, file);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static inline void
sparse_add_map (struct tar_stat_info *st, struct sp_array *sp)
{
    if (st->sparse_map == NULL)
        st->sparse_map = g_array_sized_new (FALSE, FALSE, sizeof (struct sp_array), 1);
    g_array_append_val (st->sparse_map, *sp);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Add a sparse item to the sparse file
 */
static enum oldgnu_add_status
oldgnu_add_sparse (struct tar_sparse_file *file, struct sparse *s)
{
    struct sp_array sp;
    off_t size;

    if (s->numbytes[0] == '\0')
        return add_finish;

    sp.offset = OFF_FROM_HEADER (s->offset);
    sp.numbytes = OFF_FROM_HEADER (s->numbytes);

    if (sp.offset < 0 || sp.numbytes < 0 || ckd_add (&size, sp.offset, sp.numbytes)
        || file->stat_info->stat.st_size < size || file->stat_info->archive_file_size < 0)
        return add_fail;

    sparse_add_map (file->stat_info, &sp);

    return add_ok;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
oldgnu_sparse_member_p (struct tar_sparse_file *file)
{
    (void) file;

    return current_header->header.typeflag == GNUTYPE_SPARSE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
oldgnu_fixup_header (struct tar_sparse_file *file)
{
    /* NOTE! st_size was initialized from the header which actually contains archived size.
       The following fixes it */
    off_t realsize;

    realsize = OFF_FROM_HEADER (current_header->oldgnu_header.realsize);
    file->stat_info->archive_file_size = file->stat_info->stat.st_size;
    file->stat_info->stat.st_size = MAX (0, realsize);

    return (realsize >= 0);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Convert old GNU format sparse data to internal representation.
 */
static gboolean
oldgnu_get_sparse_info (tar_super_t *archive, struct tar_sparse_file *file)
{
    size_t i;
    union block *h = current_header;
    gboolean ext_p;
    enum oldgnu_add_status rc = add_fail;

    if (file->stat_info->sparse_map != NULL)
        g_array_set_size (file->stat_info->sparse_map, 0);

    for (i = 0; i < SPARSES_IN_OLDGNU_HEADER; i++)
    {
        rc = oldgnu_add_sparse (file, &h->oldgnu_header.sp[i]);
        if (rc != add_ok)
            break;
    }

    for (ext_p = h->oldgnu_header.isextended != 0; rc == add_ok && ext_p;
         ext_p = h->sparse_header.isextended != 0)
    {
        h = tar_find_next_block (archive);
        if (h == NULL)
            return FALSE;

        tar_set_next_block_after (h);

        for (i = 0; i < SPARSES_IN_SPARSE_HEADER && rc == add_ok; i++)
            rc = oldgnu_add_sparse (file, &h->sparse_header.sp[i]);
    }

    return (rc != add_fail);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
star_sparse_member_p (struct tar_sparse_file *file)
{
    (void) file;

    return current_header->header.typeflag == GNUTYPE_SPARSE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
star_fixup_header (struct tar_sparse_file *file)
{
    /* NOTE! st_size was initialized from the header which actually contains archived size.
       The following fixes it */
    off_t realsize;

    realsize = OFF_FROM_HEADER (current_header->star_in_header.realsize);
    file->stat_info->archive_file_size = file->stat_info->stat.st_size;
    file->stat_info->stat.st_size = MAX (0, realsize);

    return (realsize >= 0);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Convert STAR format sparse data to internal representation
 */
static gboolean
star_get_sparse_info (tar_super_t *archive, struct tar_sparse_file *file)
{
    size_t i;
    union block *h = current_header;
    gboolean ext_p = TRUE;
    enum oldgnu_add_status rc = add_ok;

    if (file->stat_info->sparse_map != NULL)
        g_array_set_size (file->stat_info->sparse_map, 0);

    if (h->star_in_header.prefix[0] == '\0' && h->star_in_header.sp[0].offset[10] != '\0')
    {
        /* Old star format */
        for (i = 0; i < SPARSES_IN_STAR_HEADER; i++)
        {
            rc = oldgnu_add_sparse (file, &h->star_in_header.sp[i]);
            if (rc != add_ok)
                break;
        }

        ext_p = h->star_in_header.isextended != 0;
    }

    for (; rc == add_ok && ext_p; ext_p = h->star_ext_header.isextended != 0)
    {
        h = tar_find_next_block (archive);
        if (h == NULL)
            return FALSE;

        tar_set_next_block_after (h);

        for (i = 0; i < SPARSES_IN_STAR_EXT_HEADER && rc == add_ok; i++)
            rc = oldgnu_add_sparse (file, &h->star_ext_header.sp[i]);

        file->dumped_size += BLOCKSIZE;
    }

    return (rc != add_fail);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
pax_sparse_member_p (struct tar_sparse_file *file)
{
    return file->stat_info->sparse_map != NULL && file->stat_info->sparse_map->len > 0
        && file->stat_info->sparse_major > 0;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
pax_decode_header (tar_super_t *archive, struct tar_sparse_file *file)
{
    if (file->stat_info->sparse_major > 0)
    {
        uintmax_t u;
        char nbuf[UINTMAX_STRSIZE_BOUND];
        union block *blk;
        char *p;
        size_t sparse_map_len;
        size_t i;
        off_t start;

        start = tar_current_block_ordinal (archive);
        tar_set_next_block_after (current_header);
        blk = tar_find_next_block (archive);
        if (blk == NULL)
            /* unexpected EOF in archive */
            return FALSE;
        p = blk->buffer;
        COPY_BUF (archive, blk, nbuf, p);

        if (!decode_num (&u, nbuf, SIZE_MAX))
        {
            /* malformed sparse archive member */
            return FALSE;
        }

        if (file->stat_info->sparse_map == NULL)
            file->stat_info->sparse_map =
                g_array_sized_new (FALSE, FALSE, sizeof (struct sp_array), u);
        else
            g_array_set_size (file->stat_info->sparse_map, u);

        sparse_map_len = u;

        for (i = 0; i < sparse_map_len; i++)
        {
            struct sp_array sp;
            off_t size;

            COPY_BUF (archive, blk, nbuf, p);
            if (!decode_num (&u, nbuf, TYPE_MAXIMUM (off_t)))
            {
                /* malformed sparse archive member */
                return FALSE;
            }
            sp.offset = u;
            COPY_BUF (archive, blk, nbuf, p);
            if (!decode_num (&u, nbuf, TYPE_MAXIMUM (off_t)) || ckd_add (&size, sp.offset, u)
                || file->stat_info->stat.st_size < size)
            {
                /* malformed sparse archive member */
                return FALSE;
            }
            sp.numbytes = u;
            sparse_add_map (file->stat_info, &sp);
        }

        tar_set_next_block_after (blk);

        file->dumped_size += BLOCKSIZE * (tar_current_block_ordinal (archive) - start);
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
tar_sparse_member_p (tar_super_t *archive, struct tar_stat_info *st)
{
    struct tar_sparse_file file;

    if (!sparse_init (archive, &file))
        return FALSE;

    file.stat_info = st;
    return sparse_member_p (&file);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
tar_sparse_fixup_header (tar_super_t *archive, struct tar_stat_info *st)
{
    struct tar_sparse_file file;

    if (!sparse_init (archive, &file))
        return FALSE;

    file.stat_info = st;
    return sparse_fixup_header (&file);
}

/* --------------------------------------------------------------------------------------------- */

enum dump_status
tar_sparse_skip_file (tar_super_t *archive, struct tar_stat_info *st)
{
    gboolean rc = TRUE;
    struct tar_sparse_file file;

    if (!sparse_init (archive, &file))
        return dump_status_not_implemented;

    file.stat_info = st;
    file.fd = -1;

    rc = sparse_decode_header (archive, &file);
    (void) tar_skip_file (archive, file.stat_info->archive_file_size - file.dumped_size);
    return (sparse_done (&file) && rc) ? dump_status_ok : dump_status_short;
}

/* --------------------------------------------------------------------------------------------- */
