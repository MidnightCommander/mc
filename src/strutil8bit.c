/* 8bit strings utilities
   Copyright (C) 2007 Free Software Foundation, Inc.

   Written 2007 by:
   Rostislav Benes 

   The file_date routine is mostly from GNU's fileutils package,
   written by Richard Stallman and David MacKenzie.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <iconv.h>

#include "global.h"
#include "strutil.h"

/* functions for singlebyte encodings, all characters have width 1
 * using standard system functions
 * there are only small differences between functions in strutil8bit.c 
 * and strutilascii.c
 */

static const char replch = '?';

static void
str_8bit_insert_replace_char (GString * buffer)
{
    g_string_append_c (buffer, replch);
}

static int
str_8bit_is_valid_string (const char *text)
{
    (void) text;
    return 1;
}

static int
str_8bit_is_valid_char (const char *ch, size_t size)
{
    (void) ch;
    (void) size;
    return 1;
}

static void
str_8bit_cnext_char (const char **text)
{
    (*text)++;
}

static void
str_8bit_cprev_char (const char **text)
{
    (*text)--;
}

static int
str_8bit_cnext_noncomb_char (const char **text)
{
    if (*text[0] != '\0')
    {
	(*text)++;
	return 1;
    }
    else
	return 0;
}

static int
str_8bit_cprev_noncomb_char (const char **text, const char *begin)
{
    if ((*text) != begin)
    {
	(*text)--;
	return 1;
    }
    else
	return 0;
}

static int
str_8bit_isspace (const char *text)
{
    return isspace (text[0]);
}

static int
str_8bit_ispunct (const char *text)
{
    return ispunct (text[0]);
}

static int
str_8bit_isalnum (const char *text)
{
    return isalnum (text[0]);
}

static int
str_8bit_isdigit (const char *text)
{
    return isdigit (text[0]);
}

static int
str_8bit_isprint (const char *text)
{
    return isprint (text[0]);
}

static int
str_8bit_iscombiningmark (const char *text)
{
    (void) text;
    return 0;
}

static int
str_8bit_toupper (const char *text, char **out, size_t * remain)
{
    if (*remain <= 1)
	return 0;
    (*out)[0] = toupper ((unsigned char) text[0]);
    (*out)++;
    (*remain)--;
    return 1;
}

static int
str_8bit_tolower (const char *text, char **out, size_t * remain)
{
    if (*remain <= 1)
	return 0;
    (*out)[0] = tolower ((unsigned char) text[0]);
    (*out)++;
    (*remain)--;
    return 1;
}

static int
str_8bit_length (const char *text)
{
    return strlen (text);
}

static int
str_8bit_length2 (const char *text, int size)
{
    return (size >= 0) ? min (strlen (text), (gsize)size) : strlen (text);
}

static estr_t
str_8bit_vfs_convert_to (GIConv coder, const char *string,
			 int size, GString * buffer)
{
    estr_t result;

    if (coder == str_cnv_not_convert)
    {
	g_string_append_len (buffer, string, size);
	result = ESTR_SUCCESS;
    }
    else
	result = str_nconvert (coder, (char *) string, size, buffer);

    return result;
}


static const char *
str_8bit_term_form (const char *text)
{
    static char result[BUF_MEDIUM];
    char *actual;
    size_t remain;
    size_t length;
    size_t pos = 0;

    actual = result;
    remain = sizeof (result);
    length = strlen (text);

    for (; pos < length && remain > 1; pos++, actual++, remain--)
    {
	actual[0] = isprint (text[pos]) ? text[pos] : '.';
    }

    actual[0] = '\0';
    return result;
}

