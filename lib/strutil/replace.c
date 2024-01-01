/*
   Functions for replacing substrings in strings.

   Copyright (C) 2013-2024
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2013;

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

#include <config.h>

#include "lib/global.h"
#include "lib/strescape.h"
#include "lib/strutil.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Replace all substrings 'needle' in string 'haystack' by 'replacement'.
 * If the 'needle' in the 'haystack' is escaped by backslash,
 * then this occurrence isn't be replaced.
 *
 * @param haystack    string contains substrings for replacement. Cannot be NULL.
 * @param needle      string for search. Cannot be NULL.
 * @param replacement string for replace. Cannot be NULL.
 * @return newly allocated string with replaced substrings or NULL if @haystack is empty.
 */

char *
str_replace_all (const char *haystack, const char *needle, const char *replacement)
{
    size_t needle_len, replacement_len;
    GString *return_str = NULL;
    char *needle_in_str;

    needle_len = strlen (needle);
    replacement_len = strlen (replacement);

    while ((needle_in_str = strstr (haystack, needle)) != NULL)
    {
        if (return_str == NULL)
            return_str = g_string_sized_new (32);

        if (strutils_is_char_escaped (haystack, needle_in_str))
        {
            char *backslash = needle_in_str - 1;

            if (haystack != backslash)
                g_string_append_len (return_str, haystack, backslash - haystack);
            g_string_append_len (return_str, needle_in_str, needle_in_str - backslash);
            haystack = needle_in_str + 1;
        }
        else
        {
            if (needle_in_str != haystack)
                g_string_append_len (return_str, haystack, needle_in_str - haystack);
            g_string_append_len (return_str, replacement, replacement_len);
            haystack = needle_in_str + needle_len;
        }
    }

    if (*haystack != '\0')
    {
        if (return_str == NULL)
            return strdup (haystack);

        g_string_append (return_str, haystack);
    }

    return (return_str != NULL ? g_string_free (return_str, FALSE) : NULL);
}

/* --------------------------------------------------------------------------------------------- */
