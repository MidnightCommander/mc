/*
   Text conversion from one charset to another.

   Copyright (C) 2001-2026
   Free Software Foundation, Inc.

   Written by:
   Walery Studennikov <despair@sama.ru>

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

/** \file charsets.c
 *  \brief Source: Text conversion from one charset to another
 */

#include <config.h>

#include <limits.h>  // MB_LEN_MAX
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/global.h"
#include "lib/strutil.h"  // utf-8 functions
#include "lib/fileloc.h"
#include "lib/util.h"  // whitespace()

#include "lib/charsets.h"

/*** global variables ****************************************************************************/

GPtrArray *codepages = NULL;

const char *cp_display = NULL;
const char *cp_source = NULL;

/*** file scope macro definitions ****************************************************************/

#define UNKNCHAR '\001'

/*** file scope type declarations ****************************************************************/

typedef struct
{
    char *id;
    char *name;
} codepage_desc;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static const char NON_PRINTABLE_CHAR = '.';

static const char NO_TRANSLATION[] = N_ ("No translation");

static unsigned char conv_display_table[256];
static unsigned char conv_input_table[256];

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static codepage_desc *
new_codepage_desc (const char *id, const char *name)
{
    codepage_desc *desc;

    desc = g_new (codepage_desc, 1);
    desc->id = g_strdup (id);
    desc->name = g_strdup (name);

    return desc;
}

/* --------------------------------------------------------------------------------------------- */

static void
free_codepage_desc (gpointer data)
{
    codepage_desc *desc = (codepage_desc *) data;

    g_free (desc->id);
    g_free (desc->name);
    g_free (desc);
}

/* --------------------------------------------------------------------------------------------- */
/* returns display codepage */

static void
load_codepages_list_from_file (GPtrArray **list, const char *fname)
{
    FILE *f;
    char buf[BUF_MEDIUM];
    char *default_codepage = NULL;

    f = fopen (fname, "r");
    if (f == NULL)
        return;

    while (fgets (buf, sizeof buf, f) != NULL)
    {
        // split string into id and cpname
        char *p = buf;
        size_t buflen;

        if (*p == '\n' || *p == '\0' || *p == '#')
            continue;

        buflen = strlen (buf);

        if (buflen != 0 && buf[buflen - 1] == '\n')
            buf[buflen - 1] = '\0';
        while (*p != '\0' && !whitespace (*p))
            ++p;
        if (*p == '\0')
            goto fail;

        *p++ = '\0';
        g_strstrip (p);
        if (*p == '\0')
            goto fail;

        if (strcmp (buf, "default") == 0)
            default_codepage = g_strdup (p);
        else
        {
            const char *id = buf;

            if (*list == NULL)
            {
                *list = g_ptr_array_new_full (16, free_codepage_desc);
                g_ptr_array_add (*list, new_codepage_desc (id, p));
            }
            else
            {
                unsigned int i;

                // whether id is already present in list
                // if yes, overwrite description
                for (i = 0; i < (*list)->len; i++)
                {
                    codepage_desc *desc;

                    desc = (codepage_desc *) g_ptr_array_index (*list, i);

                    if (strcmp (id, desc->id) == 0)
                    {
                        // found
                        g_free (desc->name);
                        desc->name = g_strdup (p);
                        break;
                    }
                }

                // not found
                if (i == (*list)->len)
                    g_ptr_array_add (*list, new_codepage_desc (id, p));
            }
        }
    }

    if (default_codepage != NULL)
    {
        mc_global.display_codepage = get_codepage_index (default_codepage);
        g_free (default_codepage);
    }

fail:
    fclose (f);
}

/* --------------------------------------------------------------------------------------------- */

