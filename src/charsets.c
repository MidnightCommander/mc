/* Text conversion from one charset to another.

   Copyright (C) 2001 Walery Studennikov <despair@sama.ru>

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

/** \file charsets.c
 *  \brief Source: Text conversion from one charset to another
 */

#include <config.h>

#ifdef HAVE_CHARSET

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/global.h"
#include "charsets.h"
#include "strutil.h"		/* utf-8 functions */
#include "main.h"
#include "util.h"		/* concat_dir_and_file() */
#include "lib/fileloc.h"

int n_codepages = 0;

struct codepage_desc *codepages;

unsigned char conv_displ[256];
unsigned char conv_input[256];

const char *cp_display = NULL;
const char *cp_source = NULL;


int
load_codepages_list (void)
{
    int result = -1;
    FILE *f;
    char *fname;
    char buf[BUF_MEDIUM];
    char *default_codepage = NULL;

    fname = concat_dir_and_file (mc_home, CHARSETS_INDEX);
    if (!(f = fopen (fname, "r"))) {
	fprintf (stderr, _("Warning: file %s not found\n"), fname);
	g_free (fname);

	fname = concat_dir_and_file (mc_home_alt, CHARSETS_INDEX);
	if (!(f = fopen (fname, "r"))) {
	    fprintf (stderr, _("Warning: file %s not found\n"), fname);
	    g_free (fname);

	    /* file is not found, add defaullt codepage */
	    n_codepages = 1;
	    codepages = g_new0 (struct codepage_desc, n_codepages + 1);
	    codepages[0].id = g_strdup ("ASCII");
	    codepages[0].name = g_strdup (_("7-bit ASCII"));
	    return n_codepages;
	}
    }
    g_free (fname);

    for (n_codepages = 0; fgets (buf, sizeof (buf), f);)
	if (buf[0] != '\n' && buf[0] != '\0' && buf[0] != '#')
	    ++n_codepages;
    rewind (f);

    codepages = g_new0 (struct codepage_desc, n_codepages + 1);

    for (n_codepages = 0; fgets (buf, sizeof buf, f);) {
	/* split string into id and cpname */
	char *p = buf;
	size_t buflen = strlen (buf);

	if (*p == '\n' || *p == '\0' || *p == '#')
	    continue;

	if (buflen > 0 && buf[buflen - 1] == '\n')
	    buf[buflen - 1] = '\0';
	while (*p != '\t' && *p != ' ' && *p != '\0')
	    ++p;
	if (*p == '\0')
	    goto fail;

	*p++ = '\0';
	g_strstrip (p);
	if (*p == '\0')
	    goto fail;

	if (strcmp (buf, "default") == 0)
	    default_codepage = g_strdup (p);
	else {
	    codepages[n_codepages].id = g_strdup (buf);
	    codepages[n_codepages].name = g_strdup (p);
	    ++n_codepages;
	}
    }

    if (default_codepage != NULL) {
	display_codepage = get_codepage_index (default_codepage);
	g_free (default_codepage);
    }

    result = n_codepages;
  fail:
    fclose (f);
    return result;
}

void
free_codepages_list (void)
{
    if (n_codepages > 0) {
	int i;
	for (i = 0; i < n_codepages; i++) {
	    g_free (codepages[i].id);
	    g_free (codepages[i].name);
	}
	n_codepages = 0;
	g_free (codepages);
	codepages = 0;
    }
}

#define OTHER_8BIT "Other_8_bit"

const char *
get_codepage_id (const int n)
{
    return (n < 0) ? OTHER_8BIT : codepages[n].id;
}

int
get_codepage_index (const char *id)
{
    int i;
    if (strcmp (id, OTHER_8BIT) == 0)
	return -1;
    if (codepages == NULL)
	return -1;
    for (i = 0; i < n_codepages; i++)
	if (strcmp (id, codepages[i].id) == 0)
	    return i;
    return -1;
}

static char
translate_character (GIConv cd, char c)
{
    gchar *tmp_buff = NULL;
    gsize bytes_read, bytes_written = 0;
    const char *ibuf = &c;
    char ch = UNKNCHAR;

    int ibuflen = 1;

    tmp_buff = g_convert_with_iconv (ibuf, ibuflen, cd, &bytes_read, &bytes_written, NULL);
    if ( tmp_buff )
        ch = tmp_buff[0];
    g_free (tmp_buff);
    return ch;
}

char errbuf[255];

/*
 * FIXME: This assumes that ASCII is always the first encoding
 * in mc.charsets
 */
#define CP_ASCII 0

