/*
   Micro helper library - support recode for strings between charsets

   Copyright (C) 2009 The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2009.

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

#ifndef WITH_GLIB
#	include <stdlib.h>
#	include <string.h>
#endif

#include "src/global.h"
#include "mhl_recode.h"

/*** global variables **************************************************/
struct mhl_charset terminal_encoding;

/*** file scope macro definitions **************************************/

/*** file scope type declarations **************************************/

/*** file scope variables **********************************************/
static int is_terminal_in_utf8_mode;

/**
    names, that are used for utf-8 

    \private
*/
static const char *str_utf8_encodings[] = {
    "utf-8",
    "utf8",
    NULL
};

/**
    standard 8bit encodings, no wide or multibytes characters

    \private
*/
static const char *str_8bit_encodings[] = {
    "cp-1251",
    "cp1251",
    "cp-1250",
    "cp1250",
    "cp-866",
    "cp866",
    "cp-850",
    "cp850",
    "cp-852",
    "cp852",
    "iso-8859",
    "iso8859",
    "koi8",
    NULL
};

/*** file scope functions **********************************************/
/* ---------------------------------------------------------------------------- */
/**
    Search charset name in charsets list.

    \param const char *_charset_name
	name of charset
    
    \param const char **_charset_list
	point to list of charsets

    \return
	function return zero if _charset_name don't found in _charset_list.
	function return non-zero if _charset_name found in _charset_list.

    \private
*/
static int
mhl_recode_charset_test_class (const char *_charset_name, const char **_charset_list)
{
    int loop;
    int result = 0;

    for (loop = 0; _charset_list[loop] != NULL; loop++)
    {
	result += (strncasecmp (_charset_name, _charset_list[loop],
				  strlen (_charset_list[loop])) == 0);
    }

    return result;
}

/* ---------------------------------------------------------------------------- */
/**
    Determine recode type:
    \arg \c utf-8;
    \arg \c 8-bits one-byte;
    \arg \c 7-bits one-byte.

    \param const char *_charset_name
	name of charset

    \return
	function return type of charset (see declaration of enum mhl_charset_types
	in mhl/mhl_recode.h)

    \todo determine multibyte charsets;

    \private
*/

static int
mhl_recode_get_charset_type (const char *_charset_name)
{
    if (mhl_recode_charset_test_class (_charset_name, str_utf8_encodings))
	return MHL_CHARSET_UTF8;

    if (mhl_recode_charset_test_class (_charset_name, str_8bit_encodings))
	return MHL_CHARSET_8BIT;

    return MHL_CHARSET_7BIT;
}

/* ---------------------------------------------------------------------------- */
/*** public functions **************************************************/
/* ---------------------------------------------------------------------------- */
/**
    Init MHL recode subsystem.
    Determine system charset and initialize it.

    \param void
	function don't need any params.
    \return
	function don't return any data.

    \ingroup recode
*/
void
mhl_recode_init (void)
{
#ifdef WITH_GLIB
    is_terminal_in_utf8_mode = g_get_charset (&(terminal_encoding.name));
#else
    const char *local_lang_name = getenv ("LANG");
    is_terminal_in_utf8_mode = FALSE;

    if (local_lang_name)
    {
	terminal_encoding.name = strchr (local_lang_name, '.') + 1;

	if (!strncasecmp (terminal_encoding.name, "UTF-8", 5))
	    is_terminal_in_utf8_mode = TRUE;
    }
#endif
    mhl_recode_charset_init (&terminal_encoding);
}

/* ---------------------------------------------------------------------------- */
/**
    Init recode structure.

    \param struct mhl_charset *_charset
	point to structure mhl_charset

    \return
	function don't return any data.

    \ingroup recode
*/

void
mhl_recode_charset_init (struct mhl_charset *_charset)
{
    if (_charset == NULL || _charset->name == NULL)
	return;

    _charset->type = mhl_recode_get_charset_type (_charset->name);

    return;
}

/* ---------------------------------------------------------------------------- */