static const char *
str_8bit_fit_to_term (const char *text, int width, int just_mode)
{
    static char result[BUF_MEDIUM];
    char *actual;
    size_t remain;
    int ident;
    size_t length;
    size_t pos = 0;

    length = strlen (text);
    actual = result;
    remain = sizeof (result);

    if ((int)length <= width)
    {
	ident = 0;
	switch (HIDE_FIT (just_mode))
	{
	case J_CENTER_LEFT:
	case J_CENTER:
	    ident = (width - length) / 2;
	    break;
	case J_RIGHT:
	    ident = width - length;
	    break;
	}

	if ((int)remain <= ident)
	    goto finally;
	memset (actual, ' ', ident);
	actual += ident;
	remain -= ident;

	for (; pos < length && remain > 1; pos++, actual++, remain--)
	{
	    actual[0] = isprint (text[pos]) ? text[pos] : '.';
	}
	if (width - length - ident > 0)
	{
	    if (remain <= width - length - ident)
		goto finally;
	    memset (actual, ' ', width - length - ident);
	    actual += width - length - ident;
	    remain -= width - length - ident;
	}
    }
    else
    {
	if (IS_FIT (just_mode))
	{
	    for (; pos + 1 <= (gsize)width / 2 && remain > 1;
		 actual++, pos++, remain--)
	    {

		actual[0] = isprint (text[pos]) ? text[pos] : '.';
	    }

	    if (remain <= 1)
		goto finally;
	    actual[0] = '~';
	    actual++;
	    remain--;

	    pos += length - width + 1;

	    for (; pos < length && remain > 1; pos++, actual++, remain--)
	    {
		actual[0] = isprint (text[pos]) ? text[pos] : '.';
	    }
	}
	else
	{
	    ident = 0;
	    switch (HIDE_FIT (just_mode))
	    {
	    case J_CENTER:
		ident = (length - width) / 2;
		break;
	    case J_RIGHT:
		ident = length - width;
		break;
	    }

	    pos += ident;
	    for (; pos < (gsize)(ident + width) && remain > 1;
		 pos++, actual++, remain--)
	    {

		actual[0] = isprint (text[pos]) ? text[pos] : '.';
	    }

	}
    }
  finally:
    actual[0] = '\0';
    return result;
}

static const char *
str_8bit_term_trim (const char *text, int width)
{
    static char result[BUF_MEDIUM];
    size_t remain;
    char *actual;
    size_t pos = 0;
    size_t length;

    length = strlen (text);
    actual = result;
    remain = sizeof (result);

    if (width < (int)length)
    {
	if (width <= 3)
	{
	    memset (actual, '.', width);
	    actual += width;
	    remain -= width;
	}
	else
	{
	    memset (actual, '.', 3);
	    actual += 3;
	    remain -= 3;

	    pos += length - width + 3;

	    for (; pos < length && remain > 1; pos++, actual++, remain--)
	    {
		actual[0] = isprint (text[pos]) ? text[pos] : '.';
	    }
	}
    }
    else
    {
	for (; pos < length && remain > 1; pos++, actual++, remain--)
	{
	    actual[0] = isprint (text[pos]) ? text[pos] : '.';
	}
    }

    actual[0] = '\0';
    return result;
}

static int
str_8bit_term_width2 (const char *text, size_t length)
{
    return (length != (size_t) (-1))
	? min (strlen (text), length) : strlen (text);
}

static int
str_8bit_term_width1 (const char *text)
{
    return str_8bit_term_width2 (text, (size_t) (-1));
}

static int
str_8bit_term_char_width (const char *text)
{
    (void) text;
    return 1;
}

static void
str_8bit_msg_term_size (const char *text, int *lines, int *columns)
{

    char *p, *tmp;
    char *q;
    char c = '\0';
    int width;
    p = tmp;

    (*lines) = 1;
    (*columns) = 0;
    tmp = g_strdup ((char *)text);
    for (;;)
    {
	q = strchr (p, '\n');
	if (q != NULL)
	{
	    c = q[0];
	    q[0] = '\0';
	}

	width = str_8bit_term_width1 (p);
	if (width > (*columns))
	    (*columns) = width;

	if (q == NULL)
	    break;
	q[0] = c;
	p = q + 1;
	(*lines)++;
    }
    g_free (tmp);
}

