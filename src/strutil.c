/* common strings utilities
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
#include <stdlib.h>
#include <stdio.h>
#include <langinfo.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include "global.h"
#include "strutil.h"

/*names, that are used for utf-8 */
static const char *str_utf8_encodings[] = {
        "utf-8", 
        "utf8", 
        NULL
};

/* standard 8bit encodings, no wide or multibytes characters*/
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

/* terminal encoding*/
static char *codeset;
/* function for encoding specific operations*/
static struct str_class used_class;

GIConv str_cnv_to_term;
GIConv str_cnv_from_term;
GIConv str_cnv_not_convert;

/* if enc is same encoding like on terminal*/
static int
str_test_not_convert (const char *enc)
{
    return g_ascii_strcasecmp (enc, codeset) == 0;
}

GIConv
str_crt_conv_to (const char *to_enc)
{
    return (!str_test_not_convert (to_enc))
	? g_iconv_open (to_enc, codeset) : str_cnv_not_convert;
}

GIConv
str_crt_conv_from (const char *from_enc)
{
    return (!str_test_not_convert (from_enc))
	? g_iconv_open (codeset, from_enc) : str_cnv_not_convert;
}

void
str_close_conv (GIConv conv)
{
    if (conv != str_cnv_not_convert)
	g_iconv_close (conv);
}

static estr_t
_str_convert (GIConv coder, const char *string, int size, GString * buffer)
{
    estr_t state  = ESTR_SUCCESS;
    gchar *tmp_buff = NULL;
    gssize left;
    gsize bytes_read, bytes_written;
    GError *error = NULL;
    errno = 0;

    if (coder == INVALID_CONV)
	return ESTR_FAILURE;

    if (string == NULL || buffer == NULL)
	return ESTR_FAILURE;

/*
    if (! used_class.is_valid_string (string))
    {
	return ESTR_FAILURE;
    }
*/
    if (size < 0)
    {
	size = strlen (string);
    }
    else
    {
	left = strlen (string);
	if (left < size)
	    size = left;
    }

    left = size;
    g_iconv (coder, NULL, NULL, NULL, NULL);

    while (left)
    {
	tmp_buff = g_convert_with_iconv ((const gchar *) string,
					 left,
					 coder,
					 &bytes_read,
					 &bytes_written, &error);
	if (error)
	{
	    switch (error->code)
	    {
	    case G_CONVERT_ERROR_NO_CONVERSION:
		/* Conversion between the requested character sets is not supported. */
		tmp_buff = g_strnfill (strlen (string), '?');
		g_string_append (buffer, tmp_buff);
		g_free (tmp_buff);
		g_error_free (error);
		error = NULL;
		return ESTR_FAILURE;

	    case G_CONVERT_ERROR_ILLEGAL_SEQUENCE:
		/* Invalid byte sequence in conversion input. */
		if (tmp_buff){
		    g_string_append (buffer, tmp_buff);
		    g_free (tmp_buff);
		}
		if ((int)bytes_read < left)
		{
		    string += bytes_read + 1;
		    size -= (bytes_read + 1);
		    left -= (bytes_read + 1);
		    g_string_append_c (buffer, *(string-1));
		}
		else
		{
		    g_error_free (error);
		    error = NULL;
		    return ESTR_PROBLEM;
		}
		state = ESTR_PROBLEM;
		break;

	    case G_CONVERT_ERROR_PARTIAL_INPUT:
		/* Partial character sequence at end of input. */
		g_error_free (error);
		error = NULL;
		g_string_append (buffer, tmp_buff);
		g_free (tmp_buff);
		if ((int)bytes_read < left)
		{
		    left = left - bytes_read;
		    tmp_buff = g_strnfill (left, '?');
		    g_string_append (buffer, tmp_buff);
		    g_free (tmp_buff);
		}
		return ESTR_PROBLEM;

	    case G_CONVERT_ERROR_BAD_URI:	/* Don't know how handle this error :( */
	    case G_CONVERT_ERROR_NOT_ABSOLUTE_PATH:	/* Don't know how handle this error :( */
	    case G_CONVERT_ERROR_FAILED:	/* Conversion failed for some reason. */
	    default:
		g_error_free (error);
		error = NULL;
		if (tmp_buff){
		    g_free (tmp_buff);
		    tmp_buff = NULL;
		}
		return ESTR_FAILURE;
	    }
	    g_error_free (error);
	    error = NULL;
	}
	else
	{
	    if (tmp_buff != NULL)
	    {
		if (*tmp_buff)
		{
		    g_string_append (buffer, tmp_buff);
		    g_free (tmp_buff);
		    string += bytes_read;
		    left -= bytes_read;
		}
		else
		{
		    g_free (tmp_buff);
		    g_string_append (buffer, string);
		    return state;
		}
	    }
	    else
	    {
		g_string_append (buffer, string);
		return ESTR_PROBLEM;
	    }
	}
    }
    return state;
}

estr_t
str_convert (GIConv coder, const char *string, GString * buffer)
{
    return _str_convert (coder, string, -1, buffer);
}

