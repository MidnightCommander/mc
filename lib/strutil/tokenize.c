/*
   Parse string into tokens.

   Copyright (C) 2024
   Free Software Foundation, Inc.

   Written by:
   Andrew Borodin <aborodin@vmail.ru> 2010-2024

   The str_tokenize() and str_tokenize_word routines are mostly from
   GNU readline-8.2.

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

/** \file tokenize.c
 *  \brief Source: parse string into tokens.
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "lib/global.h"
#include "lib/util.h"           /* whiteness() */

#include "lib/strutil.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define WORD_DELIMITERS " \t\n;&()|<>"
#define QUOTE_CHARACTERS "\"'`"

#define slashify_in_quotes "\\`\"$"

#define member(c, s) ((c != '\0') ? (strchr ((s), (c)) != NULL) : FALSE)

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/*
 * Based on history_tokenize_word() from GNU readline-8.2
 */
static int
str_tokenize_word (const char *string, int start)
{
    int i = start;
    char delimiter = '\0';
    char delimopen = '\0';
    int nestdelim = 0;

    if (member (string[i], "()\n"))     /* XXX - included \n, but why? been here forever */
        return (i + 1);

    if (g_ascii_isdigit (string[i]))
    {
        int j;

        for (j = i; string[j] != '\0' && g_ascii_isdigit (string[j]); j++)
            ;

        if (string[j] == '\0')
            return j;

        if (string[j] == '<' || string[j] == '>')
            i = j;              /* digit sequence is a file descriptor */
        else
        {
            i = j;              /* digit sequence is part of a word */
            goto get_word;
        }
    }

    if (member (string[i], "<>;&|"))
    {
        char peek = string[i + 1];

        if (peek == string[i])
        {
            if (peek == '<' && (string[i + 2] == '-' || string[i + 2] == '<'))
                i++;
            return (i + 2);
        }

        if (peek == '&' && (string[i] == '>' || string[i] == '<'))
        {
            int j;

            /* file descriptor */
            for (j = i + 2; string[j] != '\0' && g_ascii_isdigit (string[j]); j++)
                ;
            if (string[j] == '-')       /* <&[digits]-, >&[digits]- */
                j++;
            return j;
        }

        if ((peek == '>' && string[i] == '&') || (peek == '|' && string[i] == '>'))
            return (i + 2);

        /* XXX - process substitution -- separated out for later -- bash-4.2 */
        if (peek == '(' && (string[i] == '>' || string[i] == '<'))
        {
            /* ) */
            i += 2;
            delimopen = '(';
            delimiter = ')';
            nestdelim = 1;
            goto get_word;
        }

        return (i + 1);
    }

  get_word:
    /* Get word from string + i; */

    if (delimiter == '\0' && member (string[i], QUOTE_CHARACTERS))
    {
        delimiter = string[i];
        i++;
    }

    for (; string[i] != '\0'; i++)
    {
        if (string[i] == '\\' && string[i + 1] == '\n')
        {
            i++;
            continue;
        }

        if (string[i] == '\\' && delimiter != '\'' &&
            (delimiter != '"' || member (string[i], slashify_in_quotes)))
        {
            i++;
            continue;
        }

        /* delimiter must be set and set to something other than a quote if
           nestdelim is set, so these tests are safe. */
        if (nestdelim != 0 && string[i] == delimopen)
        {
            nestdelim++;
            continue;
        }
        if (nestdelim != 0 && string[i] == delimiter)
        {
            nestdelim--;
            if (nestdelim == 0)
                delimiter = '\0';
            continue;
        }

        if (delimiter != '\0' && string[i] == delimiter)
        {
            delimiter = '\0';
            continue;
        }

        /* Command and process substitution; shell extended globbing patterns */
        if (nestdelim == 0 && delimiter == '\0' && member (string[i], "<>$!@?+*")
            && string[i + 1] == '(')
        {
            /* ) */
            i += 2;
            delimopen = '(';
            delimiter = ')';
            nestdelim = 1;
            continue;
        }

        if (delimiter == '\0' && member (string[i], WORD_DELIMITERS))
            break;

        if (delimiter == '\0' && member (string[i], QUOTE_CHARACTERS))
            delimiter = string[i];
    }

    return i;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* Parse string into tokens.
 *
 * Based on history_tokenize_internal() from GNU readline-8.2
 */
GPtrArray *
str_tokenize (const char *string)
{
    GPtrArray *result = NULL;
    int i = 0;

    /* Get a token, and stuff it into RESULT.  The tokens are split
       exactly where the shell would split them. */
    while (string[i] != '\0')
    {
        int start;

        /* Skip leading whitespace */
        for (; string[i] != '\0' && whiteness (string[i]); i++)
            ;

        if (string[i] == '\0')
            return result;

        start = i;
        i = str_tokenize_word (string, start);

        /* If we have a non-whitespace delimiter character (which would not be
           skipped by the loop above), use it and any adjacent delimiters to
           make a separate field.  Any adjacent white space will be skipped the
           next time through the loop. */
        if (i == start)
            for (i++; string[i] != '\0' && member (string[i], WORD_DELIMITERS); i++)
                ;

        if (result == NULL)
            result = g_ptr_array_new ();

        g_ptr_array_add (result, g_strndup (string + start, i - start));
    }

    return result;
}

/* --------------------------------------------------------------------------------------------- */