static char
translate_character (GIConv cd, char c)
{
    gchar *tmp_buff = NULL;
    gsize bytes_read, bytes_written = 0;
    const char *ibuf = &c;
    char ch = UNKNCHAR;
    int ibuflen = 1;

    tmp_buff = g_convert_with_iconv (ibuf, ibuflen, cd, &bytes_read, &bytes_written, NULL);
    if (tmp_buff != NULL)
        ch = tmp_buff[0];
    g_free (tmp_buff);
    return ch;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
load_codepages_list (void)
{
    char *fname;

    // 1: try load /usr/share/mc/mc.charsets
    fname = g_build_filename (mc_global.share_data_dir, CHARSETS_LIST, (char *) NULL);
    load_codepages_list_from_file (&codepages, fname);
    g_free (fname);

    // 2: try load /etc/mc/mc.charsets
    fname = g_build_filename (mc_global.sysconfig_dir, CHARSETS_LIST, (char *) NULL);
    load_codepages_list_from_file (&codepages, fname);
    g_free (fname);

    if (codepages == NULL)
    {
        // files are not found, add default codepage
        fprintf (stderr, "%s\n", _ ("Warning: cannot load codepages list"));

        codepages = g_ptr_array_new_with_free_func (free_codepage_desc);
        g_ptr_array_add (codepages, new_codepage_desc (DEFAULT_CHARSET, _ ("7-bit ASCII")));
    }
}

/* --------------------------------------------------------------------------------------------- */

void
free_codepages_list (void)
{
    g_ptr_array_free (codepages, TRUE);
    // NULL-ize pointer to make unit tests happy
    codepages = NULL;
}

/* --------------------------------------------------------------------------------------------- */

const char *
get_codepage_id (const int n)
{
    return (n < 0) ? NO_TRANSLATION : ((codepage_desc *) g_ptr_array_index (codepages, n))->id;
}

/* --------------------------------------------------------------------------------------------- */

const char *
get_codepage_name (const int n)
{
    return (n < 0) ? _ (NO_TRANSLATION)
                   : ((codepage_desc *) g_ptr_array_index (codepages, n))->name;
}

/* --------------------------------------------------------------------------------------------- */

int
get_codepage_index (const char *id)
{
    if (codepages == NULL)
        return -1;
    if (strcmp (id, NO_TRANSLATION) == 0)
        return -1;
    for (guint i = 0; i < codepages->len; i++)
        if (strcmp (id, get_codepage_id (i)) == 0)
            return (int) i;
    return -1;
}

/* --------------------------------------------------------------------------------------------- */
/** Check if specified encoding can be used in mc.
 * @param encoding name of encoding
 * @return TRUE if encoding is supported by mc, FALSE otherwise
 */

gboolean
is_supported_encoding (const char *encoding)
{
    GIConv coder;
    gboolean result;

    if (encoding == NULL)
        return FALSE;

    coder = str_crt_conv_from (encoding);
    result = coder != INVALID_CONV;
    if (result)
        str_close_conv (coder);
    return result;
}

/* --------------------------------------------------------------------------------------------- */

char *
init_translation_table (int cpsource, int cpdisplay)
{
    int i;
    GIConv cd;

    // Fill input <-> display tables

    if (cpsource < 0 || cpdisplay < 0 || cpsource == cpdisplay)
    {
        for (i = 0; i <= 255; ++i)
        {
            conv_display_table[i] = i;
            conv_input_table[i] = i;
        }
        cp_source = cp_display;
        return NULL;
    }

    for (i = 0; i <= 127; ++i)
    {
        conv_display_table[i] = i;
        conv_input_table[i] = i;
    }

    cp_source = get_codepage_id (cpsource);
    cp_display = get_codepage_id (cpdisplay);

    // display <- input table

    cd = g_iconv_open (cp_display, cp_source);
    if (cd == INVALID_CONV)
        return g_strdup_printf (_ ("Cannot translate from %s to %s"), cp_source, cp_display);

    for (i = 128; i <= 255; ++i)
        conv_display_table[i] = translate_character (cd, i);

    g_iconv_close (cd);

    // input <- display table

    cd = g_iconv_open (cp_source, cp_display);
    if (cd == INVALID_CONV)
        return g_strdup_printf (_ ("Cannot translate from %s to %s"), cp_display, cp_source);

    for (i = 128; i <= 255; ++i)
    {
        unsigned char ch;
        ch = translate_character (cd, i);
        conv_input_table[i] = (ch == UNKNCHAR) ? i : ch;
    }

    g_iconv_close (cd);

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

int
convert_8bit_to_display (const int c)
{
    return (c < 0 || c >= 256 ? c : (int) conv_display_table[c]);
}

/* --------------------------------------------------------------------------------------------- */

int
convert_8bit_from_input (const int c)
{
    return (c < 0 || c >= 256 ? c : (int) conv_input_table[c]);
}

/* --------------------------------------------------------------------------------------------- */

GString *
str_nconvert_to_display (const char *str, int len)
{
    GString *buff;
    GIConv conv;

    if (str == NULL)
        return NULL;

    if (cp_display == cp_source)
        return g_string_new (str);

    conv = str_crt_conv_from (cp_source);
    if (conv == INVALID_CONV)
        return g_string_new (str);

    buff = g_string_new ("");
    str_nconvert (conv, str, len, buff);
    str_close_conv (conv);
    return buff;
}

/* --------------------------------------------------------------------------------------------- */

GString *
str_nconvert_to_input (const char *str, int len)
{
    GString *buff;
    GIConv conv;

    if (str == NULL)
        return NULL;

    if (cp_display == cp_source)
        return g_string_new (str);

    conv = str_crt_conv_to (cp_source);
    if (conv == INVALID_CONV)
        return g_string_new (str);

    buff = g_string_new ("");
    str_nconvert (conv, str, len, buff);
    str_close_conv (conv);
    return buff;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Convert utf-8 character to 8-bit one in the input (source) codepage.
 *
 * @param str utf-8 character
 *
 * @return 8-bit character
 */
unsigned char
convert_utf8_to_input (const char *str)
{
    unsigned char buf_ch[MB_LEN_MAX + 1];
    unsigned char ch = NON_PRINTABLE_CHAR;
    GIConv conv;

    if (str == NULL)
        return NON_PRINTABLE_CHAR;

    const char *cp_to = get_codepage_id (mc_global.source_codepage);

    conv = str_crt_conv_to (cp_to);
    if (conv != INVALID_CONV)
    {
        switch (str_translate_char (conv, str, -1, (char *) buf_ch, sizeof (buf_ch)))
        {
        case ESTR_SUCCESS:
            ch = buf_ch[0];
            break;
        default:
            ch = NON_PRINTABLE_CHAR;
            break;
        }

        str_close_conv (conv);
    }

    return ch;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Convert Unicode character to 8-bit one using specified coder
 *
 * @param c Unicode character
 * @param conv conversion descriptor
 *
 * @return 8-bit character
 */
unsigned char
convert_unichar_to_8bit (const gunichar c, GIConv conv)
{
    unsigned char str[MB_LEN_MAX + 1];
    unsigned char buf_ch[MB_LEN_MAX + 1];
    const int res = g_unichar_to_utf8 (c, (char *) str);

    if (res == 0)
        return NON_PRINTABLE_CHAR;

    str[res] = '\0';

    switch (str_translate_char (conv, (char *) str, -1, (char *) buf_ch, sizeof (buf_ch)))
    {
    case ESTR_SUCCESS:
        return buf_ch[0];
    default:
        return NON_PRINTABLE_CHAR;
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Convert 8-bit character to Unicode one using specified coder
 *
 * @param c 8-bit character
 * @param conv conversion descriptor
 *
 * @return Unicode character
 */
gunichar
convert_8bit_to_unichar (const char c, GIConv conv)
{
    unsigned char str[2];
    unsigned char buf_ch[MB_LEN_MAX + 1];

    str[0] = (const unsigned char) c;
    str[1] = '\0';

    switch (str_translate_char (conv, (char *) str, -1, (char *) buf_ch, sizeof (buf_ch)))
    {
    case ESTR_SUCCESS:
    {
        const gunichar res = g_utf8_get_char_validated ((char *) buf_ch, -1);

        return (res == (gunichar) (-1) || res == (gunichar) (-2) ? buf_ch[0] : res);
    }
    default:
        return NON_PRINTABLE_CHAR;
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Convert 8-bit character to Unicode one in the input (source) codepage.
 *
 * @param c 8-bit character
 *
 * @return Unicode character
 */
gunichar
convert_8bit_to_input_unichar (const char c)
{
    gunichar uni = NON_PRINTABLE_CHAR;
    GIConv conv;
    const char *cp_to = get_codepage_id (mc_global.source_codepage);

    conv = str_crt_conv_to (cp_to);
    if (conv != INVALID_CONV)
    {
        uni = convert_8bit_to_unichar (c, conv);
        str_close_conv (conv);
    }

    return uni;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
charset_conv_init (charset_conv_t *conv, get_byte_fn get_byte, get_utf8_fn get_utf8)
{
    conv->utf8 = TRUE;
    conv->conv = INVALID_CONV;
    conv->get_byte = get_byte;
    conv->get_utf8 = get_utf8;

    return codepage_change_conv (&conv->conv, &conv->utf8);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Convert one 8-bit or UTF-8 character to display codepage.
 *
 * @param conv charset conversion handler
 * @param from buffer where read character from
 * @param pos character offset in @from
 */
void
convert_char_to_display (charset_conv_t *conv, void *from, const off_t pos)
{
    int c;

    if (conv->utf8)
    {
        conv->len = 0;
        c = conv->get_utf8 (from, pos, &conv->len);
    }
    else
    {
        conv->len = 1;
        c = conv->get_byte (from, pos);
    }

    conv->wide = FALSE;

    if (c == -1)
    {
        // End of buffer
        conv->ch = -1;
        conv->len = 0;
        conv->printable = FALSE;
    }
    else if (mc_global.utf8_display)
    {
        if (conv->utf8)
        {
            conv->ch = c;
            conv->wide = g_unichar_iswide (c);
        }
        else
            conv->ch = convert_8bit_to_unichar ((unsigned char) c, conv->conv);

        conv->printable = g_unichar_isprint (conv->ch);
    }
    else
    {
        if (conv->utf8)
            conv->ch = convert_unichar_to_8bit (c, conv->conv);
        else
            conv->ch = convert_8bit_to_display (c);

        conv->printable = is_printable (conv->ch);
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Change codepage converter in accordance with display codepage.
 *
 * @param converter codepage converter
 * @param utf8 TRUE if display codepage is UTF-8, FALSE otherwise
 *
 * @return TRUE codepage converter was created successfully, FALSE otherwise.
 *         If FALSE is returned, @converter and @utf8 are unchanged.
 */
gboolean
codepage_change_conv (GIConv *converter, gboolean *utf8)
{
    const int cp_from =
        mc_global.source_codepage >= 0 ? mc_global.source_codepage : mc_global.display_codepage;
    const char *cp_id = get_codepage_id (cp_from);
    GIConv conv;

    if (cp_id == NULL)
        return FALSE;

    conv = str_crt_conv_from (cp_id);
    if (conv != INVALID_CONV)
    {
        if (*converter != str_cnv_from_term)
            str_close_conv (*converter);
        *converter = conv;
    }

    *utf8 = str_isutf8 (cp_id);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