estr_t
str_nconvert (GIConv coder, const char *string, int size, GString * buffer)
{
    return _str_convert (coder, string, size, buffer);
}

estr_t
str_vfs_convert_from (GIConv coder, const char *string, GString * buffer)
{
    estr_t result;

    if (coder == str_cnv_not_convert)
    {
	g_string_append (buffer, string != NULL ? string : "");
	result = ESTR_SUCCESS;
    }
    else
	result = _str_convert (coder, string, -1, buffer);

    return result;
}

estr_t
str_vfs_convert_to (GIConv coder, const char *string, int size,
		    GString * buffer)
{
    return used_class.vfs_convert_to (coder, string, size, buffer);
}

void
str_printf (GString * buffer, const char *format, ...)
{
    va_list ap;
    va_start (ap, format);
#if GLIB_CHECK_VERSION (2, 14, 0)
    g_string_append_vprintf (buffer, format, ap);
#else
    {
	gchar *tmp;
	tmp = g_strdup_vprintf (format, ap);
	g_string_append (buffer, tmp);
	g_free(tmp);
    }
#endif
    va_end (ap);
}

void
str_insert_replace_char (GString * buffer)
{
    used_class.insert_replace_char (buffer);
}

estr_t
str_translate_char (GIConv conv, const char *keys, size_t ch_size,
		    char *output, size_t out_size)
{
    size_t left;
    size_t cnv;

    g_iconv (conv, NULL, NULL, NULL, NULL);

    left = (ch_size == (size_t) (-1)) ? strlen (keys) : ch_size;

    cnv = g_iconv (conv, (gchar **) &keys, &left, &output, &out_size);
    if (cnv == (size_t)(-1)) {
        return (errno == EINVAL) ? ESTR_PROBLEM : ESTR_FAILURE;
    } else {
        output[0] = '\0';
        return ESTR_SUCCESS;
    }
}


static const char *
str_detect_termencoding ()
{
    return (nl_langinfo (CODESET));
}

static int
str_test_encoding_class (const char *encoding, const char **table)
{
    int t;
    int result = 0;
    if ( encoding == NULL )
        return result;

    for (t = 0; table[t] != NULL; t++)
    {
	result += (g_ascii_strncasecmp (encoding, table[t],
					strlen (table[t])) == 0);
    }
    return result;
}

static void
str_choose_str_functions ()
{
    if (str_test_encoding_class (codeset, str_utf8_encodings))
    {
	used_class = str_utf8_init ();
    }
    else if (str_test_encoding_class (codeset, str_8bit_encodings))
    {
	used_class = str_8bit_init ();
    }
    else
    {
	used_class = str_ascii_init ();
    }
}

int
str_isutf8 (const char *codeset_name)
{
    int result = 0;
    if (str_test_encoding_class (codeset_name, str_utf8_encodings))
    {
	result = 1;
    }
    return result;
}

void
str_init_strings (const char *termenc)
{
    codeset = g_strdup ((termenc != NULL) 
                        ? termenc 
                        : str_detect_termencoding ());

    str_cnv_not_convert = g_iconv_open (codeset, codeset);
    if (str_cnv_not_convert == INVALID_CONV)
    {
	if (termenc != NULL)
	{
	    g_free (codeset);
	    codeset = g_strdup (str_detect_termencoding ());
	    str_cnv_not_convert = g_iconv_open (codeset, codeset);
	}

	if (str_cnv_not_convert == INVALID_CONV)
	{
	    g_free (codeset);
	    codeset = g_strdup ("ascii");
	    str_cnv_not_convert = g_iconv_open (codeset, codeset);
	}
    }

    str_cnv_to_term = str_cnv_not_convert;
    str_cnv_from_term = str_cnv_not_convert;

    str_choose_str_functions ();
}

void
str_uninit_strings ()
{
    g_iconv_close (str_cnv_not_convert);
}

const char *
str_term_form (const char *text)
{
    return used_class.term_form (text);
}

const char *
str_fit_to_term (const char *text, int width, align_crt_t just_mode)
{
    return used_class.fit_to_term (text, width, just_mode);
}

const char *
str_term_trim (const char *text, int width)
{
    return used_class.term_trim (text, width);
}

void
str_msg_term_size (const char *text, int *lines, int *columns)
{
    used_class.msg_term_size (text, lines, columns);
}

const char *
str_term_substring (const char *text, int start, int width)
{
    return used_class.term_substring (text, start, width);
}

char *
str_get_next_char (char *text)
{

    used_class.cnext_char ((const char **) &text);
    return text;
}

const char *
str_cget_next_char (const char *text)
{
    used_class.cnext_char(&text);
    return text;
}

void
str_next_char (char **text)
{
    used_class.cnext_char ((const char **) text);
}

void
str_cnext_char (const char **text)
{
    used_class.cnext_char (text);
}

char *
str_get_prev_char (char *text)
{
    used_class.cprev_char ((const char **) &text);
    return text;
}

const char *
str_cget_prev_char (const char *text)
{
    used_class.cprev_char (&text);
    return text;
}

void
str_prev_char (char **text)
{
    used_class.cprev_char ((const char **) text);
}

void
str_cprev_char (const char **text)
{
    used_class.cprev_char (text);
}

