/*
   Functions for escaping and unescaping strings

   Copyright (C) 2009 The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2009;
   Patrick Winnertz <winnie@debian.org>, 2009

   This file is part of the Midnight Commander.

   The Midnight Commander is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA.
 */

#include <config.h>
#include "../src/strescape.h"


/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define shell_escape_toesc(x)	\
    (((x)==' ')||((x)=='!')||((x)=='#')||((x)=='$')||((x)=='%')||	\
     ((x)=='(')||((x)==')')||((x)=='\'')||((x)=='&')||((x)=='~')||	\
     ((x)=='{')||((x)=='}')||((x)=='[')||((x)==']')||((x)=='`')||	\
     ((x)=='?')||((x)=='|')||((x)=='<')||((x)=='>')||((x)==';')||	\
     ((x)=='*')||((x)=='\\')||((x)=='"'))

#define shell_escape_nottoesc(x)	\
    (((x)!=0) && (!shell_escape_toesc((x))))

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

/*** public functions ****************************************************************************/

/** To be compatible with the general posix command lines we have to escape
 strings for the command line

 \params in
 string for escaping

 \returns
 return escaped string (which needs to be freed later)
        or NULL when NULL string is passed.
 */
char*
shell_escape(const char* src)
{
	GString *str;
	char *result = NULL;

	/* do NOT break allocation semantics */
	if (!src)
		return NULL;

	if (*src == '\0')
		return strdup("");

	str = g_string_new("");
	
	/* look for the first char to escape */
	while (1)
	{
		char c;
		/* copy over all chars not to escape */
		while ((c=(*src)) && shell_escape_nottoesc(c))
		{
			g_string_append_c(str,c);
			src++;
		}

		/* at this point we either have an \0 or an char to escape */
		if (!c) {
			result = str->str;
			g_string_free(str,FALSE);
			return result;
		}

		g_string_append_c(str,'\\');
		g_string_append_c(str,c);
		src++;
	}
}
/* --------------------------------------------------------------------------------------------- */

/** Unescape paths or other strings for e.g the internal cd
    shell-unescape within a given buffer (writing to it!)

 \params src
 string for unescaping

 \returns
 return unescaped string (which needs to be freed)
 */
char*
shell_unescape(const char* text)
{
	GString *str;
	char *result = NULL;
	const char* readptr;
	char c;
	
	if (!text)
		return NULL;


	/* look for the first \ - that's quick skipover if there's nothing to escape */
	readptr = text;
	while ((*readptr) && ((*readptr)!='\\'))	readptr++;
	if (!(*readptr)) {
		result = g_strdup(text);
		return result;
	}
	str = g_string_new_len(text, readptr - text);

	/* if we're here, we're standing on the first '\' */
	while ((c = *readptr))
	{
		if (c=='\\')
		{
			readptr++;
			switch ((c = *readptr))
			{
				case '\0': /* end of string! malformed escape string */
					goto out;

				case 'n':	g_string_append_c(str,'\n');	break;
				case 'r':	g_string_append_c(str,'\r');	break;
				case 't':	g_string_append_c(str,'\t');	break;

				case ' ':
				case '\\':
				case '#':
				case '$':
				case '%':
				case '(':
				case ')':
				case '[':
				case ']':
				case '{':
				case '}':
				case '<':
				case '>':
				case '!':
				case '*':
				case '?':
				case '~':
				case '`':
				case '"':
				case ';':
				default:
					g_string_append_c(str,c); break;
			}
		}
		else	/* got a normal character */
		{
			g_string_append_c(str,c);
		}
		readptr++;
	}
out:

	result = str->str;
	g_string_free(str,FALSE);
    return result;
}
/* --------------------------------------------------------------------------------------------- */

/** Check if char in pointer contain escape'd chars

 \params in
 string for checking

 \returns
 return TRUE if string contain escaped chars
 otherwise return FALSE
 */
gboolean
strutils_is_char_escaped ( const char *start, const char *current )
{
    int num_esc = 0;

    if (start == NULL || current == NULL || current <= start)
        return FALSE;

    current--;
    while (current >= start && *current == '\\' ) {
        num_esc++;
        current--;
    }
    return (gboolean) num_esc % 2;
}


/* --------------------------------------------------------------------------------------------- */