static const char *
str_8bit_term_substring (const char *text, int start, int width)
{
    static char result[BUF_MEDIUM];
    size_t remain;
    char *actual;
    size_t pos = 0;
    size_t length;

    actual = result;
    remain = sizeof (result);
    length = strlen (text);

    if (start < (int)length)
    {
	pos += start;
	for (; pos < length && width > 0 && remain > 1;
	     pos++, width--, actual++, remain--)
	{

	    actual[0] = isprint (text[pos]) ? text[pos] : '.';
	}
    }

    for (; width > 0 && remain > 1; actual++, remain--, width--)
    {
	actual[0] = ' ';
    }

    actual[0] = '\0';
    return result;
}

static const char *
str_8bit_trunc (const char *text, int width)
{
    static char result[MC_MAXPATHLEN];
    int remain;
    char *actual;
    size_t pos = 0;
    size_t length;

    actual = result;
    remain = sizeof (result);
    length = strlen (text);

    if ((int)length > width)
    {
	for (; pos + 1 <= (gsize)width / 2 && remain > 1; actual++, pos++, remain--)
	{
	    actual[0] = isprint (text[pos]) ? text[pos] : '.';
	}

	if (remain <= 1)
	    goto finally;
	actual[0] = '~';
	actual++;
	remain--;

	pos += length - width + 1;

	for (; pos < length && remain > 1; pos++, actual++, remain--)
	{
	    actual[0] = isprint (text[pos]) ? text[pos] : '.';
	}
    }
    else
    {
	for (; pos < length && remain > 1; pos++, actual++, remain--)
	{
	    actual[0] = isprint (text[pos]) ? text[pos] : '.';
	}
    }

  finally:
    actual[0] = '\0';
    return result;
}

static int
str_8bit_offset_to_pos (const char *text, size_t length)
{
    (void) text;
    return (int) length;
}

static int
str_8bit_column_to_pos (const char *text, size_t pos)
{
    (void) text;
    return (int)pos;
}

static char *
str_8bit_create_search_needle (const char *needle, int case_sen)
{
    (void) case_sen;
    return (char *) needle;
}

static void
str_8bit_release_search_needle (char *needle, int case_sen)
{
    (void) case_sen;
    (void) needle;
}

static const char *
str_8bit_search_first (const char *text, const char *search, int case_sen)
{
    char *fold_text;
    char *fold_search;
    const char *match;
    size_t offsset;

    fold_text = (case_sen) ? (char *) text : g_strdown (g_strdup (text));
    fold_search = (case_sen) ? (char *) search : g_strdown (g_strdup (search));

    match = g_strstr_len (fold_text, -1, fold_search);
    if (match != NULL)
    {
	offsset = match - fold_text;
	match = text + offsset;
    }

    if (!case_sen)
    {
	g_free (fold_text);
	g_free (fold_search);
    }

    return match;
}

static const char *
str_8bit_search_last (const char *text, const char *search, int case_sen)
{
    char *fold_text;
    char *fold_search;
    const char *match;
    size_t offsset;

    fold_text = (case_sen) ? (char *) text : g_strdown (g_strdup (text));
    fold_search = (case_sen) ? (char *) search : g_strdown (g_strdup (search));

    match = g_strrstr_len (fold_text, -1, fold_search);
    if (match != NULL)
    {
	offsset = match - fold_text;
	match = text + offsset;
    }

    if (!case_sen)
    {
	g_free (fold_text);
	g_free (fold_search);
    }

    return match;
}

static int
str_8bit_compare (const char *t1, const char *t2)
{
    return strcmp (t1, t2);
}

static int
str_8bit_ncompare (const char *t1, const char *t2)
{
    return strncmp (t1, t2, min (strlen (t1), strlen (t2)));
}

