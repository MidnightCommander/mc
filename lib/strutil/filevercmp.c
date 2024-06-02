/*
   Copyright (C) 1995 Ian Jackson <iwj10@cus.cam.ac.uk>
   Copyright (C) 2001 Anthony Towns <aj@azure.humbug.org.au>
   Copyright (C) 2008-2022 Free Software Foundation, Inc.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <stdlib.h>
#include <limits.h>

#include "lib/strutil.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* Return the length of a prefix of @s that corresponds to the suffix defined by this extended
 * regular expression in the C locale: (\.[A-Za-z~][A-Za-z0-9~]*)*$
 *
 * Use the longest suffix matching this regular expression, except do not use all of s as a suffix
 * if s is nonempty.
 *
 * If *len is -1, s is a string; set *lem to s's length.
 * Otherwise, *len should be nonnegative, s is a char array, and *len does not change.
 */
static ssize_t
file_prefixlen (const char *s, ssize_t *len)
{
    size_t n = (size_t) (*len); /* SIZE_MAX if N == -1 */
    size_t i = 0;
    size_t prefixlen = 0;

    while (TRUE)
    {
        gboolean done;

        if (*len < 0)
            done = s[i] == '\0';
        else
            done = i == n;

        if (done)
        {
            *len = (ssize_t) i;
            return (ssize_t) prefixlen;
        }

        i++;
        prefixlen = i;

        while (i + 1 < n && s[i] == '.' && (g_ascii_isalpha (s[i + 1]) || s[i + 1] == '~'))
            for (i += 2; i < n && (g_ascii_isalnum (s[i]) || s[i] == '~'); i++)
                ;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* Return a version sort comparison value for @s's byte at position @pos.
 *
 * @param s a string
 * @param pos a position in @s
 * @param len a length of @s. If @pos == @len, sort before all non-'~' bytes.
 */

static int
order (const char *s, size_t pos, size_t len)
{
    unsigned char c;

    if (pos == len)
        return (-1);

    c = s[pos];

    if (g_ascii_isdigit (c))
        return 0;
    if (g_ascii_isalpha (c))
        return c;
    if (c == '~')
        return (-2);

    g_assert (UCHAR_MAX <= (INT_MAX - 1 - 2) / 2);

    return (int) c + UCHAR_MAX + 1;
}

/* --------------------------------------------------------------------------------------------- */

/* Slightly modified verrevcmp function from dpkg
 *
 * This implements the algorithm for comparison of version strings
 * specified by Debian and now widely adopted.  The detailed
 * specification can be found in the Debian Policy Manual in the
 * section on the 'Version' control field.  This version of the code
 * implements that from s5.6.12 of Debian Policy v3.8.0.1
 * https://www.debian.org/doc/debian-policy/ch-controlfields.html#s-f-Version
 *
 * @param s1 first char array to compare
 * @param s1_len length of @s1
 * @param s2 second char array to compare
 * @param s2_len length of @s2
 *
 * @return an integer less than, equal to, or greater than zero, if @s1 is <, == or > than @s2.
 */
static int
verrevcmp (const char *s1, ssize_t s1_len, const char *s2, ssize_t s2_len)
{
    ssize_t s1_pos = 0;
    ssize_t s2_pos = 0;

    while (s1_pos < s1_len || s2_pos < s2_len)
    {
        int first_diff = 0;

        while ((s1_pos < s1_len && !g_ascii_isdigit (s1[s1_pos]))
               || (s2_pos < s2_len && !g_ascii_isdigit (s2[s2_pos])))
        {
            int s1_c, s2_c;

            s1_c = order (s1, s1_pos, s1_len);
            s2_c = order (s2, s2_pos, s2_len);

            if (s1_c != s2_c)
                return (s1_c - s2_c);

            s1_pos++;
            s2_pos++;
        }

        while (s1_pos < s1_len && s1[s1_pos] == '0')
            s1_pos++;
        while (s2_pos < s2_len && s2[s2_pos] == '0')
            s2_pos++;

        while (s1_pos < s1_len && s2_pos < s2_len
               && g_ascii_isdigit (s1[s1_pos]) && g_ascii_isdigit (s2[s2_pos]))
        {
            if (first_diff == 0)
                first_diff = s1[s1_pos] - s2[s2_pos];

            s1_pos++;
            s2_pos++;
        }

        if (s1_pos < s1_len && g_ascii_isdigit (s1[s1_pos]))
            return 1;
        if (s2_pos < s2_len && g_ascii_isdigit (s2[s2_pos]))
            return (-1);
        if (first_diff != 0)
            return first_diff;
    }

    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* Compare version strings.
 *
 * @param s1 first string to compare
 * @param s2 second string to compare
 *
 * @return an integer less than, equal to, or greater than zero, if @s1 is <, == or > than @s2.
 */
int
filevercmp (const char *s1, const char *s2)
{
    return filenvercmp (s1, -1, s2, -1);
}

/* --------------------------------------------------------------------------------------------- */
/* Compare version strings.
 *
 * @param a first string to compare
 * @param alen length of @a or (-1)
 * @param b second string to compare
 * @param blen length of @b or (-1)
 *
 * @return an integer less than, equal to, or greater than zero, if @s1 is <, == or > than @s2.
 */
int
filenvercmp (const char *a, ssize_t alen, const char *b, ssize_t blen)
{
    gboolean aempty, bempty;
    ssize_t aprefixlen, bprefixlen;
    gboolean one_pass_only;
    int result;

    /* Special case for empty versions. */
    aempty = alen < 0 ? a[0] == '\0' : alen == 0;
    bempty = blen < 0 ? b[0] == '\0' : blen == 0;

    if (aempty)
        return (bempty ? 0 : -1);
    if (bempty)
        return 1;

    /* Special cases for leading ".": "." sorts first, then "..", then other names with leading ".",
       then other names. */
    if (a[0] == '.')
    {
        gboolean adot, bdot;
        gboolean adotdot, bdotdot;

        if (b[0] != '.')
            return (-1);

        adot = alen < 0 ? a[1] == '\0' : alen == 1;
        bdot = blen < 0 ? b[1] == '\0' : blen == 1;

        if (adot)
            return (bdot ? 0 : -1);
        if (bdot)
            return 1;

        adotdot = a[1] == '.' && (alen < 0 ? a[2] == '\0' : alen == 2);
        bdotdot = b[1] == '.' && (blen < 0 ? b[2] == '\0' : blen == 2);
        if (adotdot)
            return (bdotdot ? 0 : -1);
        if (bdotdot)
            return 1;
    }
    else if (b[0] == '.')
        return 1;

    /* Cut file suffixes. */
    aprefixlen = file_prefixlen (a, &alen);
    bprefixlen = file_prefixlen (b, &blen);

    /* If both suffixes are empty, a second pass would return the same thing. */
    one_pass_only = aprefixlen == alen && bprefixlen == blen;

    result = verrevcmp (a, aprefixlen, b, bprefixlen);

    /* Return the initial result if nonzero, or if no second pass is needed.
       Otherwise, restore the suffixes and try again. */
    return (result != 0 || one_pass_only ? result : verrevcmp (a, alen, b, blen));
}

/* --------------------------------------------------------------------------------------------- */
