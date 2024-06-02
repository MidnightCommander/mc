/*
   Virtual File System: GNU Tar file system.

   Copyright (C) 1995-2024
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

#include <config.h>

#include <ctype.h>              /* isdigit() */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "lib/global.h"
#include "lib/util.h"           /* MC_PTR_FREE */

#include "tar-internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define XHDR_PROTECTED 0x01
#define XHDR_GLOBAL    0x02

/*** file scope type declarations ****************************************************************/

/* General Interface */

/* Since tar VFS is read-only, inplement decodes only */
/* *INDENT-OFF* */
struct xhdr_tab
{
    const char *keyword;
    gboolean (*decoder) (struct tar_stat_info * st, const char *keyword, const char *arg, size_t size);
    int flags;
};
/* *INDENT-ON* */

/* Keyword options */
struct keyword_item
{
    char *pattern;
    char *value;
};

enum decode_record_status
{
    decode_record_ok,
    decode_record_finish,
    decode_record_fail
};

/*** forward declarations (file scope functions) *************************************************/

static gboolean dummy_decoder (struct tar_stat_info *st, const char *keyword, const char *arg,
                               size_t size);
static gboolean atime_decoder (struct tar_stat_info *st, const char *keyword, const char *arg,
                               size_t size);
static gboolean gid_decoder (struct tar_stat_info *st, const char *keyword, const char *arg,
                             size_t size);
#if 0
static gboolean gname_decoder (struct tar_stat_info *st, const char *keyword, const char *arg,
                               size_t size);
#endif
static gboolean linkpath_decoder (struct tar_stat_info *st, const char *keyword, const char *arg,
                                  size_t size);
static gboolean mtime_decoder (struct tar_stat_info *st, const char *keyword, const char *arg,
                               size_t size);
static gboolean ctime_decoder (struct tar_stat_info *st, const char *keyword, const char *arg,
                               size_t size);
static gboolean path_decoder (struct tar_stat_info *st, const char *keyword, const char *arg,
                              size_t size);
static gboolean size_decoder (struct tar_stat_info *st, const char *keyword, const char *arg,
                              size_t size);
static gboolean uid_decoder (struct tar_stat_info *st, const char *keyword, const char *arg,
                             size_t size);
#if 0
static gboolean uname_decoder (struct tar_stat_info *st, const char *keyword, const char *arg,
                               size_t size);
#endif
static gboolean sparse_path_decoder (struct tar_stat_info *st, const char *keyword, const char *arg,
                                     size_t size);
static gboolean sparse_major_decoder (struct tar_stat_info *st, const char *keyword,
                                      const char *arg, size_t size);
static gboolean sparse_minor_decoder (struct tar_stat_info *st, const char *keyword,
                                      const char *arg, size_t size);
static gboolean sparse_size_decoder (struct tar_stat_info *st, const char *keyword, const char *arg,
                                     size_t size);
static gboolean sparse_numblocks_decoder (struct tar_stat_info *st, const char *keyword,
                                          const char *arg, size_t size);
static gboolean sparse_offset_decoder (struct tar_stat_info *st, const char *keyword,
                                       const char *arg, size_t size);
static gboolean sparse_numbytes_decoder (struct tar_stat_info *st, const char *keyword,
                                         const char *arg, size_t size);
static gboolean sparse_map_decoder (struct tar_stat_info *st, const char *keyword, const char *arg,
                                    size_t size);
static gboolean dumpdir_decoder (struct tar_stat_info *st, const char *keyword, const char *arg,
                                 size_t size);

/*** file scope variables ************************************************************************/

enum
{
    BILLION = 1000000000,
    LOG10_BILLION = 9
};

/* *INDENT-OFF* */
static struct xhdr_tab xhdr_tab[] =
{
    { "atime",                atime_decoder,            0  },
    { "comment",              dummy_decoder,            0  },
    { "charset",              dummy_decoder,            0  },
    { "ctime",                ctime_decoder,            0  },
    { "gid",                  gid_decoder,              0  },
#if 0
    { "gname",                gname_decoder,            0  },
#endif
    { "linkpath",             linkpath_decoder,         0  },
    { "mtime",                mtime_decoder,            0  },
    { "path",                 path_decoder,             0  },
    { "size",                 size_decoder,             0  },
    { "uid",                  uid_decoder,              0  },
#if 0
    { "uname",                uname_decoder,            0  },
#endif