static int
str_8bit_casecmp (const char *t1, const char *t2)
{
    return g_strcasecmp (t1, t2);
}

static int
str_8bit_ncasecmp (const char *t1, const char *t2)
{
    return g_strncasecmp (t1, t2, min (strlen (t1), strlen (t2)));
}

static int
str_8bit_prefix (const char *text, const char *prefix)
{
    int result;
    for (result = 0; text[result] != '\0' && prefix[result] != '\0'
	 && text[result] == prefix[result]; result++);
    return result;
}

static int
str_8bit_caseprefix (const char *text, const char *prefix)
{
    int result;
    for (result = 0; text[result] != '\0' && prefix[result] != '\0'
	 && toupper (text[result]) == toupper (prefix[result]); result++);
    return result;
}



static void
str_8bit_fix_string (char *text)
{
    (void) text;
}

static char *
str_8bit_create_key (const char *text, int case_sen)
{
    return (case_sen) ? (char *) text : g_strdown (g_strdup (text));
}

static int
str_8bit_key_collate (const char *t1, const char *t2, int case_sen)
{
    if (case_sen)
	return strcmp (t1, t2);
    else
	return strcoll (t1, t2);
}

static void
str_8bit_release_key (char *key, int case_sen)
{
    if (!case_sen)
	g_free (key);
}

struct str_class
str_8bit_init ()
{
    struct str_class result;

    result.vfs_convert_to = str_8bit_vfs_convert_to;
    result.insert_replace_char = str_8bit_insert_replace_char;
    result.is_valid_string = str_8bit_is_valid_string;
    result.is_valid_char = str_8bit_is_valid_char;
    result.cnext_char = str_8bit_cnext_char;
    result.cprev_char = str_8bit_cprev_char;
    result.cnext_char_safe = str_8bit_cnext_char;
    result.cprev_char_safe = str_8bit_cprev_char;
    result.cnext_noncomb_char = str_8bit_cnext_noncomb_char;
    result.cprev_noncomb_char = str_8bit_cprev_noncomb_char;
    result.isspace = str_8bit_isspace;
    result.ispunct = str_8bit_ispunct;
    result.isalnum = str_8bit_isalnum;
    result.isdigit = str_8bit_isdigit;
    result.isprint = str_8bit_isprint;
    result.iscombiningmark = str_8bit_iscombiningmark;
    result.toupper = str_8bit_toupper;
    result.tolower = str_8bit_tolower;
    result.length = str_8bit_length;
    result.length2 = str_8bit_length2;
    result.length_noncomb = str_8bit_length;
    result.fix_string = str_8bit_fix_string;
    result.term_form = str_8bit_term_form;
    result.fit_to_term = str_8bit_fit_to_term;
    result.term_trim = str_8bit_term_trim;
    result.term_width2 = str_8bit_term_width2;
    result.term_width1 = str_8bit_term_width1;
    result.term_char_width = str_8bit_term_char_width;
    result.msg_term_size = str_8bit_msg_term_size;
    result.term_substring = str_8bit_term_substring;
    result.trunc = str_8bit_trunc;
    result.offset_to_pos = str_8bit_offset_to_pos;
    result.column_to_pos = str_8bit_column_to_pos;
    result.create_search_needle = str_8bit_create_search_needle;
    result.release_search_needle = str_8bit_release_search_needle;
    result.search_first = str_8bit_search_first;
    result.search_last = str_8bit_search_last;
    result.compare = str_8bit_compare;
    result.ncompare = str_8bit_ncompare;
    result.casecmp = str_8bit_casecmp;
    result.ncasecmp = str_8bit_ncasecmp;
    result.prefix = str_8bit_prefix;
    result.caseprefix = str_8bit_caseprefix;
    result.create_key = str_8bit_create_key;
    result.create_key_for_filename = str_8bit_create_key;
    result.key_collate = str_8bit_key_collate;
    result.release_key = str_8bit_release_key;

    return result;
}
