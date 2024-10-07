/*
   Virtual File System: GNU Tar file system.

   Copyright (C) 2023-2024
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
 * \author Andrew Borodin
 * \date 2022
 */

#include <config.h>

#include <ctype.h>              /* isdigit() */
#include <inttypes.h>           /* uintmax_t */

#include "lib/global.h"
#include "lib/widget.h"         /* message() */
#include "lib/vfs/vfs.h"        /* mc_read() */

#include "tar-internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/* Log base 2 of common values. */
#define LG_8 3
#define LG_256 8

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* Table of base-64 digit values + 1, indexed by unsigned chars.
   See Internet RFC 2045 Table 1.
   Zero entries are for unsigned chars that are not base-64 digits.  */
/* *INDENT-OFF* */
static char const base64_map[UCHAR_MAX + 1] =
{
    ['A'] =  0 + 1, ['B'] =  1 + 1, ['C'] =  2 + 1, ['D'] =  3 + 1,
    ['E'] =  4 + 1, ['F'] =  5 + 1, ['G'] =  6 + 1, ['H'] =  7 + 1,
    ['I'] =  8 + 1, ['J'] =  9 + 1, ['K'] = 10 + 1, ['L'] = 11 + 1,
    ['M'] = 12 + 1, ['N'] = 13 + 1, ['O'] = 14 + 1, ['P'] = 15 + 1,
    ['Q'] = 16 + 1, ['R'] = 17 + 1, ['S'] = 18 + 1, ['T'] = 19 + 1,
    ['U'] = 20 + 1, ['V'] = 21 + 1, ['W'] = 22 + 1, ['X'] = 23 + 1,
    ['Y'] = 24 + 1, ['Z'] = 25 + 1,
    ['a'] = 26 + 1, ['b'] = 27 + 1, ['c'] = 28 + 1, ['d'] = 29 + 1,
    ['e'] = 30 + 1, ['f'] = 31 + 1, ['g'] = 32 + 1, ['h'] = 33 + 1,
    ['i'] = 34 + 1, ['j'] = 35 + 1, ['k'] = 36 + 1, ['l'] = 37 + 1,
    ['m'] = 38 + 1, ['n'] = 39 + 1, ['o'] = 40 + 1, ['p'] = 41 + 1,
    ['q'] = 42 + 1, ['r'] = 43 + 1, ['s'] = 44 + 1, ['t'] = 45 + 1,
    ['u'] = 46 + 1, ['v'] = 47 + 1, ['w'] = 48 + 1, ['x'] = 49 + 1,
    ['y'] = 50 + 1, ['z'] = 51 + 1,
    ['0'] = 52 + 1, ['1'] = 53 + 1, ['2'] = 54 + 1, ['3'] = 55 + 1,
    ['4'] = 56 + 1, ['5'] = 57 + 1, ['6'] = 58 + 1, ['7'] = 59 + 1,
    ['8'] = 60 + 1, ['9'] = 61 + 1,
    ['+'] = 62 + 1, ['/'] = 63 + 1,
};
/* *INDENT-ON* */

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static gboolean
tar_short_read (size_t status, tar_super_t *archive)
{
    size_t left;                /* bytes left */
    char *more;                 /* pointer to next byte to read */

    more = archive->record_start->buffer + status;
    left = record_size - status;

    while (left % BLOCKSIZE != 0 || (left != 0 && status != 0))
    {
        if (status != 0)
        {
            ssize_t r;

            r = mc_read (archive->fd, more, left);
            if (r == -1)
                return FALSE;

            status = (size_t) r;
        }

        if (status == 0)
            break;

        left -= status;
        more += status;
    }

    record_end = archive->record_start + (record_size - left) / BLOCKSIZE;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
tar_flush_read (tar_super_t *archive)
{
    size_t status;

    status = mc_read (archive->fd, archive->record_start->buffer, record_size);
    if ((idx_t) status == record_size)
        return TRUE;

    return tar_short_read (status, archive);
}

/* --------------------------------------------------------------------------------------------- */

/**  Flush the current buffer from the archive.
 */
static gboolean
tar_flush_archive (tar_super_t *archive)
{
    record_start_block += record_end - archive->record_start;
    current_block = archive->record_start;
    record_end = archive->record_start + blocking_factor;

    return tar_flush_read (archive);
}

/* --------------------------------------------------------------------------------------------- */

static off_t
tar_seek_archive (tar_super_t *archive, off_t size)
{
    off_t start, offset;
    off_t nrec, nblk;
    off_t skipped;

    /* If low level I/O is already at EOF, do not try to seek further. */
    if (record_end < archive->record_start + blocking_factor)
        return 0;

    skipped = (blocking_factor - (current_block - archive->record_start)) * BLOCKSIZE;
    if (size <= skipped)
        return 0;

    /* Compute number of records to skip */
    nrec = (size - skipped) / record_size;
    if (nrec == 0)
        return 0;

    start = tar_current_block_ordinal (archive);

    offset = mc_lseek (archive->fd, nrec * record_size, SEEK_CUR);
    if (offset < 0)
        return offset;

#if 0
    if ((offset % record_size) != 0)
    {
        message (D_ERROR, MSG_ERROR, _("tar: mc_lseek not stopped at a record boundary"));
        return -1;
    }
#endif

    /* Convert to number of records */
    offset /= BLOCKSIZE;
    /* Compute number of skipped blocks */
    nblk = offset - start;

    /* Update buffering info */
    record_start_block = offset - blocking_factor;
    current_block = record_end;

    return nblk;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
is_octal_digit (char c)
{
    return '0' <= c && c <= '7';
}

/* --------------------------------------------------------------------------------------------- */

void
tar_assign_string (char **string, char *value)
{
    g_free (*string);
    *string = value;
}

/* --------------------------------------------------------------------------------------------- */

void
tar_assign_string_dup (char **string, const char *value)
{
    g_free (*string);
    *string = g_strdup (value);
}

/* --------------------------------------------------------------------------------------------- */

void
tar_assign_string_dup_n (char **string, const char *value, size_t n)
{
    g_free (*string);
    *string = g_strndup (value, n);
}

/* --------------------------------------------------------------------------------------------- */

/* Convert a prefix of the string @arg to a system integer type. If @arglim, set *@arglim to point
   to just after the prefix. If @overflow, set *@overflow to TRUE or FALSE depending on whether
   the input is out of @minval..@maxval range. If the input is out of that range, return an extreme
   value. @minval must not be positive.

   If @minval is negative, @maxval can be at most INTMAX_MAX, and negative integers @minval .. -1
   are assumed to be represented using leading '-' in the usual way. If the represented value
   exceeds INTMAX_MAX, return a negative integer V such that (uintmax_t) V yields the represented
   value.

   On conversion error: if @arglim set *@arglim = @arg if @overflow set *@overflow = FALSE;
   then return 0.

   Sample call to this function:

   char *s_end;
   gboolean overflow;
   idx_t i;

   i = stoint (s, &s_end, &overflow, 0, IDX_MAX);
   if ((s_end == s) | (s_end == '\0') | overflow)
   diagnose_invalid (s);

   This example uses "|" instead of "||" for fewer branches at runtime,
   which tends to be more efficient on modern processors.

   This function is named "stoint" instead of "strtoint" because
   <string.h> reserves names beginning with "str".
 */
#if ! (INTMAX_MAX <= UINTMAX_MAX)
#error "strtosysint: nonnegative intmax_t does not fit in uintmax_t"
#endif
intmax_t
stoint (const char *arg, char **arglim, gboolean *overflow, intmax_t minval, uintmax_t maxval)
{
    char const *p = arg;
    intmax_t i;
    int v = 0;

    if (isdigit (*p))
    {
        if (minval <= 0)
        {
            i = *p - '0';

            while (isdigit (*++p) != 0)
            {
                v |= ckd_mul (&i, i, 10) ? 1 : 0;
                v |= ckd_add (&i, i, *p - '0') ? 1 : 0;
            }

            v |= maxval < (uintmax_t) i ? 1 : 0;
            if (v != 0)
                i = maxval;
        }
        else
        {
            uintmax_t u = *p - '0';

            while (isdigit (*++p) != 0)
            {
                v |= ckd_mul (&u, u, 10) ? 1 : 0;
                v |= ckd_add (&u, u, *p - '0') ? 1 : 0;
            }

            v |= maxval < u ? 1 : 0;
            if (v != 0)
                u = maxval;
            i = tar_represent_uintmax (u);
        }
    }
    else if (minval < 0 && *p == '-' && isdigit (p[1]))
    {
        p++;
        i = -(*p - '0');

        while (isdigit (*++p) != 0)
        {
            v |= ckd_mul (&i, i, 10) ? 1 : 0;
            v |= ckd_sub (&i, i, *p - '0') ? 1 : 0;
        }

        v |= i < minval ? 1 : 0;
        if (v != 0)
            i = minval;
    }
    else
        i = 0;

    if (arglim != NULL)
        *arglim = (char *) p;
    if (overflow != NULL)
        *overflow = v != 0;
    return i;
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
intmax_t
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

        if (!g_ascii_isspace (*where))
            break;

        where++;
    }

    if (is_octal_digit (*where))
    {
        char const *where1 = where;
        gboolean overflow = FALSE;

        while (TRUE)
        {
            value += *where++ - '0';
            if (where == lim || !is_octal_digit (*where))
                break;
            overflow |= ckd_mul (&value, value, 8);
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
                if (where == lim || !is_octal_digit (*where))
                    break;
                digit = *where - '0';
                overflow |= ckd_mul (&value, value, 8);
            }

            overflow |= ckd_add (&value, value, 1);

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

        negative = *where++ == '-';

        while (where != lim)
        {
            unsigned char uc = *where;
            char dig;

            dig = base64_map[uc];
            if (dig <= 0)
                break;

            if (ckd_mul (&value, value, 64))
                return (-1);
            value |= dig - 1;
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
        topbits = ((uintmax_t) - signbit) << (UINTMAX_WIDTH - LG_256 - (LG_256 - 2));

        value = (*where++ & ((1 << (LG_256 - 2)) - 1)) - signbit;

        while (TRUE)
        {
            unsigned char uc;

            uc = *where++;
            value = (value << LG_256) + uc;
            if (where == lim)
                break;

            if (((value << LG_256 >> LG_256) | topbits) != value)
                return (-1);
        }

        negative = signbit != 0;
        if (negative)
            value = -value;
    }

    if (where != lim && *where != '\0' && !g_ascii_isspace (*where))
        return (-1);

    if (value <= (negative ? minus_minval : maxval))
        return tar_represent_uintmax (negative ? -value : value);

    return (-1);
}

/* --------------------------------------------------------------------------------------------- */

off_t
off_from_header (const char *p, size_t s)
{
    /* Negative offsets are not allowed in tar files, so invoke
       from_header with minimum value 0, not TYPE_MINIMUM (off_t). */
    return tar_from_header (p, s, "off_t", 0, TYPE_MAXIMUM (off_t), FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Return the location of the next available input or output block.
 * Return NULL for EOF.
 */
union block *
tar_find_next_block (tar_super_t *archive)
{
    if (current_block == record_end)
    {
        if (hit_eof)
            return NULL;

        if (!tar_flush_archive (archive))
        {
            message (D_ERROR, MSG_ERROR, _("Inconsistent tar archive"));
            return NULL;
        }

        if (current_block == record_end)
        {
            hit_eof = TRUE;
            return NULL;
        }
    }

    return current_block;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Indicate that we have used all blocks up thru @block.
 */
gboolean
tar_set_next_block_after (union block *block)
{
    while (block >= current_block)
        current_block++;

    /* Do *not* flush the archive here. If we do, the same argument to tar_set_next_block_after()
       could mean the next block (if the input record is exactly one block long), which is not
       what is intended.  */

    return !(current_block > record_end);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Compute and return the block ordinal at current_block.
 */
off_t
tar_current_block_ordinal (const tar_super_t *archive)
{
    return record_start_block + (current_block - archive->record_start);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Skip over @size bytes of data in blocks in the archive.
 */
gboolean
tar_skip_file (tar_super_t *archive, off_t size)
{
    union block *x;
    off_t nblk;

    nblk = tar_seek_archive (archive, size);
    if (nblk >= 0)
        size -= nblk * BLOCKSIZE;

    while (size > 0)
    {
        x = tar_find_next_block (archive);
        if (x == NULL)
            return FALSE;

        tar_set_next_block_after (x);
        size -= BLOCKSIZE;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