    /* Sparse file handling */
    { "GNU.sparse.name",      sparse_path_decoder,      XHDR_PROTECTED },
    { "GNU.sparse.major",     sparse_major_decoder,     XHDR_PROTECTED },
    { "GNU.sparse.minor",     sparse_minor_decoder,     XHDR_PROTECTED },
    { "GNU.sparse.realsize",  sparse_size_decoder,      XHDR_PROTECTED },
    { "GNU.sparse.numblocks", sparse_numblocks_decoder, XHDR_PROTECTED },

    { "GNU.sparse.size",      sparse_size_decoder,      XHDR_PROTECTED },
    /* tar 1.14 - 1.15.1 keywords. Multiple instances of these appeared in 'x'
       headers, and each of them was meaningful. It confilcted with POSIX specs,
       which requires that "when extended header records conflict, the last one
       given in the header shall take precedence." */
    { "GNU.sparse.offset",    sparse_offset_decoder,    XHDR_PROTECTED },
    { "GNU.sparse.numbytes",  sparse_numbytes_decoder,  XHDR_PROTECTED },
    /* tar 1.15.90 keyword, introduced to remove the above-mentioned conflict. */
    { "GNU.sparse.map",       sparse_map_decoder,       0 },

    { "GNU.dumpdir",          dumpdir_decoder,          XHDR_PROTECTED },

    { NULL,                   NULL,                     0 }
};
/* *INDENT-ON* */

/* List of keyword/value pairs decoded from the last 'g' type header */
static GSList *global_header_override_list = NULL;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* Convert a prefix of the string @arg to a system integer type whose minimum value is @minval
   and maximum @maxval. If @minval is negative, negative integers @minval .. -1 are assumed
   to be represented using leading '-' in the usual way. If the represented value exceeds INTMAX_MAX,
   return a negative integer V such that (uintmax_t) V yields the represented value. If @arglim is
   nonnull, store into *@arglim a pointer to the first character after the prefix.

   This is the inverse of sysinttostr.

   On a normal return, set errno = 0.
   On conversion error, return 0 and set errno = EINVAL.
   On overflow, return an extreme value and set errno = ERANGE.
 */