const char *
init_translation_table (int cpsource, int cpdisplay)
{
    int i;
    GIConv cd;

    /* Fill inpit <-> display tables */

    if (cpsource < 0 || cpdisplay < 0 || cpsource == cpdisplay) {
	for (i = 0; i <= 255; ++i) {
	    conv_displ[i] = i;
	    conv_input[i] = i;
	    cp_source = cp_display;
	}
	return NULL;
    }

    for (i = 0; i <= 127; ++i) {
	conv_displ[i] = i;
	conv_input[i] = i;
    }
    cp_source = (char *) codepages[cpsource].id;
    cp_display = (char *) codepages[cpdisplay].id;

    /* display <- inpit table */

    cd = g_iconv_open (cp_display, cp_source);
    if (cd == INVALID_CONV) {
	g_snprintf (errbuf, sizeof (errbuf),
		    _("Cannot translate from %s to %s"), cp_source, cp_display);
	return errbuf;
    }

    for (i = 128; i <= 255; ++i)
	conv_displ[i] = translate_character (cd, i);

    g_iconv_close (cd);

    /* inpit <- display table */

    cd = g_iconv_open (cp_source, cp_display);
    if (cd == INVALID_CONV) {
	g_snprintf (errbuf, sizeof (errbuf),
		    _("Cannot translate from %s to %s"), cp_display, cp_source);
	return errbuf;
    }

    for (i = 128; i <= 255; ++i) {
	unsigned char ch;
	ch = translate_character (cd, i);
	conv_input[i] = (ch == UNKNCHAR) ? i : ch;
    }

    g_iconv_close (cd);

    return NULL;
}

void
convert_to_display (char *str)
{
    if (!str)
	return;

    while (*str) {
	*str = conv_displ[(unsigned char) *str];
	str++;
    }
}

GString *
str_convert_to_display (char *str)
{
    return str_nconvert_to_display (str, -1);

}

GString *
str_nconvert_to_display (char *str, int len)
{
    GString *buff;
    GIConv conv;

    if (!str)
	return g_string_new("");

    if (cp_display == cp_source)
	return g_string_new(str);

    conv = str_crt_conv_from (cp_source);

    buff = g_string_new("");
    str_nconvert (conv, str, len, buff);
    str_close_conv (conv);
    return buff;
}

void
convert_from_input (char *str)
{
    if (!str)
	return;

    while (*str) {
	*str = conv_input[(unsigned char) *str];
	str++;
    }
}

GString *
str_convert_to_input (char *str)
{
    return str_nconvert_to_input (str, -1);
}

GString *
str_nconvert_to_input (char *str, int len)
{
    GString *buff;
    GIConv conv;

    if (!str)
	return g_string_new("");

    if (cp_display == cp_source)
	return g_string_new(str);

    conv = str_crt_conv_to (cp_source);

    buff = g_string_new("");
    str_nconvert (conv, str, len, buff);
    str_close_conv (conv);
    return buff;
}

unsigned char
convert_from_utf_to_current (const char *str)
{
    unsigned char buf_ch[6 + 1];
    unsigned char ch = '.';
    GIConv conv;
    const char *cp_to;

    if (!str)
        return '.';

    cp_to =  get_codepage_id ( source_codepage );
    conv = str_crt_conv_to ( cp_to );

    if (conv != INVALID_CONV) {
        switch (str_translate_char (conv, str, -1, (char *)buf_ch, sizeof(buf_ch))) {
        case ESTR_SUCCESS:
            ch = buf_ch[0];
            break;
        case ESTR_PROBLEM:
        case ESTR_FAILURE:
            ch = '.';
            break;
        }
        str_close_conv (conv);
    }

    return ch;

}

unsigned char
convert_from_utf_to_current_c (const int input_char, GIConv conv)
{
    unsigned char str[6 + 1];
    unsigned char buf_ch[6 + 1];
    unsigned char ch = '.';

    int res = 0;

    res = g_unichar_to_utf8 (input_char, (char *)str);
    if ( res == 0 ) {
        return ch;
    }
    str[res] = '\0';

    switch (str_translate_char (conv, (char *)str, -1, (char *)buf_ch, sizeof(buf_ch))) {
    case ESTR_SUCCESS:
        ch = buf_ch[0];
        break;
    case ESTR_PROBLEM:
    case ESTR_FAILURE:
        ch = '.';
        break;
    }
    return ch;
}

int
convert_from_8bit_to_utf_c (const char input_char, GIConv conv)
{
    unsigned char str[2];
    unsigned char buf_ch[6 + 1];
    int ch = '.';
    int res = 0;

    str[0] = (unsigned char) input_char;
    str[1] = '\0';

    switch (str_translate_char (conv, (char *)str, -1, (char *)buf_ch, sizeof(buf_ch))) {
    case ESTR_SUCCESS:
        res = g_utf8_get_char_validated ((char *)buf_ch, -1);
        if ( res < 0 ) {
            ch = buf_ch[0];
        } else {
            ch = res;
        }
        break;
    case ESTR_PROBLEM:
    case ESTR_FAILURE:
        ch = '.';
        break;
    }
    return ch;
}

int
convert_from_8bit_to_utf_c2 (const char input_char)
{
    unsigned char str[2];
    unsigned char buf_ch[6 + 1];
    int ch = '.';
    int res = 0;
    GIConv conv;
    const char *cp_from;

    str[0] = (unsigned char) input_char;
    str[1] = '\0';

    cp_from = get_codepage_id ( source_codepage );
    conv = str_crt_conv_to (cp_from);

    if (conv != INVALID_CONV) {
        switch (str_translate_char (conv, (char *) str, -1, (char *) buf_ch, sizeof(buf_ch))) {
        case ESTR_SUCCESS:
            res = g_utf8_get_char_validated ((char *) buf_ch, -1);
            if ( res < 0 ) {
                ch = buf_ch[0];
            } else {
                ch = res;
            }
            break;
        case ESTR_PROBLEM:
        case ESTR_FAILURE:
            ch = '.';
            break;
        }
        str_close_conv (conv);
    }
    return ch;

}
#endif				/* HAVE_CHARSET */
