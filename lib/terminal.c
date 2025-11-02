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
 * Parse a CSI command, starting from the third byte (i.e. the first
 * parameter byte, if any).
 *
 * On success, *sptr will point to one-past the end of the sequence.
 * On failure, *sptr will point to the first invalid byte.
 *
 * Here's the format in a sort of pidgin BNF:
 *
 * CSI-command = Esc '[' parameters (intermediate-byte)* final-byte
 * parameters = [0-9;:]+
 *            | [<=>?] (parameter-byte)* # private mode
 * parameter-byte = [\x30-\x3F]     # one of "0-9;:<=>?"
 * intermediate-byte = [\x20–\x2F]  # one of " !\"#$%&'()*+,-./"
 * final-byte = [\x40-\x7e]         # one of "@A–Z[\]^_`a–z{|}~"
 */
gboolean
parse_csi (csi_command_t *out, const char **sptr, const char *end)
{
    gboolean ok = FALSE;
    char c;
    char private_mode = '\0';
    // parameter bytes
    size_t param_count = 0;

    const char *s = *sptr;
    if (s == end)
        goto invalid_sequence;

    c = *s;

#define NEXT_CHAR                                                                                  \
    do                                                                                             \
    {                                                                                              \
        if (++s == end)                                                                            \
            goto invalid_sequence;                                                                 \
        c = *s;                                                                                    \
    }                                                                                              \
    while (FALSE)

    if (c >= '<' && c <= '?')  // "<=>?"
    {
        private_mode = c;
        NEXT_CHAR;
    }

    if (private_mode != '\0')
    {
        while (c >= 0x30 && c <= 0x3F)
            NEXT_CHAR;
    }
    else
    {
        uint32_t tmp = 0;
        size_t sub_index = 0;

        if (out != NULL)
            // N.B. empty parameter strings are allowed. For our current use,
            // treating them as zeroes happens to work.
            memset (out->params, 0, sizeof (out->params));

        while (c >= 0x30 && c <= 0x3F)
        {
            if (c >= '0' && c <= '9')
            {
                if (param_count == 0)
                    param_count = 1;
                if (tmp * 10 < tmp)
                    goto invalid_sequence;  // overflow
                tmp *= 10;
                if (tmp + c - '0' < tmp)
                    goto invalid_sequence;  // overflow
                tmp += c - '0';
                if (out != NULL)
                    out->params[param_count - 1][sub_index] = tmp;
            }
            else if (c == ':' && ++sub_index < G_N_ELEMENTS (out->params[0]))
                tmp = 0;
            else if (c == ';' && ++param_count <= G_N_ELEMENTS (out->params))
                tmp = 0, sub_index = 0;
            else
                goto invalid_sequence;
            NEXT_CHAR;
        }
    }

    while (c >= 0x20 && c <= 0x2F)  // intermediate bytes
        NEXT_CHAR;
#undef NEXT_CHAR

    if (c < 0x40 || c > 0x7E)  // final byte
        goto invalid_sequence;

    ++s;
    ok = TRUE;

    if (out != NULL)
    {
        out->private_mode = private_mode;
        out->param_count = param_count;
    }

invalid_sequence:
    *sptr = s;
    return ok;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Remove all control sequences (CSI, OSC) from the argument string.
 *
 * The 256-color and true-color escape sequences should allow either ';' or ':' inside as
 * separator, actually, ':' is the more correct according to ECMA-48. Some terminal emulators
 * (e.g. xterm, gnome-terminal) support this.
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
                ++r;
                parse_csi (NULL, &r, end);
                // We're already past the sequence, no need to increment.
                continue;
            }
            if (*r == ']')
            {
                /*
                 * Skip xterm's OSC (Operating System Command)
                 * https://www.xfree86.org/current/ctlseqs.html
                 * OSC P s ; P t ST
                 * OSC P s ; P t BEL
                 */
                for (const char *new_r = r; *new_r != '\0'; new_r++)
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
            // Copy byte by byte, thereby letting mc use its standard mechanism to denote invalid
            // UTF-8 sequences. See #4801.
            *(w++) = *(r++);
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