#if ! (INTMAX_MAX <= UINTMAX_MAX)
#error "strtosysint: nonnegative intmax_t does not fit in uintmax_t"
#endif
static intmax_t
strtosysint (const char *arg, char **arglim, intmax_t minval, uintmax_t maxval)
{
    errno = 0;

    if (maxval <= INTMAX_MAX)
    {
        if (isdigit (arg[*arg == '-' ? 1 : 0]))
        {
            gint64 i;

            i = g_ascii_strtoll (arg, arglim, 10);
            if ((gint64) minval <= i && i <= (gint64) maxval)
                return (intmax_t) i;

            errno = ERANGE;
            return i < (gint64) minval ? minval : (intmax_t) maxval;
        }
    }
    else
    {
        if (isdigit (*arg))
        {
            guint64 i;

            i = g_ascii_strtoull (arg, arglim, 10);
            if (i <= (guint64) maxval)
                return tar_represent_uintmax ((uintmax_t) i);

            errno = ERANGE;
            return maxval;
        }
    }

    errno = EINVAL;
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static struct xhdr_tab *
locate_handler (const char *keyword)
{
    struct xhdr_tab *p;

    for (p = xhdr_tab; p->keyword != NULL; p++)
        if (strcmp (p->keyword, keyword) == 0)
            return p;

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
keyword_item_run (gpointer data, gpointer user_data)
{
    struct keyword_item *kp = (struct keyword_item *) data;
    struct tar_stat_info *st = (struct tar_stat_info *) user_data;
    struct xhdr_tab const *t;

    t = locate_handler (kp->pattern);
    if (t != NULL)
        return t->decoder (st, t->keyword, kp->value, strlen (kp->value));

    return TRUE;                /* FIXME */
}

/* --------------------------------------------------------------------------------------------- */

static void
keyword_item_free (gpointer data)
{
    struct keyword_item *kp = (struct keyword_item *) data;

    g_free (kp->pattern);
    g_free (kp->value);
    g_free (kp);
}

/* --------------------------------------------------------------------------------------------- */

static void
xheader_list_append (GSList **root, const char *kw, const char *value)
{
    struct keyword_item *kp;

    kp = g_new (struct keyword_item, 1);
    kp->pattern = g_strdup (kw);
    kp->value = g_strdup (value);
    *root = g_slist_prepend (*root, kp);
}

/* --------------------------------------------------------------------------------------------- */

static inline void
xheader_list_destroy (GSList **root)
{
    g_slist_free_full (*root, keyword_item_free);
    *root = NULL;
}

/* --------------------------------------------------------------------------------------------- */

static inline void
run_override_list (GSList *kp, struct tar_stat_info *st)
{
    g_slist_foreach (kp, (GFunc) keyword_item_run, st);
}

/* --------------------------------------------------------------------------------------------- */

static struct timespec
decode_timespec (const char *arg, char **arg_lim, gboolean parse_fraction)
{
    time_t s = TYPE_MINIMUM (time_t);
    int ns = -1;
    const char *p = arg;
    gboolean negative = *arg == '-';
    struct timespec r;

    if (!isdigit (arg[negative]))
        errno = EINVAL;
    else
    {
        errno = 0;

        if (negative)
        {
            gint64 i;

            i = g_ascii_strtoll (arg, arg_lim, 10);
            if (TYPE_SIGNED (time_t) ? TYPE_MINIMUM (time_t) <= i : 0 <= i)
                s = (intmax_t) i;
            else
                errno = ERANGE;
        }
        else
        {
            guint64 i;

            i = g_ascii_strtoull (arg, arg_lim, 10);
            if (i <= TYPE_MAXIMUM (time_t))
                s = (uintmax_t) i;
            else
                errno = ERANGE;
        }

        p = *arg_lim;
        ns = 0;

        if (parse_fraction && *p == '.')
        {
            int digits = 0;
            gboolean trailing_nonzero = FALSE;

            while (isdigit (*++p))
                if (digits < LOG10_BILLION)
                {
                    digits++;
                    ns = 10 * ns + (*p - '0');
                }
                else if (*p != '0')
                    trailing_nonzero = TRUE;

            while (digits < LOG10_BILLION)
            {
                digits++;
                ns *= 10;
            }

            if (negative)
            {
                /* Convert "-1.10000000000001" to s == -2, ns == 89999999.
                   I.e., truncate time stamps towards minus infinity while
                   converting them to internal form.  */
                if (trailing_nonzero)
                    ns++;
                if (ns != 0)
                {
                    if (s == TYPE_MINIMUM (time_t))
                        ns = -1;
                    else
                    {
                        s--;
                        ns = BILLION - ns;
                    }
                }
            }
        }

        if (errno == ERANGE)
            ns = -1;
    }

    *arg_lim = (char *) p;
    r.tv_sec = s;
    r.tv_nsec = ns;
    return r;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
decode_time (struct timespec *ts, const char *arg, const char *keyword)
{
    char *arg_lim;
    struct timespec t;

    (void) keyword;

    t = decode_timespec (arg, &arg_lim, TRUE);

    if (t.tv_nsec < 0)
        /* Malformed extended header */
        return FALSE;

    if (*arg_lim != '\0')
        /* Malformed extended header */
        return FALSE;

    *ts = t;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
decode_signed_num (intmax_t *num, const char *arg, intmax_t minval, uintmax_t maxval,
                   const char *keyword)
{
    char *arg_lim;
    intmax_t u;

    (void) keyword;

    if (!isdigit (*arg))
        return FALSE;           /* malformed extended header */

    u = strtosysint (arg, &arg_lim, minval, maxval);

    if (errno == EINVAL || *arg_lim != '\0')
        return FALSE;           /* malformed extended header */

    if (errno == ERANGE)
        return FALSE;           /* out of range */

    *num = u;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
decode_num (uintmax_t *num, const char *arg, uintmax_t maxval, const char *keyword)
{
    intmax_t i;

    if (!decode_signed_num (&i, arg, 0, maxval, keyword))
        return FALSE;

    *num = i;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
raw_path_decoder (struct tar_stat_info *st, const char *arg)
{
    if (*arg != '\0')
    {
        tar_assign_string_dup (&st->orig_file_name, arg);
        tar_assign_string_dup (&st->file_name, arg);
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
dummy_decoder (struct tar_stat_info *st, const char *keyword, const char *arg, size_t size)
{
    (void) st;
    (void) keyword;
    (void) arg;
    (void) size;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
atime_decoder (struct tar_stat_info *st, const char *keyword, const char *arg, size_t size)
{
    struct timespec ts;

    (void) size;

    if (!decode_time (&ts, arg, keyword))
        return FALSE;

    st->atime = ts;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
gid_decoder (struct tar_stat_info *st, const char *keyword, const char *arg, size_t size)
{
    intmax_t u;

    (void) size;

    if (!decode_signed_num (&u, arg, TYPE_MINIMUM (gid_t), TYPE_MINIMUM (gid_t), keyword))
        return FALSE;

    st->stat.st_gid = u;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

#if 0
static gboolean
gname_decoder (struct tar_stat_info *st, const char *keyword, const char *arg, size_t size)
{
    (void) keyword;
    (void) size;

    tar_assign_string_dup (&st->gname, arg);
    return TRUE;
}
#endif

/* --------------------------------------------------------------------------------------------- */

static gboolean
linkpath_decoder (struct tar_stat_info *st, const char *keyword, const char *arg, size_t size)
{
    (void) keyword;
    (void) size;

    tar_assign_string_dup (&st->link_name, arg);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
ctime_decoder (struct tar_stat_info *st, const char *keyword, const char *arg, size_t size)
{
    struct timespec ts;

    (void) size;

    if (!decode_time (&ts, arg, keyword))
        return FALSE;

    st->ctime = ts;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mtime_decoder (struct tar_stat_info *st, const char *keyword, const char *arg, size_t size)
{
    struct timespec ts;

    (void) size;

    if (!decode_time (&ts, arg, keyword))
        return FALSE;

    st->mtime = ts;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
path_decoder (struct tar_stat_info *st, const char *keyword, const char *arg, size_t size)
{
    (void) keyword;
    (void) size;

    if (!st->sparse_name_done)
        return raw_path_decoder (st, arg);

    return TRUE;                /* FIXME */
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
size_decoder (struct tar_stat_info *st, const char *keyword, const char *arg, size_t size)
{
    uintmax_t u;

    (void) size;

    if (!decode_num (&u, arg, TYPE_MAXIMUM (off_t), keyword))
        return FALSE;

    st->stat.st_size = u;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
uid_decoder (struct tar_stat_info *st, const char *keyword, const char *arg, size_t size)
{
    intmax_t u;

    (void) size;

    if (!decode_signed_num (&u, arg, TYPE_MINIMUM (uid_t), TYPE_MAXIMUM (uid_t), keyword))
        return FALSE;

    st->stat.st_uid = u;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

#if 0
static gboolean
uname_decoder (struct tar_stat_info *st, const char *keyword, const char *arg, size_t size)
{
    (void) keyword;
    (void) size;

    tar_assign_string_dup (&st->uname, arg);
    return TRUE;
}
#endif

/* --------------------------------------------------------------------------------------------- */

static gboolean
dumpdir_decoder (struct tar_stat_info *st, const char *keyword, const char *arg, size_t size)
{
    (void) keyword;

#if GLIB_CHECK_VERSION (2, 68, 0)
    st->dumpdir = g_memdup2 (arg, size);
#else
    st->dumpdir = g_memdup (arg, size);
#endif
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Decodes a single extended header record, advancing @ptr to the next record.
 *
 * @param p pointer to extended header record
 * @param st stat info
 *
 * @return decode_record_ok or decode_record_finish on success, decode_record_fail otherwize
 */
static enum decode_record_status
decode_record (struct xheader *xhdr, char **ptr,
               gboolean (*handler) (void *data, const char *keyword, const char *value,
                                    size_t size), void *data)
{
    char *start = *ptr;
    char *p = start;
    size_t len;
    char *len_lim;
    const char *keyword;
    char *nextp;
    size_t len_max;
    gboolean ret;

    len_max = xhdr->buffer + xhdr->size - start;

    while (*p == ' ' || *p == '\t')
        p++;

    if (!isdigit (*p))
        return (*p != '\0' ? decode_record_fail : decode_record_finish);

    len = (uintmax_t) g_ascii_strtoull (p, &len_lim, 10);
    if (len_max < len)
        return decode_record_fail;

    nextp = start + len;

    for (p = len_lim; *p == ' ' || *p == '\t'; p++)
        ;

    if (p == len_lim)
        return decode_record_fail;

    keyword = p;
    p = strchr (p, '=');
    if (!(p != NULL && p < nextp))
        return decode_record_fail;

    if (nextp[-1] != '\n')
        return decode_record_fail;

    *p = nextp[-1] = '\0';
    ret = handler (data, keyword, p + 1, nextp - p - 2);        /* '=' + trailing '\n' */
    *p = '=';
    nextp[-1] = '\n';
    *ptr = nextp;

    return (ret ? decode_record_ok : decode_record_fail);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
decg (void *data, const char *keyword, const char *value, size_t size)
{
    GSList **kwl = (GSList **) data;
    struct xhdr_tab const *tab;

    (void) size;

    tab = locate_handler (keyword);
    if (tab != NULL && (tab->flags & XHDR_GLOBAL) != 0)
    {
        if (!tab->decoder (data, keyword, value, size))
            return FALSE;
    }
    else
        xheader_list_append (kwl, keyword, value);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
decx (void *data, const char *keyword, const char *value, size_t size)
{
    struct keyword_item kp = {
        .pattern = (char *) keyword,
        .value = (char *) value
    };

    (void) size;

    return keyword_item_run (&kp, data);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
sparse_path_decoder (struct tar_stat_info *st, const char *keyword, const char *arg, size_t size)
{
    (void) keyword;
    (void) size;

    st->sparse_name_done = TRUE;
    return raw_path_decoder (st, arg);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
sparse_major_decoder (struct tar_stat_info *st, const char *keyword, const char *arg, size_t size)
{
    uintmax_t u;

    (void) size;

    if (!decode_num (&u, arg, TYPE_MAXIMUM (unsigned), keyword))
          return FALSE;

    st->sparse_major = u;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
sparse_minor_decoder (struct tar_stat_info *st, const char *keyword, const char *arg, size_t size)
{
    uintmax_t u;

    (void) size;

    if (!decode_num (&u, arg, TYPE_MAXIMUM (unsigned), keyword))
          return FALSE;

    st->sparse_minor = u;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
sparse_size_decoder (struct tar_stat_info *st, const char *keyword, const char *arg, size_t size)
{
    uintmax_t u;

    (void) size;

    if (!decode_num (&u, arg, TYPE_MAXIMUM (off_t), keyword))
        return FALSE;

    st->real_size_set = TRUE;
    st->real_size = u;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
sparse_numblocks_decoder (struct tar_stat_info *st, const char *keyword, const char *arg,
                          size_t size)
{
    uintmax_t u;

    (void) size;

    if (!decode_num (&u, arg, SIZE_MAX, keyword))
        return FALSE;

    if (st->sparse_map == NULL)
        st->sparse_map = g_array_sized_new (FALSE, FALSE, sizeof (struct sp_array), u);
    else
        g_array_set_size (st->sparse_map, u);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
sparse_offset_decoder (struct tar_stat_info *st, const char *keyword, const char *arg, size_t size)
{
    uintmax_t u;
    struct sp_array *s;

    (void) size;

    if (!decode_num (&u, arg, TYPE_MAXIMUM (off_t), keyword))
        return FALSE;

    s = &g_array_index (st->sparse_map, struct sp_array, st->sparse_map->len - 1);
    s->offset = u;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
sparse_numbytes_decoder (struct tar_stat_info *st, const char *keyword, const char *arg,
                         size_t size)
{
    uintmax_t u;
    struct sp_array s;

    (void) size;

    if (!decode_num (&u, arg, SIZE_MAX, keyword))
        return FALSE;

    s.offset = 0;
    s.numbytes = u;
    g_array_append_val (st->sparse_map, s);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
sparse_map_decoder (struct tar_stat_info *st, const char *keyword, const char *arg, size_t size)
{
    gboolean offset = TRUE;
    struct sp_array e;

    (void) keyword;
    (void) size;

    if (st->sparse_map != NULL)
        g_array_set_size (st->sparse_map, 0);

    while (TRUE)
    {
        gint64 u;
        char *delim;

        if (!isdigit (*arg))
        {
            /* malformed extended header */
            return FALSE;
        }

        errno = 0;
        u = g_ascii_strtoll (arg, &delim, 10);
        if (TYPE_MAXIMUM (off_t) < u)
        {
            u = TYPE_MAXIMUM (off_t);
            errno = ERANGE;
        }
        if (offset)
        {
            e.offset = u;
            if (errno == ERANGE)
            {
                /* out of range */
                return FALSE;
            }
        }
        else
        {
            e.numbytes = u;
            if (errno == ERANGE)
            {
                /* out of range */
                return FALSE;
            }

            g_array_append_val (st->sparse_map, e);
        }

        offset = !offset;

        if (*delim == '\0')
            break;
        if (*delim != ',')
        {
            /* malformed extended header */
            return FALSE;
        }

        arg = delim + 1;
    }

    if (!offset)
    {
        /* malformed extended header */
        return FALSE;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Decodes an extended header.
 *
 * @param st stat info
 *
 * @return TRUE on success, FALSE otherwize
 */
gboolean
tar_xheader_decode (struct tar_stat_info *st)
{
    char *p;
    enum decode_record_status status;

    run_override_list (global_header_override_list, st);

    p = st->xhdr.buffer + BLOCKSIZE;

    while ((status = decode_record (&st->xhdr, &p, decx, st)) == decode_record_ok)
        ;

    if (status == decode_record_fail)
        return FALSE;

    /* The archived (effective) file size is always set directly in tar header
       field, possibly overridden by "size" extended header - in both cases,
       result is now decoded in st->stat.st_size */
    st->archive_file_size = st->stat.st_size;

    /* The real file size (given by stat()) may be redefined for sparse
       files in "GNU.sparse.realsize" extended header */
    if (st->real_size_set)
        st->stat.st_size = st->real_size;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
tar_xheader_read (tar_super_t *archive, struct xheader *xhdr, union block *p, off_t size)
{
    size_t j = 0;

    size = MAX (0, size);
    size += BLOCKSIZE;

    xhdr->size = size;
    xhdr->buffer = g_malloc (size + 1);
    xhdr->buffer[size] = '\0';

    do
    {
        size_t len;

        if (p == NULL)
            return FALSE;       /* Unexpected EOF in archive */

        len = MIN (size, BLOCKSIZE);

        memcpy (xhdr->buffer + j, p->buffer, len);
        tar_set_next_block_after (p);
        p = tar_find_next_block (archive);

        j += len;
        size -= len;
    }
    while (size > 0);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
tar_xheader_decode_global (struct xheader *xhdr)
{
    char *p;
    gboolean ret;

    p = xhdr->buffer + BLOCKSIZE;

    xheader_list_destroy (&global_header_override_list);

    while ((ret = decode_record (xhdr, &p, decg, &global_header_override_list)) == decode_record_ok)
        ;

    return (ret == decode_record_finish);
}

/* --------------------------------------------------------------------------------------------- */

void
tar_xheader_destroy (struct xheader *xhdr)
{
    MC_PTR_FREE (xhdr->buffer);
    xhdr->size = 0;
}

/* --------------------------------------------------------------------------------------------- */
