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
#include <iconv.h>
#include <glib.h>
#include <langinfo.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include "global.h"
#include "strutil.h"

//names, that are used for utf-8 
static const char *str_utf8_encodings[] = {
        "utf-8", 
        "utf8", 
        NULL};

// standard 8bit encodings, no wide or multibytes characters
static const char *str_8bit_encodings[] = {
        "iso-8859",
        "iso8859",
        NULL
};        

// terminal encoding
static char *codeset;
// function for encoding specific operations
static struct str_class used_class;
// linked list of string buffers
static struct str_buffer *buffer_list = NULL; 

iconv_t str_cnv_to_term;
iconv_t str_cnv_from_term;
iconv_t str_cnv_not_convert;

// if enc is same encoding like on terminal
static int 
str_test_not_convert (const char *enc)
{
    return g_ascii_strcasecmp (enc, codeset) == 0;
}        

str_conv_t
str_crt_conv_to (const char *to_enc)
{
    return (!str_test_not_convert (to_enc)) ? iconv_open (to_enc, codeset) : 
            str_cnv_not_convert;
}

str_conv_t
str_crt_conv_from (const char *from_enc)
{
    return (!str_test_not_convert (from_enc)) ? iconv_open (codeset, from_enc) :
            str_cnv_not_convert;
}

void
str_close_conv (str_conv_t conv)
{
    if (conv != str_cnv_not_convert)
        iconv_close (conv);
}

struct str_buffer *
str_get_buffer () 
{
    struct str_buffer *result;
    
    result = buffer_list;
    
    while (result != NULL) {
        if (!result->used) {
            str_reset_buffer (result);
            result->used = 1;
            return result;
        }
        result = result->next;
    }
    
    result = g_new (struct str_buffer, 1);
    result->size = BUF_TINY;
    result->data = g_new0 (char, result->size);
    result->data[0] = '\0';
    result->actual = result->data;
    result->remain = result->size;
    
    result->next = buffer_list;
    buffer_list = result;
    
    result->used = 1;
    
    return result;
}    

void
str_release_buffer (struct str_buffer *buffer)
{
    buffer->used = 0;
}

void
str_incrase_buffer (struct str_buffer *buffer) 
{
    size_t offset;
    
    offset = buffer->actual - buffer->data;
    buffer->remain+= buffer->size;
    buffer->size*= 2;
    buffer->data = g_renew (char, buffer->data, buffer->size);
    buffer->actual = buffer->data + offset;
}

void
str_reset_buffer (struct str_buffer *buffer)
{
    buffer->data[0] = '\0';
    buffer->actual = buffer->data;
    buffer->remain = buffer->size;
}

static int
_str_convert (str_conv_t coder, char *string, struct str_buffer *buffer)
{
    int state;        
    size_t left;
    size_t nconv;
                    
    errno = 0;
    
    if (used_class.is_valid_string (string)) {
        state = 0;
       
        left = strlen (string);

        if (coder == (iconv_t) (-1)) return ESTR_FAILURE;
        
        iconv(coder, NULL, NULL, NULL, NULL);

        while (((int)left) > 0) {
            nconv = iconv(coder, &string, &left, 
                          &(buffer->actual), &(buffer->remain));
            if (nconv == (size_t) (-1)) {
                switch (errno) {
                    case EINVAL:
                        return ESTR_FAILURE;
                    case EILSEQ:
                        string++;
                        left--;
                        if (buffer->remain <= 0) {
                            str_incrase_buffer (buffer);
                        }
                        buffer->actual[0] = '?';
                        buffer->actual++;
                        buffer->remain--;
                        state = ESTR_PROBLEM;   
                        break; 
                    case E2BIG:
                        str_incrase_buffer (buffer);
                        break;
                }
            }
        };

        return state;
    } else return ESTR_FAILURE;
}

int
str_convert (str_conv_t coder, char *string, struct str_buffer *buffer)
{
    int result; 
    
    result = _str_convert (coder, string, buffer);
    buffer->actual[0] = '\0';
    
    return result;
}

static int
_str_vfs_convert_from (str_conv_t coder, char *string, 
                       struct str_buffer *buffer)
{
    size_t left;
    size_t nconv;

    left = strlen (string);
        
    if (coder == (iconv_t) (-1)) return ESTR_FAILURE;
    
    iconv(coder, NULL, NULL, NULL, NULL);

    do {
        nconv = iconv(coder, &string, &left, 
                      &(buffer->actual), &(buffer->remain));
        if (nconv == (size_t) (-1)) {
            switch (errno) {
                case EINVAL:
                    return ESTR_FAILURE;
                case EILSEQ:
                    return ESTR_FAILURE;
                case E2BIG:
                    str_incrase_buffer (buffer);
                    break;
            }
        }
    } while (left > 0);
    
    return 0;
}

int
str_vfs_convert_from (str_conv_t coder, char *string, struct str_buffer *buffer)
{
    int result;
            
    if (coder == str_cnv_not_convert) {
        str_insert_string (string, buffer);
        result = 0;
    } else result = _str_vfs_convert_from (coder, string, buffer);
    buffer->actual[0] = '\0';
    
    return result;
}
        
int
str_vfs_convert_to (str_conv_t coder, const char *string, 
                    int size, struct str_buffer *buffer)
{
    return used_class.vfs_convert_to (coder, string, size, buffer);
}

