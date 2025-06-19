/*
   Functions for escaping and unescaping strings

   Copyright (C) 2009-2025
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2009;
   Patrick Winnertz <winnie@debian.org>, 2009

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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <config.h>

#include "lib/global.h"
#include "lib/strutil.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static const char ESCAPE_SHELL_CHARS[] = " !#$%()&{}[]`?|<>;*\\\"'";
static const char ESCAPE_REGEX_CHARS[] = "^!#$%()&{}[]`?|<>;*+.\\";
static const char ESCAPE_GLOB_CHARS[] = "$*\\?";

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

char *
str_escape (const char *src, const ssize_t src_len, const char *escaped_chars,
            const gboolean escape_non_printable)
{
    GString *ret;

    // do NOT break allocation semantics
    if (src == NULL)
        return NULL;

    if (*src == '\0')
        return g_strdup ("");

    ret = g_string_new ("");

    const size_t src_len1 = src_len < 0 ? strlen (src) : (size_t) src_len;

    for (size_t curr_index = 0; curr_index < src_len1; curr_index++)
    {
        if (escape_non_printable)
        {
            switch (src[curr_index])
            {
            case '\n':
                g_string_append (ret, "\\n");
                continue;
            case '\t':
                g_string_append (ret, "\\t");
                continue;
            case '\b':
                g_string_append (ret, "\\b");
                continue;
            case '\0':
                g_string_append (ret, "\\0");
                continue;
            default:
                break;
            }
        }

        if (strchr (escaped_chars, (int) src[curr_index]))
            g_string_append_c (ret, '\\');

        g_string_append_c (ret, src[curr_index]);
    }
    return g_string_free (ret, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

char *
str_unescape (const char *src, const ssize_t src_len, const char *unescaped_chars,
              const gboolean unescape_non_printable)
{
    GString *ret;
    size_t curr_index;

    if (src == NULL)
        return NULL;

    if (*src == '\0')
        return g_strdup ("");

    ret = g_string_sized_new (16);

    const size_t src_len1 = src_len < 0 ? strlen (src) : (size_t) src_len;

    for (curr_index = 0; curr_index < src_len1 - 1; curr_index++)
    {
        if (src[curr_index] != '\\')
        {
            g_string_append_c (ret, src[curr_index]);
            continue;
        }

        curr_index++;

        if (unescaped_chars == ESCAPE_SHELL_CHARS && src[curr_index] == '$')
        {
            // special case: \$ is used to disallow variable substitution
            g_string_append_c (ret, '\\');
        }
        else
        {
            if (unescape_non_printable)
            {
                switch (src[curr_index])
                {
                case 'n':
                    g_string_append_c (ret, '\n');
                    continue;
                case 't':
                    g_string_append_c (ret, '\t');
                    continue;
                case 'b':
                    g_string_append_c (ret, '\b');
                    continue;
                case '0':
                    g_string_append_c (ret, '\0');
                    continue;
                default:
                    break;
                }
            }

            if (strchr (unescaped_chars, (int) src[curr_index]) == NULL)
                g_string_append_c (ret, '\\');
        }

        g_string_append_c (ret, src[curr_index]);
    }
    g_string_append_c (ret, src[curr_index]);

    return g_string_free (ret, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * To be compatible with the general posix command lines we have to escape
 * strings for the command line
 *
 * @param src string for escaping
 *
 * @return escaped string (which needs to be freed later) or NULL when NULL string is passed.
 */

char *
str_shell_escape (const char *src)
{
    return str_escape (src, -1, ESCAPE_SHELL_CHARS, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

char *
str_glob_escape (const char *src)
{
    return str_escape (src, -1, ESCAPE_GLOB_CHARS, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

char *
str_regex_escape (const char *src)
{
    return str_escape (src, -1, ESCAPE_REGEX_CHARS, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Unescape paths or other strings for e.g the internal cd
 * shell-unescape within a given buffer (writing to it!)
 *
 * @param text string for unescaping
 *
 * @return unescaped string (which needs to be freed)
 */

char *
str_shell_unescape (const char *text)
{
    return str_unescape (text, -1, ESCAPE_SHELL_CHARS, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

char *
str_glob_unescape (const char *text)
{
    return str_unescape (text, -1, ESCAPE_GLOB_CHARS, TRUE);
}

/* --------------------------------------------------------------------------------------------- */
char *
str_regex_unescape (const char *text)
{
    return str_unescape (text, -1, ESCAPE_REGEX_CHARS, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Check if char in pointer contain escape'd chars
 *
 * @param start string for checking
 * @param current pointer to checked character
 *
 * @return TRUE if string contain escaped chars otherwise return FALSE
 */

gboolean
str_is_char_escaped (const char *start, const char *current)
{
    int num_esc = 0;

    if (start == NULL || current == NULL || current <= start)
        return FALSE;

    current--;
    while (current >= start && *current == '\\')
    {
        num_esc++;
        current--;
    }
    return (gboolean) num_esc % 2;
}

/* --------------------------------------------------------------------------------------------- */