char *
str_get_next_char_safe (char *text)
{
    used_class.cnext_char_safe ((const char **) &text);
    return text;
}

const char *
str_cget_next_char_safe (const char *text)
{
    used_class.cnext_char_safe (&text);
    return text;
}

void
str_next_char_safe (char **text)
{
    used_class.cnext_char_safe ((const char **) text);
}

void
str_cnext_char_safe (const char **text)
{
    used_class.cnext_char_safe (text);
}

char *
str_get_prev_char_safe (char *text)
{
    used_class.cprev_char_safe ((const char **) &text);
    return text;
}

const char *
str_cget_prev_char_safe (const char *text)
{
    used_class.cprev_char_safe (&text);
    return text;
}

void
str_prev_char_safe (char **text)
{
    used_class.cprev_char_safe ((const char **) text);
}

void
str_cprev_char_safe (const char **text)
{
    used_class.cprev_char_safe (text);
}

int
str_next_noncomb_char (char **text)
{
    return used_class.cnext_noncomb_char ((const char **) text);
}

int
str_cnext_noncomb_char (const char **text)
{
    return used_class.cnext_noncomb_char (text);
}

int
str_prev_noncomb_char (char **text, const char *begin)
{
    return used_class.cprev_noncomb_char ((const char **) text, begin);
}

int
str_cprev_noncomb_char (const char **text, const char *begin)
{
    return used_class.cprev_noncomb_char (text, begin);
}

int
str_is_valid_char (const char *ch, size_t size)
{
    return used_class.is_valid_char (ch, size);
}

int
str_term_width1 (const char *text)
{
    return used_class.term_width1 (text);
}

int
str_term_width2 (const char *text, size_t length)
{
    return used_class.term_width2 (text, length);
}

int
str_term_char_width (const char *text)
{
    return used_class.term_char_width (text);
}

int
str_offset_to_pos (const char *text, size_t length)
{
    return used_class.offset_to_pos (text, length);
}

int
str_length (const char *text)
{
    return used_class.length (text);
}

int
str_length_char (const char *text)
{
    return str_cget_next_char_safe (text)-text;
}

int
str_length2 (const char *text, int size)
{
    return used_class.length2 (text, size);
}

int
str_length_noncomb (const char *text)
{
    return used_class.length_noncomb (text);
}

int
str_column_to_pos (const char *text, size_t pos)
{
    return used_class.column_to_pos (text, pos);
}

int
str_isspace (const char *ch)
{
    return used_class.isspace (ch);
}

int
str_ispunct (const char *ch)
{
    return used_class.ispunct (ch);
}

int
str_isalnum (const char *ch)
{
    return used_class.isalnum (ch);
}

int
str_isdigit (const char *ch)
{
    return used_class.isdigit (ch);
}

int
str_toupper (const char *ch, char **out, size_t * remain)
{
    return used_class.toupper (ch, out, remain);
}

int
str_tolower (const char *ch, char **out, size_t * remain)
{
    return used_class.tolower (ch, out, remain);
}

int
str_isprint (const char *ch)
{
    return used_class.isprint (ch);
}

int
str_iscombiningmark (const char *ch)
{
    return used_class.iscombiningmark (ch);
}

const char *
str_trunc (const char *text, int width)
{
    return used_class.trunc (text, width);
}

char *
str_create_search_needle (const char *needle, int case_sen)
{
    return used_class.create_search_needle (needle, case_sen);
}


void
str_release_search_needle (char *needle, int case_sen)
{
    used_class.release_search_needle (needle, case_sen);
}

const char *
str_search_first (const char *text, const char *search, int case_sen)
{
    return used_class.search_first (text, search, case_sen);
}

const char *
str_search_last (const char *text, const char *search, int case_sen)
{
    return used_class.search_last (text, search, case_sen);
}

int
str_is_valid_string (const char *text)
{
    return used_class.is_valid_string (text);
}

int
str_compare (const char *t1, const char *t2)
{
    return used_class.compare (t1, t2);
}

int
str_ncompare (const char *t1, const char *t2)
{
    return used_class.ncompare (t1, t2);
}

int
str_casecmp (const char *t1, const char *t2)
{
    return used_class.casecmp (t1, t2);
}

int
str_ncasecmp (const char *t1, const char *t2)
{
    return used_class.ncasecmp (t1, t2);
}

int
str_prefix (const char *text, const char *prefix)
{
    return used_class.prefix (text, prefix);
}

int
str_caseprefix (const char *text, const char *prefix)
{
    return used_class.caseprefix (text, prefix);
}

void
str_fix_string (char *text)
{
    used_class.fix_string (text);
}

char *
str_create_key (const char *text, int case_sen)
{
    return used_class.create_key (text, case_sen);
}

char *
str_create_key_for_filename (const char *text, int case_sen)
{
    return used_class.create_key_for_filename (text, case_sen);
}

int
str_key_collate (const char *t1, const char *t2, int case_sen)
{
    return used_class.key_collate (t1, t2, case_sen);
}

void
str_release_key (char *key, int case_sen)
{
    used_class.release_key (key, case_sen);
}