void
str_insert_string (const char *string, struct str_buffer *buffer)
{
    size_t s;
    
    s = strlen (string);
    while (buffer->remain < s) str_incrase_buffer (buffer);
        
    memcpy (buffer->actual, string, s);
    buffer->actual+= s;
    buffer->remain-= s;
    buffer->actual[0] = '\0';
}

void
str_insert_string2 (const char *string, int size, struct str_buffer *buffer)
{
    size_t s;
    
    s = (size >= 0) ? size : strlen (string);
    while (buffer->remain < s) str_incrase_buffer (buffer);
        
    memcpy (buffer->actual, string, s);
    buffer->actual+= s;
    buffer->remain-= s;
    buffer->actual[0] = '\0';
}

void
str_printf (struct str_buffer *buffer, const char *format, ...)
{
    int size;
    va_list ap;
    
    va_start (ap, format);
    size = vsnprintf (buffer->actual, buffer->remain, format, ap);
    while (buffer->remain <= size) {
        str_incrase_buffer (buffer);
        size = vsnprintf (buffer->actual, buffer->remain, format, ap);
    }
    buffer->actual+= size;
    buffer->remain-= size;
    va_end (ap);
}

void
str_insert_char (char ch, struct str_buffer *buffer)
{
    if (buffer->remain <= 1) str_incrase_buffer (buffer);
        
    buffer->actual[0] = ch;
    buffer->actual++;
    buffer->remain--;
    buffer->actual[0] = '\0';
}

void
str_insert_replace_char (struct str_buffer *buffer) 
{
    used_class.insert_replace_char (buffer);
}

void
str_backward_buffer (struct str_buffer *buffer, int count)
{
    char *prev;
    
    while ((count > 0) && (buffer->actual > buffer->data)) {
        prev = str_get_prev_char (buffer->actual);
        buffer->remain+= buffer->actual - prev;
        buffer->actual = prev;
        buffer->actual[0] = '\0';
        count--;
    }
}
        

int 
str_translate_char (str_conv_t conv, char *keys, size_t ch_size, 
                    char *output, size_t out_size)
{
    size_t left;
    size_t cnv;
    
    iconv (conv, NULL, NULL, NULL, NULL);
    
    left = (ch_size == (size_t)(-1)) ? strlen (keys) : ch_size;
    
    cnv = iconv (conv, &keys, &left, &output, &out_size);
    if (cnv == (size_t)(-1)) {
        if (errno == EINVAL) return ESTR_PROBLEM; else return ESTR_FAILURE;
    } else {
        output[0] = '\0';
        return 0;
    }
}


static const char *
str_detect_termencoding ()
{
    return (nl_langinfo(CODESET));
}

static int
str_test_encoding_class (const char *encoding, const char **table)
{
    int t;
    int result = 0;
    
    for (t = 0; table[t] != NULL; t++) {
        result+= (g_ascii_strncasecmp (encoding, table[t], 
                  strlen (table[t])) == 0); 
    }
    
    return result;
}

static void
str_choose_str_functions ()
{
    if (str_test_encoding_class (codeset, str_utf8_encodings)) {
        used_class = str_utf8_init ();
    } else if (str_test_encoding_class (codeset, str_8bit_encodings)) {
        used_class = str_8bit_init ();
    } else {
        used_class = str_ascii_init ();
    }
}        

void 
str_init_strings (const char *termenc)
{
    codeset = g_strdup ((termenc != NULL) 
                        ? termenc 
                        : str_detect_termencoding ());

    str_cnv_not_convert = iconv_open (codeset, codeset);
    if (str_cnv_not_convert == INVALID_CONV) {
        if (termenc != NULL) {
            g_free (codeset);
            codeset = g_strdup (str_detect_termencoding ());
            str_cnv_not_convert = iconv_open (codeset, codeset);
        }
    
        if (str_cnv_not_convert == INVALID_CONV) {
            g_free (codeset);
            codeset = g_strdup ("ascii");
            str_cnv_not_convert = iconv_open (codeset, codeset);
        }
    }
    
    str_cnv_to_term = str_cnv_not_convert;
    str_cnv_from_term = str_cnv_not_convert;
    
    str_choose_str_functions ();
}

static void
str_release_buffer_list ()
{
    struct str_buffer *buffer;
    struct str_buffer *next;
    
    buffer = buffer_list;
    while (buffer != NULL) {
        next = buffer->next;
        g_free (buffer->data);
        g_free (buffer);
        buffer = next;
    }
}            
    
void 
str_uninit_strings ()
{
    str_release_buffer_list ();
    
    iconv_close (str_cnv_not_convert);
}
    
const char *
str_term_form (const char *text)
{
    return used_class.term_form (text);
}

const char *
str_fit_to_term (const char *text, int width, int just_mode)
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
    return used_class.msg_term_size (text, lines, columns);
}

const char *
str_term_substring (const char *text, int start, int width)
{
    return used_class.term_substring (text, start, width);
}

char *
str_get_next_char (char *text)
{
    
    used_class.cnext_char ((const char **)&text);
    return text;
}

const char *
str_cget_next_char (const char *text)
{
    used_class.cnext_char (&text);
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
str_offset_to_pos (const char* text, size_t length)
{
    return used_class.offset_to_pos (text, length);
}

int 
str_length (const char* text)
{
    return used_class.length (text);
}

int 
str_length2 (const char* text, int size)
{
    return used_class.length2 (text, size);
}

int 
str_length_noncomb (const char* text)
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
str_toupper (const char *ch, char **out, size_t *remain)
{
    return used_class.toupper (ch, out, remain);
}

int
str_tolower (const char *ch, char **out, size_t *remain)
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

