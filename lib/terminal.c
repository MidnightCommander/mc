/*
   Terminal emulation.

   Copyright (C) 2025
   Free Software Foundation, Inc.

   This file is part of the Midnight Commander.
   Authors:
   Miguel de Icaza, 1994, 1995, 1996
   Johannes Altmanninger, 2025

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

/** \file terminal.c
 *  \brief Source: terminal emulation.
 *  \author Johannes Altmanninger
 *  \date 2025
 *
 *  Subshells running inside Midnight Commander may assume they run inside
 *  a terminal. This module helps us act like a real terminal in relevant
 *  aspects.
 */

#include <config.h>

#include "lib/util.h"
#include "lib/strutil.h"

#include "lib/terminal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Remove all control sequences from the argument string.  We define
 * "control sequence", in a sort of pidgin BNF, as follows:
 *
 * control-seq = Esc non-'['
 *             | Esc '[' (parameter-byte)* (intermediate-byte)* final-byte
 * parameter-byte = [\x30-\x3F]     # one of "0-9;:<=>?"
 * intermediate-byte = [\x20–\x2F]  # one of " !\"#$%&'()*+,-./"
 * final-byte = [\x40-\x7e]         # one of "@A–Z[\]^_`a–z{|}~"
 *
 * The 256-color and true-color escape sequences should allow either ';' or ':' inside as separator,
 * actually, ':' is the more correct according to ECMA-48.
 * Some terminal emulators (e.g. xterm, gnome-terminal) support this.
 *
 * Non-printable characters are also removed.
 */

char *
strip_ctrl_codes (char *s)
{
    char *w;        // Current position where the stripped data is written
    const char *r;  // Current position where the original data is read

    if (s == NULL)
        return NULL;

    const char *end = s + strlen (s);

    for (w = s, r = s; *r != '\0';)
    {
        if (*r == ESC_CHAR)
        {
            // Skip the control sequence's arguments
            // '(' need to avoid strange 'B' letter in *Suse (if mc runs under root user)
            if (*(++r) == '[' || *r == '(')
            {
                // strchr() matches trailing binary 0
                while (*(++r) != '\0' && strchr ("0123456789;:<=>?", *r) != NULL)
                    ;
                while (*r != '\0' && (*r < 0x40 || *r > 0x7E))
                    ++r;
            }
            if (*r == ']')
            {
                /*
                 * Skip xterm's OSC (Operating System Command)
                 * https://www.xfree86.org/current/ctlseqs.html
                 * OSC P s ; P t ST
                 * OSC P s ; P t BEL
                 */
                const char *new_r;

                for (new_r = r; *new_r != '\0'; new_r++)
                {
                    switch (*new_r)
                    {
                        // BEL
                    case '\a':
                        r = new_r;
                        goto osc_out;
                    case ESC_CHAR:
                        // ST
                        if (new_r[1] == '\\')
                        {
                            r = new_r + 1;
                            goto osc_out;
                        }
                        break;
                    default:
                        break;
                    }
                }
            osc_out:;
            }

            /*
             * Now we are at the last character of the sequence.
             * Skip it unless it's binary 0.
             */
            if (*r != '\0')
                r++;
        }
        else
        {
            const char *n;

            n = str_cget_next_char (r);
            if (str_isprint (r))
            {
                memmove (w, r, n - r);
                w += n - r;
            }
            r = n;
        }
    }

    *w = '\0';
    return s;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Convert "\E" -> esc character and ^x to control-x key and ^^ to ^ key
 *
 * @param p pointer to string
 *
 * @return newly allocated string
 */

char *
convert_controls (const char *p)
{
    char *valcopy;
    char *q;

    valcopy = g_strdup (p);

    // Parse the escape special character
    for (q = valcopy; *p != '\0';)
        switch (*p)
        {
        case '\\':
            p++;

            if (*p == 'e' || *p == 'E')
            {
                p++;
                *q++ = ESC_CHAR;
            }
            break;

        case '^':
            p++;
            if (*p == '^')
                *q++ = *p++;
            else
            {
                char c;

                c = *p | 0x20;
                if (c >= 'a' && c <= 'z')
                {
                    *q++ = c - 'a' + 1;
                    p++;
                }
                else if (*p != '\0')
                    p++;
            }
            break;

        default:
            *q++ = *p++;
        }

    *q = '\0';
    return valcopy;
}

/* --------------------------------------------------------------------------------------------- */
