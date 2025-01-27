/* A more useful interface to strtol.

   Copyright (C) 1995-2025
   Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Jim Meyering. */

#include <config.h>

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "lib/strutil.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static strtol_error_t
bkm_scale (uintmax_t *x, int scale_factor)
{
    if (UINTMAX_MAX / scale_factor < *x)
    {
        *x = UINTMAX_MAX;
        return LONGINT_OVERFLOW;
    }

    *x *= scale_factor;
    return LONGINT_OK;
}

/* --------------------------------------------------------------------------------------------- */

static strtol_error_t
bkm_scale_by_power (uintmax_t *x, int base, int power)
{
    strtol_error_t err = LONGINT_OK;
    while (power-- != 0)
        err |= bkm_scale (x, base);
    return err;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* Act like the system's strtol (NPTR, ENDPTR, BASE) except:
   - The TYPE of the result might be something other than long int.
   - Return strtol_error, and store any result through an additional
     TYPE *VAL pointer instead of returning the result.
   - If TYPE is unsigned, reject leading '-'.
   - Behavior is undefined if BASE is negative, 1, or greater than 36.
     (In this respect xstrtol acts like the C standard, not like POSIX.)
   - Accept an additional char const *VALID_SUFFIXES pointer to a
     possibly-empty string containing allowed numeric suffixes,
     which multiply the value.  These include SI suffixes like 'k' and 'M';
     these normally stand for powers of 1024, but if VALID_SUFFIXES also
     includes '0' they can be followed by "B" to stand for the usual
     SI powers of 1000 (or by "iB" to stand for powers of 1024 as before).
     Other supported suffixes include 'K' for 1024 or 1000, 'b' for 512,
     'c' for 1, and 'w' for 2.
   - Suppose that after the initial whitespace, the number is missing
     but there is a valid suffix.  Then the number is treated as 1.
*/
strtol_error_t
xstrtoumax (const char *nptr, char **endptr, int base, uintmax_t *val, const char *valid_suffixes)
{
    char *t_ptr;
    char **p;
    uintmax_t tmp;
    strtol_error_t err = LONGINT_OK;

    p = endptr != NULL ? endptr : &t_ptr;

    {
        const char *q = nptr;
        unsigned char ch = *q;

        while (isspace (ch))
            ch = *++q;

        if (ch == '-')
        {
            *p = (char *) nptr;
            return LONGINT_INVALID;
        }
    }

    errno = 0;
    tmp = strtol (nptr, p, base);

    if (*p == nptr)
    {
        /* If there is no number but there is a valid suffix, assume the
           number is 1.  The string is invalid otherwise.  */
        if (!(valid_suffixes != NULL && *nptr != '\0' && strchr (valid_suffixes, *nptr) != NULL))
            return LONGINT_INVALID;
        tmp = 1;
    }
    else if (errno != 0)
    {
        if (errno != ERANGE)
            return LONGINT_INVALID;
        err = LONGINT_OVERFLOW;
    }

    // Let valid_suffixes == NULL mean "allow any suffix".
    /* FIXME: update all callers except the ones that allow suffixes
       after the number, changing last parameter NULL to "".  */
    if (valid_suffixes == NULL)
    {
        *val = tmp;
        return err;
    }

    if (**p != '\0')
    {
        int xbase = 1024;
        int suffixes = 1;
        strtol_error_t overflow;

        if (strchr (valid_suffixes, **p) == NULL)
        {
            *val = tmp;
            return err | LONGINT_INVALID_SUFFIX_CHAR;
        }

        switch (**p)
        {
        case 'E':
        case 'G':
        case 'g':
        case 'k':
        case 'K':
        case 'M':
        case 'm':
        case 'P':
        case 'Q':
        case 'R':
        case 'T':
        case 't':
        case 'Y':
        case 'Z':
            if (strchr (valid_suffixes, '0') != NULL)
            {
                /* The "valid suffix" '0' is a special flag meaning that
                   an optional second suffix is allowed, which can change
                   the base.  A suffix "B" (e.g. "100MB") stands for a power
                   of 1000, whereas a suffix "iB" (e.g. "100MiB") stands for
                   a power of 1024.  If no suffix (e.g. "100M"), assume
                   power-of-1024.  */

                switch (p[0][1])
                {
                case 'i':
                    if (p[0][2] == 'B')
                        suffixes += 2;
                    break;

                case 'B':
                case 'D':  // 'D' is obsolescent
                    xbase = 1000;
                    suffixes++;
                    break;
                default:
                    break;
                }
            }
            break;
        default:
            break;
        }

        switch (**p)
        {
        case 'b':
            overflow = bkm_scale (&tmp, 512);
            break;

        case 'B':
            /* This obsolescent first suffix is distinct from the 'B'
               second suffix above.  E.g., 'tar -L 1000B' means change
               the tape after writing 1000 KiB of data.  */
            overflow = bkm_scale (&tmp, 1024);
            break;

        case 'c':
            overflow = LONGINT_OK;
            break;

        case 'E':  // exa or exbi
            overflow = bkm_scale_by_power (&tmp, xbase, 6);
            break;

        case 'G':  // giga or gibi
        case 'g':  // 'g' is undocumented; for compatibility only
            overflow = bkm_scale_by_power (&tmp, xbase, 3);
            break;

        case 'k':  // kilo
        case 'K':  // kibi
            overflow = bkm_scale_by_power (&tmp, xbase, 1);
            break;

        case 'M':  // mega or mebi
        case 'm':  // 'm' is undocumented; for compatibility only
            overflow = bkm_scale_by_power (&tmp, xbase, 2);
            break;

        case 'P':  // peta or pebi
            overflow = bkm_scale_by_power (&tmp, xbase, 5);
            break;

        case 'Q':  // quetta or 2**100
            overflow = bkm_scale_by_power (&tmp, xbase, 10);
            break;

        case 'R':  // ronna or 2**90
            overflow = bkm_scale_by_power (&tmp, xbase, 9);
            break;

        case 'T':  // tera or tebi
        case 't':  // 't' is undocumented; for compatibility only
            overflow = bkm_scale_by_power (&tmp, xbase, 4);
            break;

        case 'w':
            overflow = bkm_scale (&tmp, 2);
            break;

        case 'Y':  // yotta or 2**80
            overflow = bkm_scale_by_power (&tmp, xbase, 8);
            break;

        case 'Z':  // zetta or 2**70
            overflow = bkm_scale_by_power (&tmp, xbase, 7);
            break;

        default:
            *val = tmp;
            return err | LONGINT_INVALID_SUFFIX_CHAR;
        }

        err |= overflow;
        *p += suffixes;
        if (**p != '\0')
            err |= LONGINT_INVALID_SUFFIX_CHAR;
    }

    *val = tmp;
    return err;
}

/* --------------------------------------------------------------------------------------------- */
